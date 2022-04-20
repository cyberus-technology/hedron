Redesigning the Hedron Host-Interrupt Subsystem

# Goal

The host interrupt subsystem of Hedron has not been changed within the last decade.
While it has been proven to be stable and reliable, we have reached its design limits
in many areas. This becomes more and more of an issue for SVP-WS and Particle.

The goal if this document is to propose an alternative mechanism for host-interrupts
which improves interrupt handling in our hedron-userland and, as such, in our products.

# Status Quo

## Host Interrupt Representation for User Space

Interrupts in Hedron are mapped to semaphore kernel objects which can be delegated to
user space via the Roottask. There always is a 1:1 relationship between an interrupt and
a semaphore object, i.e. a specific semaphore can represent exactly one interrupt
at a given time.

A user space thread can wait for an interrupt by blocking on the corresponding semaphore
object. When the associated interrupt arrives, Hedron will signal the semaphore object
effectively unblocking the waiting thread. No other information is transferred to user space,
thus, userspace must know which interrupt is assigned to which semaphore.

## PIN-based Interrupts

PIN-based interrupts are those which are delivered by either I/O APIC or PIC. In the case
of Hedron only I/O APIC interrupts are exposed to userspace which is not an issue because
every device connected to the PIC is also connected to the I/O APIC. The actual PIN might be
different though.

The location of the corresponding interrupt semaphores in the kernel PD is provided by the
Hedron documentation. More precisely, they are located at (n, n + NUM\_APIC\_INTERRUPTS) where
n is the total number of idle scheduling contexts (one per CPU, see documentation 6.1).
Userspace must also probe for NUM\_APIC\_INTERRUPTS which can be calculated from HIP
information.

### MSIs

MSIs are interrupts that devices can issue directly onto the memory bus where the corresponding
LAPIC can pick them up. They are configured via an address/data pair containing information
about the desination CPU, trigger mode and a interrupt vector.

For MSIs, any interrupt semaphore that is not already claimed by an IO-APIC PIN can be used.
This also means that the number of available MSI vectors depends on the number and configuration
of available IO-APIC devices in the system. E.g. when a system has an IO-APIC with 120 PINs,
only 72 interrupt semaphores can be used for MSIs in total.

### Edge vs. Level Triggered Interrupts

While MSIs can only be edge-triggered, legacy interrupts can also be
level-triggered.

Level-triggered interrupts are different in the sense that the interrupt line
stays asserted as long as the device considers the underlying condition
that triggered the interrupt to be asserted to be present. The
interrupt line will only be de-asserted when the device driver had a
chance to run and clear the condition.

A monolithic kernel can solve this by running the device driver in the
interrupt service routine and acknowledging the interrupt when the
driver completed handling the condition.

In a microkernel context, where the device driver does not run in the
kernel, this creates a problem, because the device interrupt needs to
be "silenced" until the driver had a chance to run. Hedron solves this
issue by masking the interrupt at the interrupt controller (IOAPIC)
until userspace starts to wait for the interrupt again.

### Kernel Implementation Details

Whenever an interrupt arrives in Hedron, an EOI is sent to the local APIC and then
the corresponding interrupt semaphore object is signaled.

In case of a level-triggered interrupt, it is masked by Hedron via the
corresponding I/O APIC RTE before signaling the interrupt semaphore. It is unmasked
as soon as a userspace thread waits on the interrupt semaphore again.

## Using Interrupt Semaphores

The current workflow is as follows:

- Delegate a interrupt semaphore from the kernel PD into a user PD (only roottask can do this)
- If the interrupt is connected to the IO-APIC, the correct semaphore must be delegated as they
  are configured during boot by the hypervisor (discoverable via HIP)
- Call `asign_gsi` with the semaphore to correctly configure the IOMMU
- Use a thread per interrupt semaphore to detect interrupt arrival in userspace


## Issues with the Current Mechanism

- Limited to 256 interrupts for user space (actually 192)
- rigid configuration for IO-APIC interrupts
- one thread per interrupt semaphore -> lots of resources required
- irq migration requires thread migration (or having NUM\_CPU * NUM\_IRQ threads)
- bad compatibility with posted interrupts

# Proposal for a New Host-Interrupt Subsystem

This section requires knowledge about KPages. If you don't know what KPages are
please read this write-up [1].

## Goals

The goal of the new host-interrupt subsystem is to solve the issues the current mechansim has
while keeping the current functionality intact.

## Key Changes

The new mechanism allows for up to 192 interrupts per logical CPU core.
Furthermore, it enables easy interrupt migration by allowing to assign an interrupt
to a different core without doing a costly and complex thread migration.

Like before, interrupts will still be delivered via semaphores. The way they works will
be fundamentally different though.

Instead of having special interrupt-semaphores, userspace can now freely use any semaphore
as interrupt delivery object. It is also possible to use the same interrupt delivery semaphore
for multiple interrupts.

Of course, this means that userspace must have a way to figure out
which interrupt signaled the semaphore. This is why each semaphore object that represents
and interrupt has an associated KPage holding a bitmap of 192 bits. Whenever an interrupt
triggers, the hypervisor will set a specific bit in this bitmap before signaling the
associated semaphore. The bit which is set can be configured from userspace.

## Transition from Old to New Mechanism

The new kernel API will be a breaking change. It is however possible to adopt the current GSI userland
subsystem to use the new mechanism without redesigning it, i.e. using a KPage/Semaphore pair for each
host interrupt exclusively. Doing so will not enable any of the benefits of the new mechanism
but can be done with very little effort.

Once we have a complete hedron implementation and adopted the userland as stated above, we can
redesign the GSI subsystem to benefit from the new mechanism.

## Host Interrupt Representation for User Space

- Each interrupt is represented by a (semaphore, vector, kpage) triplet
- When the semaphore is signaled, userspace can check the associated KPage
  to identify the which interrupt has arrived
- Userspace must know the mapping of KPage[vector] to I/O APIC PIN or MSI and do the right thing
- Userspace must actively de-assert level triggered interrupts via a system call

## Using the new mechanism

The new workflow will be as follows:

- Create a new semaphore (or use an already existing semaphore if applicable)
- Create a new KPage (or use an already existing one if applicable)
- Chose a vector inside the KPage
- Configure the interrupt via `assign_msi` or `assign_gsi`
- Use a thread to detect interrupt arrival in userspace

## Solving the existing issues

### Issue 1: Limited to 256 interrupts for user space (actually 192)

Instead of maintaining a global IDT in the kernel, the new mechanism allows an implementation where we
can use per-core IDTs. In theory, this would allow use to use 256 interrupt vectors per code.
Since we still need a few interrupt vectors for the kernel, the number of vectors user space can
use per-core is currently limited to 192. This number is subject for discussion as we chose it arbitrary.

### Issue 2: rigid configuration for IO-APIC interrupts

With the current mechanism, the kernel reserves an interrupt semaphore per IO-APIC pin, even when no device is
connected to said pin. On systems with lots of IO-APICs (e.g. the SR630) and systems that come with
a modern IO-APIC with 120 PINs, this severely limits the number of MSIs user space can use because only
192 interrupt semaphore objects exist system-wide.

Using the new mechanism, user space can freely chose which IO-APIC PINs should be enabled and which shouldn't,
leaving more room for MSIs.

### Issue 3: one thread per interrupt semaphore -> lots of resources required

Using the current mechanism, the kernel reserves a fixed number of interrupt semaphore objects where each semaphore
represents exactly one interrupt. Since there is no way for user space to block on multiple semaphores at the
same time, the only possible implementation is to use one thread per interrupt.

With the new mechanism, multiple interrupts can use the same KPage/semaphore pair. Thus, an implementation
where we only use a single thread per-core becomes possible, effectively reducing the number of threads
in the system and sparing resources.

### Issue 4: irq migration requires thread migration (or having NUM\_CPU * NUM\_IRQ threads)

This issue is directly connected to issue 3, where the main problem is that we have exactly one interrupt
semaphore per host interrupt. Additionally, Hedron ECs are permanently bound to a specific CPU and cannot
migrate. Although it is currently possible to re-configure the receiving core via Hedrons `assign_gsi`
system call, we cannot move the EC to another core. This means that we will always have 2 additional
IPIs, one to wakeup the remote thread on a different CPU, and one to recall the receiving CPU once we
processed the IRQ though our interrupt subsystem.

As already stated in issue 3, the new mechanism allows to use the same KPage/semaphore pair for multiple
interrupts. This means that we can have a single interrupt thread per logical core, and interrupts
can freely migrate between these threads. When user land wants to migrate an interrupt from one core
to another, it simply needs to call `assign_gsi` or `assign_msi` again with the KPage/semaphore pair
that corresponds to the interrupt thread on the desired core.


### Issue 5: bad compatibility with posted interrupts

Posted interrupts are a hardware feature to accelerate interrupt delivery from physical devices
in a virtualized system. If you don't know how posted interrupts work, please refer to the Intel
SDM __and__ the VT-d specification.

Posted interrupts require a in-memory structure called posted-interrupt descriptor and an interrupt vector
called posted interrupt notification vector.

When posted interrupts are enabled for a specific interrupt, the behavior depends on whether the target CPU
is currently in vmx-root or vmx-non-root mode. If in vmx-non-root mode, the VMM is not invoked and the
interrupt is directly delivered to the executing vCPU. If in vmx-root mode, the posted interrupt notification
vector is raised and a bit in the posted-interupt descriptor is set. Once userland sees such an interrupt,
it has to determine which interrupt in the posted-interrupt descriptor has the highest priority and inject
said interrupt into the guest.

With the current mechanism, we have no way to share this posted interrupt descriptor between hypervisor
and VMM. When using the new machanism, the KPage used for `assign_gsi` and `assign_msi` can be the
posted-interrupt descriptor.

# A Word About Lowest Priority Delivery Mode

Lowest priority delivery mode is an interrupt delivery mode where a set of CPUs can be specified as interrupt
destination but only the cpu with the lowest processor priority will receive it.

This mode cannot be used by user space because user space cannot control the actual vector that is programmed
into the hardware. Thus, the receiving processor which is the one with the lowest priority with regards to the
host might not be the one with the lowest priority for the guest (because the guest uses a virtualized LAPIC).

Thus, lowest priority delivery mode for physical devices will not be supported by the hypervisor interrupt interface.

# Kernel API Changes

## HIP

The `sel_gsi` field will be renamed to `user_irq_num`. `user_irq_num`
specifies the number of available interrupt vectors that userspace can
use for each CPU.

## Capabilities

Hedron will not expose any special interrupt semaphores to the
roottask anymore.

## irq_ctrl

The `irq_ctrl` system call is used to perform operations on the
interrupt configuration. Individual operations on interrupts are
sub-operations of this system call.

Logically, the `irq_ctrl` suboperations can be used to do the
following things:

- They can connect a MSI or pin-based (IOAPIC) interrupt to an
  interrupt vector on a specific CPU (the `irq_ctrl_assign`
  suboperations).
- They can connect a vector on a specific CPU to a semaphore/kpage
  pair (`irq_ctrl_configure_vector`).

To handle level triggered interrupts, the kernel also exposes IOAPIC
pin masking via `irq_ctrl_mask_ioapic_pin`.

The kernel reserves a range of interrupt vectors for userspace
use. These are `0` to `user_irq_num - 1` (inclusive). Userspace can
freely reprogram these vectors using the suboperations below.

Each PD with passthrough permissions (see `create_pd`) can invoke this
system call.

**Access to `irq_ctrl` is inherently insecure and should not be
granted to untrusted userspace PDs.**

### In

| *Register* | *Content*          | *Description*                                                                     |
|------------|--------------------|-----------------------------------------------------------------------------------|
| ARG1[7:0]  | System Call Number | Needs to be `HC_IRQ_CTRL`.                                                        |
| ARG1[9:8]  | Sub-operation      | Needs to be one of `HC_IRQ_CTRL_*` to select one of the `irq_ctrl_*` calls below. |
| ...        | ...                |                                                                                   |

### Out

See the specific `irq_ctrl` sub-operation.

## `irq_ctrl_configure_vector`

This system call connects an interrupt vector on a CPU to a
semaphore/kpage pair. In essence, this configures how userspace will
be informed when the kernel receives an interrupt that it doesn't
handle itself.

The kernel ignores interrupts that arrive on vectors that are not
configured using this system call.

When the configured interrupt arrives, the kernel will atomically set
the specified bit in the kpage. If this bit was not set before, the
semaphore will be up'ed by the kernel. That means that the semaphore
will receive one `up` event per 0 -> 1 transition of the specified
kpage bit.

If userspace uses the same kpage for multiple interrupts, the
suggested usage is to use a `down_zero` operation on the interrupt and
atomically clear any 1 bits before blocking on the semaphore again.

Calling this system call with a null capability for both semaphore and
kpage will unassign this interrupt. After this, userspace will not be
informed about interrupts via the previously configured semaphore and
kpage. The IOMMU Interrupt Remapping Table Entry for this interrupt
will be cleared. In case of a pin-based (IOAPIC) interrupt the
interrupt will be masked at the IOAPIC.

### In

| *Register*  | *Content*          | *Description*                                                                             |
|-------------|--------------------|-------------------------------------------------------------------------------------------|
| ARG1[7:0]   | System Call Number | Needs to be `HC_IRQ_CTRL`.                                                                |
| ARG1[9:8]   | Sub-operation      | Needs to be `HC_IRQ_CTRL_VECTOR`.                                                         |
| ARG1[11:10] | Ignored            | Should be set to zero.                                                                    |
| ARG1[19:12] | Vector             | The host vector that is configured.                                                       |
| ARG1[35:20] | CPU number         | The CPU number on which the vector is configured.                                         |
| ARG2        | Semaphore Selector | The selector referencing a semaphore the hypervisor will associate with the vector.       |
| ARG3        | KPage Selector     | A object capability referencing a KPage.                                                  |
| ARG4[14:0]  | Bit inside KPage   | The index of the bit that will be set in the kpage when the associated interrupt arrives. |

### Out

| *Register* | *Content*          | *Description*                                               |
|------------|--------------------|-------------------------------------------------------------|
| OUT1[7:0]  | Status             | See "Hypercall Status".                                     |

## `irq_ctrl_assign_ioapic_pin`

This system call configures an IOAPIC pin to deliver interrupts to the
given CPU and vector. If Hedron was booted with IOMMU support, the
interrupt will be whitelisted in the IOMMU Interrupt Routing Table.

If another IOAPIC pin was previously assigned to the given CPU and
vector pair, it will be masked.

If Hedron was booted with IOMMU support and another IOAPIC pin or MSI
was previously assigned to the given CPU and vector pair, its IOMMU
Interrrupt Routing Table entry will be removed.

When a level triggered interrupt arrives, the corresponding IOAPIC pin
will be masked. To receive this particular interrupt again, the pin
must be unmasked using `irq_ctrl_mask_ioapic_pin`.

When this function returns, the respective IOAPIC pin is initially
unmasked.

### In

| *Register*  | *Content*              | *Description*                                                                            |
|-------------|------------------------|------------------------------------------------------------------------------------------|
| ARG1[7:0]   | System Call Number     | Needs to be `HC_IRQ_CTRL`.                                                               |
| ARG1[9:8]   | Sub-operation          | Needs to be `HC_IRQ_CTRL_ASSIGN_IOAPIC_PIN`.                                             |
| ARG1[10]    | Interrupt Trigger Mode | The trigger mode setting of the interrupt (level=1/edge=0);                              |
| ARG1[11]    | Interrupt Polarity     | The polarity setting of the interrupt (low=1/high=0).                                    |
| ARG1[19:12] | Vector                 | The host vector this interrupt should be directed to.                                    |
| ARG1[35:20] | CPU number             | The CPU number this GSI should be routed to.                                             |
| ARG2[3:0]   | IOAPIC ID              | The ID of the associated IOAPIC device.                                                  |
| ARG2[11:4]  | IOAPIC PIN             | The PIN index of the interrupt line on the selected IOAPIC device.                       |

### Out

| *Register* | *Content*          | *Description*                                               |
|------------|--------------------|-------------------------------------------------------------|
| OUT1[7:0]  | Status             | See "Hypercall Status".                                     |

## `irq_ctrl_mask_ioapic_pin`

Mask or unmask a specific interrupt pin of an IOAPIC.

### In

| *Register*  | *Content*          | *Description*                                                                                      |
|-------------|--------------------|----------------------------------------------------------------------------------------------------|
| ARG1[7:0]   | System Call Number | Needs to be `HC_IRQ_CTRL`.                                                                         |
| ARG1[9:8]   | Sub-operation      | Needs to be `HC_IRQ_CTRL_MASK_IOAPIC_PIN`.                                                         |
| ARG1[10]    | Mask/Unmask        | The setting of the mask bit (masked=1/unmasked=0)                                                  |
| ARG1[63:11] | Ignored            | Should be set to zero.                                                                             |
| ARG2[3:0]   | IOAPIC ID          | The ID of the associated IOAPIC device.                                                            |
| ARG2[11:4]  | IOAPIC PIN         | The PIN index of the interrupt line on the selected IOAPIC device that will be masked or unmasked. |

### Out

| *Register* | *Content*          | *Description*                                               |
|------------|--------------------|-------------------------------------------------------------|
| OUT1[7:0]  | Status             | See "Hypercall Status".                                     |

## `irq_ctrl_assign_msi`

Configures an MSI to arrive at the given CPU and vector. This system
call returns the MSI address/data pair that userspace must program
into the corresponding device.

Any previous configuration of the CPU and vector pair will be
overwritten as described in `irq_ctrl_assign_ioapic_pin` for the same
scenario.


### In

| *Register*  | *Content*               | *Description*                                                                              |
|-------------|-------------------------|--------------------------------------------------------------------------------------------|
| ARG1[7:0]   | System Call Number      | Needs to be `HC_ASSIGN_MSI`.                                                               |
| ARG1[9:8]   | Sub-operation           | Needs to be `HC_IRQ_CTRL_ASSIGN_MSI`.                                                      |
| ARG1[11:10] | Ignored                 | Should be set to zero.                                                                     |
| ARG1[19:12] | Vector                  | The host vector this interrupt should be directed to.                                      |
| ARG1[35:20] | CPU number              | The CPU number this GSI should be routed to.                                               |
| ARG2[11:0]  | Ignored                 | Should be se to zero.                                                                      |
| ARG2[63:12] | Device Config/MMIO Page | The host-linear address of the PCI configuration space or HPET MMIO region as page number. |

### Out

| *Register* | *Content*          | *Description*                                               |
|------------|--------------------|-------------------------------------------------------------|
| OUT1[7:0]  | Status             | See "Hypercall Status".                                     |
| OUT2       | MSI address        | The MSI address to program into the device.                 |
| OUT3       | MSI data           | The MSI data to program into the device                     |

# Kernel Implementation Changes

This section gives an overview about planned internal changes in
Hedron to support the above API.

## Resolving the fixed GSI/vector mapping

In the current Hedron, there is a fixed mapping between Hedron
interrupt vectors, GSIs, and IOMMU Interrupt Remapping Table Entry
(IRTE) index. The kernel can freely convert between either of them and
uses the `Gsi::gsi_table` array to keep track of metainformation:

- The responsible IOAPIC (if any) for this GSI,
- The current IOAPIC entry (if any) to avoid expensive re-reading of
  the entry from the IOAPIC when setting or clearing the mask bit,
- The corresponding Interrupt Semaphore to trigger,

When we start to treat each interrupt vector in each CPU separately,
this simple mapping goes away. Some of the above information can not
be attached to the GSI list anymore.

To keep track of per-interrupt-vector information, we introduce a new
data structure `Vector_info`. We also introduce a new array indexed by
CPU and interrupt vector that assigns a `Vector_info` per CPU/vector
pair.

The `Vector_info` struct will store all information that Hedron
requires to signal an interrupt to userspace. To do this, it contains:

- a semaphore reference,
- a kpage reference, and
- the bit number inside the kpage.

Additionally, `Vector_info` must also contain the IOAPIC pin (if any)
that needs to be masked, when this interrupt arrives, to prevent
interrupt storms.

## IOMMU

Both MSIs and pin-based (IOAPIC) interrupts must be whitelisted when
Hedron enables Interrupt Remapping in the IOMMU. Enabling Interrupt
Remapping always happens on hardware that supports it when Hedron is
booted with the "iommu" command-line parameter.

To whitelist interrupts, the IOMMU contains a flat table, the
Interrupt Remapping Table (IRT). Entries in this table are called
Interrupt Remapping Table Entries (IRTEs). The name unfortunately
shares acronyms with the IOAPIC's Interrupt Redirection Table. This
document will use IOMMU IRT to disambiguate the two.

So far, Hedron had an IOMMU IRT that had one entry for each of the
supported 192 interrupts. This size is not sufficient anymore. The
IOMMU IRT now needs an entry for each interrupt vector on each CPU.

The maximum number of IOMMU IRT entries is 2^16 (65536), which should
be sufficient. We currently support 64 CPUs with 192 interrupt vectors
and thus will consume 12288 vectors. The 2^16 entries in the IOMMU IRT
will last for 341 CPUs, until we have to change to a different scheme.

## Synchronization

Hedron currently does not need synchronization to deliver an arriving
interrupt. The mapping from arriving interrupt vector to IRQ semaphore
has been immutable. This will change with this redesign.

As the user can now freely configure interrupt vectors to kpage and
semphore pairs, the interrupt handler in the kernel itself must be
synchronized with updates to its configuration. A simple solution is
to keep a spinlock for each possible vector on each CPU.

The performance impact should be minimal, because the spinlock is not
contended unless userspace repeatedly calls
`irq_ctrl_assign_ioapic_pin` or `irq_ctrl_assign_msi`.

## Optimizations: IOAPICs

Previously, IOAPIC pins were not directly addressed and we used GSI
numbers internally. GSI numbers can be translated to an IOAPIC pin
by consulting the `gsi_table`, which contains the responsible IOAPIC
for each GSI.

IOAPIC data structures are currently linked together in a
`Forward_list`. This list is used for operations on all IOAPICs, such
as saving and restoring state during ACPI S3 suspend-to-RAM.

In the new API, IOAPICs are indexed by ID to avoid searching through
the list of IOAPICs to find the correct one.  For that, we will keep
an array of IOAPIC pointers that is indexed by the 16 possible IOAPIC
IDs. This array will replace the `Forward_list`.

# References

[1] https://gitlab.vpn.cyberus-technology.de/supernova-core/hedron/-/issues/99
