# System Call Reference

## call

Performs an IPC call to a PT. Because a PT is permanently bound to an EC, 
and ECs belongs to specific CPUs, the PT must be bound to the same CPU 
as the caller. All data is transferred via the UTCB. The SC is donated to 
the callee. Thus, the complete time it takes to handle the call is accounted
to the caller until the callee replies.

### In

| *Register*  | *Content*             | *Description*                                   |
|-------------|-----------------------|-------------------------------------------------|
| ARG1[7:0]   | System Call Number    | Needs to be `HC_CALL`.                          |
| ARG1[11:8]  | Flags                 | 0 for blocking, 1 for non-blocking              |
| ARG1[63:12] | Portal selector       | Capability selector of the destination portal   |

### Out

| *Register* | *Content* | *Description*           |
|------------|-----------|-------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status". |

## reply

Replies to a PT call by sending data via the UTCB of the callee local EC to the UTCB of the caller EC.
Only makes sense in portal handlers, such as exception handlers or other application-specific 
IPC endpoints. It's undefined behaviour, if you execute this syscall in a global EC. Reply must be called, after
the promised functionality of the portal was fulfilled. This system call does not return. The caller
returns from its `call` system call instead.

### In

| *Register*  | *Content*             | *Description*                                   |
|-------------|-----------------------|-------------------------------------------------|
| ARG1[7:0]   | System Call Number    | Needs to be `HC_REPLY`.                         |

## create_ec

`create_ec` creates an EC kernel object and a capability pointing to
the newly created kernel object.

An EC can be either a normal host EC or a virtual CPU. It does not
come with scheduling time allocated to it. ECs need scheduling
contexts (SCs) to be scheduled and thus executed.

ECs can be either _global_ or _local_. A global EC can have a
dedicated scheduling context (SC) bound to it. When this SC is
scheduled the EC runs. Global ECs can be both, normal host ECs and
vCPUs. A normal EC bound to an SC builds what is commonly known as a
thread.

Local ECs can only be normal ECs and not vCPUs. They cannot have SCs
bound to them and are used for portal handlers. These handlers never
execute with their own SC, but borrow the scheduling context from the
caller.

Each EC has an _event base_. This event base is an offset into the
capability space of the PD the EC belongs to. Exceptions (for normal
ECs) and VM exits (for vCPUs) are sent as messages to the portal index
that results from adding the event reason to the event base. For vCPUs
the event reason are VM exit reasons, for normal ECs the reasons are
exception numbers.

### In

| *Register*  | *Content*             | *Description*                                                                                               |
|-------------|-----------------------|-------------------------------------------------------------------------------------------------------------|
| ARG1[7:0]   | System Call Number    | Needs to be `HC_CREATE_EC`.                                                                                 |
| ARG1[8]     | Global EC             | If set, create a global EC, otherwise a local EC.                                                           |
| ARG1[9]     | vCPU                  | If set, a vCPU is constructed, otherwise a normal EC.                                                       |
| ARG1[10]    | Use APIC Access Page  | Whether a vCPU should respect the APIC Access Page. Ignored for non-vCPUs or if no vLAPIC page is created.  |
| ARG1[11]    | User Page Destination | If 0, the UTCB / vLAPIC page will be mapped in the parent PD, otherwise it's mapped in the current PD.      |
| ARG1[63:12] | Destination Selector  | A capability selector in the current PD that will point to the newly created EC.                            |
| ARG2        | Parent PD             | A capability selector to a PD domain in which the new EC will execute in.                                   |
| ARG3[11:0]  | CPU number            | Number between 0..MAX (depends on implementation, see `config.hpp`) *Note: ECs are CPU-local.*              |
| ARG3[63:12] | UTCB / vLAPIC Page    | A page number where the UTCB / vLAPIC page will be created. Page 0 means no vLAPIC page or UTCB is created. |
| ARG4        | Stack Pointer         | The initial stack pointer for normal ECs. Ignored for vCPUs.                                                |
| ARG5        | Event Base            | The Event Base of the newly created EC.                                                                     |

### Out

| *Register* | *Content* | *Description*           |
|------------|-----------|-------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status". |

## ec_ctrl

The `ec_ctrl` system call allows to interact with execution contexts.

### Sub-operations

| *Constant*          | *Value* |
|---------------------|---------|
| `HC_EC_CTRL_RECALL` | 0       |

### In

| *Register*  | *Content*          | *Description*                                                                   |
|-------------|--------------------|---------------------------------------------------------------------------------|
| ARG1[7:0]   | System Call Number | Needs to be `HC_EC_CTRL`.                                                       |
| ARG1[9:8]   | Sub-operation      | Needs to be one of `HC_EC_CTRL_*` to select one of the `ec_ctrl_*` calls below. |
| ARG1[63:12] | EC Selector        | A capability selector in the current PD that points to an EC.                   |
| ...         | ...                |                                                                                 |

### Out

See the specific `ec_ctrl` sub-operation.

## ec_ctrl_recall

`ec_ctrl_recall` forces the given execution context to enter its
recall exception handler via its recall exception portal as soon as
possible. ECs can be recalled from any CPU, not only the CPU on which
they are scheduled to run.

The common use case for recall is to force a vCPU into its `RECALL`
handler to be able to inject interrupts into a virtual machine.

### In

| *Register*  | *Content*          | *Description*                                                 |
|-------------|--------------------|---------------------------------------------------------------|
| ARG1[7:0]   | System Call Number | Needs to be `HC_EC_CTRL`.                                     |
| ARG1[9:8]   | Sub-operation      | Needs to be `HC_EC_CTRL_RECALL`.                              |
| ARG1[63:12] | EC Selector        | A capability selector in the current PD that points to an EC. |

### Out

| *Register* | *Content* | *Description*                                |
|------------|-----------|----------------------------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status".                      |

## create_pd

`create_pd` creates a PD kernel object and a capability pointing to
the newly created kernel object. Protection domains are security
boundaries. They consist of several capability spaces. The host,
guest, and DMA capability spaces are the address spaces that are used
for ECs belonging to this PD or DMA from assigned devices. Similarly,
port I/O from ECs in this PD is checked against the port I/O
capability space.

PDs are roughly analogous to processes.

There are two special kinds of PDs. The PD (roottask) initially
created by the hypervisor is privileged in that it directly
delegates arbitrary physical memory, I/O ports, and interrupts. This
property cannot be passed on.

The other special kind of PD is a _passthrough_ PD that has special
hardware access. The roottask is such a passthrough PD and can pass
this right on via the corresponding flag.

**Passthrough access is inherently insecure and should not be granted to
untrusted userspace PDs.**

### In

| *Register*  | *Content*            | *Description*                                                                                                      |
|-------------|----------------------|--------------------------------------------------------------------------------------------------------------------|
| ARG1[7:0]   | System Call Number   | Needs to be `HC_CREATE_PD`.                                                                                        |
| ARG1[8]     | Passthrough Access   | If set and calling PD has the same right, create a PD with special passthrough permissions. See above for details. |
| ARG1[11:9]  | Ignored              | Should be set to zero.                                                                                             |
| ARG1[63:12] | Destination Selector | A capability selector in the current PD that will point to the newly created PD.                                   |
| ARG2        | Parent PD            | A capability selector to the parent PD.                                                                            |
| ARG3        | CRD                  | A capability range descriptor. If this is not empty, the capabilities will be delegated from parent to new PD.     |

### Out

| *Register* | *Content* | *Description*           |
|------------|-----------|-------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status". |

## pd_ctrl

The `pd_ctrl` system call allows modification of protection domain
kernel objects and rights transfers.

### Sub-operations

| *Constant*              | *Value* |
|-------------------------|---------|
| `HC_PD_CTRL_DELEGATE`   | 2       |
| `HC_PD_CTRL_MSR_ACCESS` | 3       |

### In

| *Register* | *Content*          | *Description*                                                                   |
|------------|--------------------|---------------------------------------------------------------------------------|
| ARG1[7:0]  | System Call Number | Needs to be `HC_PD_CTRL`.                                                       |
| ARG1[9:8]  | Sub-operation      | Needs to be one of `HC_PD_CTRL_*` to select one of the `pd_ctrl_*` calls below. |
| ...        | ...                |                                                                                 |

### Out

See the specific `pd_ctrl` sub-operation.

## pd_ctrl_delegate

`pd_ctrl_delegate` transfers memory, port I/O and object capabilities
from one protection domain to another. It allows the same
functionality as rights delegation via IPC.

For memory delegations, the CRD controls the type of the _destination_
page table. The source of delegations is always the source PD's host
page table.

Delegation operations allocate memory in the kernel and may fail with
`OOM` when the kernel runs out of memory. In this case, the delegation
may be partially completed. Userspace can retry the operation when
more memory is available to the kernel.

Delegation operations can also fail with `BAD_PAR` when source or
destination ranges do not refer to valid userspace addresses.

### In

| *Register*  | *Content*          | *Description*                                                                                      |
|-------------|--------------------|----------------------------------------------------------------------------------------------------|
| ARG1[7:0]   | System Call Number | Needs to be `HC_PD_CTRL`.                                                                          |
| ARG1[9:8]   | Sub-operation      | Needs to be `HC_PD_CTRL_DELEGATE`.                                                                 |
| ARG1[63:12] | Source PD          | A capability selector for the source protection domain to copy access rights and capabilites from. |
| ARG2        | Destination PD     | A capability selector for the destination protection domain that will receive these rights.        |
| ARG3        | Source CRD         | A capability range descriptor describing the send window in the source PD.                         |
| ARG4        | Delegate Flags     | See [Delegate Flags](../data-structures#delegate-flags) section.                                   |
| ARG5        | Destination CRD    | A capability range descriptor describing the receive window in the destination PD.                 |

### Out

| *Register* | *Content* | *Description*           |
|------------|-----------|-------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status". |

## pd_ctrl_msr_access

`pd_ctrl_msr_access` allows access to MSRs from passthrough PDs (see
`create_pd`). Several MSRs that are critical to the operation of the
hypervisor are not accessible.

**MSR access is inherently insecure and should not be granted to
untrusted userspace PDs.**

### In

| *Register*  | *Content*          | *Description*                                                         |
|-------------|--------------------|-----------------------------------------------------------------------|
| ARG1[7:0]   | System Call Number | Needs to be `HC_PD_CTRL`.                                             |
| ARG1[9:8]   | Sub-operation      | Needs to be `HC_PD_CTRL_MSR_ACCESS`.                                  |
| ARG1[10]    | Write              | If set, the access is a write to the MSR. Otherwise, the MSR is read. |
| ARG1[11]    | Ignored            | Should be set to zero.                                                |
| ARG1[63:12] | MSR Index          | The MSR to read or write.                                             |
| ARG2        | MSR Value          | If the operation is a write, the value to write, otherwise ignored.   |

### Out

| *Register* | *Content* | *Description*                                |
|------------|-----------|----------------------------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status".                      |
| OUT2       | MSR Value | MSR value when the operation is a read.      |

## create_sm

`create_sm` creates an SM kernel object and a capability pointing to the newly created kernel object.

### In

| *Register*  | *Content*            | *Description*                                                                    |
|-------------|----------------------|----------------------------------------------------------------------------------|
| ARG1[7:0]   | System Call Number   | Needs to be `HC_CREATE_SM`.                                                      |
| ARG1[63:12] | Destination Selector | A capability selector in the current PD that will point to the newly created SM. |
| ARG2        | Owner PD             | A capability selector to a PD that owns the SM.                                  |
| ARG3        | Initial Count        | Initial integer value of the semaphore counter.                                  |

### Out

| *Register* | *Content* | *Description*           |
|------------|-----------|-------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status". |

## revoke

The `revoke` system call is used to remove capabilities from a
protection domain. For object capabilities and I/O ports, capabilities
are recursively revoked for every PD that received their mapping from
the given capability range.

Usage with memory CRDs is **deprecated** and currently limited to
revoking all rights at the same time. It will be removed, use
`pd_ctrl_delegate` instead.

### In

| *Register*  | *Content*          | *Description*                                                                             |
|-------------|--------------------|-------------------------------------------------------------------------------------------|
| ARG1[7:0]   | System Call Number | Needs to be `HC_REVOKE`.                                                                  |
| ARG1[8]     | Self               | If set, the capability is also revoked in the current PD. Ignored for memory revocations. |
| ARG1[9]     | Remote             | If set, the given PD is used instead of the current one.                                  |
| ARG2        | CRD                | The capability range descriptor describing the region to be removed.                      |
| ARG3        | PD                 | If remote is set, this is the PD to revoke rights from.                                   |

### Out

| *Register* | *Content* | *Description*           |
|------------|-----------|-------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status". |

## machine_ctrl

The `machine_ctrl` system call is used to perform global operations on
the machine the hypervisor is running on. Individual operations
are sub-operations of this system call.

Only PDs with passthrough permissions (see `create_pd`) can invoke
this system call.

**Access to `machine_ctrl` is inherently insecure and should not be
granted to untrusted userspace PDs.**

### Sub-operations

| *Constant*                         | *Value* |
|------------------------------------|---------|
| `HC_MACHINE_CTRL_SUSPEND`          | 0       |
| `HC_MACHINE_CTRL_UPDATE_MICROCODE` | 1       |

### In

| *Register* | *Content*          | *Description*                                                                             |
|------------|--------------------|-------------------------------------------------------------------------------------------|
| ARG1[7:0]  | System Call Number | Needs to be `HC_MACHINE_CTRL`.                                                            |
| ARG1[9:8]  | Sub-operation      | Needs to be one of `HC_MACHINE_CTRL_*` to select one of the `machine_ctrl_*` calls below. |
| ...        | ...                |                                                                                           |

### Out

See the specific `machine_ctrl` sub-operation.

## machine_ctrl_suspend

The `machine_ctrl_suspend` system call performs the last step of
putting the system into an ACPI sleep state. It will park all
application processors, save any internal state that will be lost
while sleeping, flush caches and finally program SLP_TYPx fields and
sets the SLP_EN bit to enter the sleep state. The wake vector in the
FACS will be temporarily overwritten and restored after the system has
resumed.

The parameters of the system call are the respective `SLP_TYP` values
that are indicated by the `\_Sx` system state DSDT method.

For userspace the suspend/resume cycle will happen during the
execution of the system call and execution resumes as normal when the
system call returns.

Hedron will not restore the IOAPIC configuration. After resume, all
IOAPIC RTEs will be masked and need to be re-programmed using
`irq_ctrl_assign_ioapic_pin`.

Userspace **must not** attempt to put the system into sleep states S1 to
S3 without using this system call, because it will put the system in
an undefined state.

Userspace **should not** call this function concurrently. All
invocations except of one will fail in this case.

The ACPI specification knows two variants of entering S4 (see ACPI
Specification 6.2 Section 16.1.4). This system call does not support
either variant. Userspace can trigger a OSPM-initiated S4 transition
directly and needs to make sure that Hedron is reloaded when the system
resumes. How this is performed is out-of-scope for this document. In
this case, Hedron will not retain state. Platform firmware-initiated S4
transitions are not supported in general.

Hardware-reduced ACPI platforms are **not** supported.

### In

| *Register*  | *Content*          | *Description*                               |
|-------------|--------------------|---------------------------------------------|
| ARG1[7:0]   | System Call Number | Needs to be `HC_MACHINE_CTRL`.              |
| ARG1[9:8]   | Sub-operation      | Needs to be `HC_MACHINE_CTRL_SUSPEND`.      |
| ARG1[11:10] | Ignored            | Should be set to zero.                      |
| ARG1[19:12] | PM1a_CNT.SLP_TYP   | The value to write into `PM1a_CNT.SLP_TYP`. |
| ARG1[27:20] | PM1b_CNT.SLP_TYP   | The value to write into `PM1b_CNT.SLP_TYP`. |

### Out

| *Register*  | *Content*     | *Description*                                                         |
|-------------|---------------|-----------------------------------------------------------------------|
| OUT1[7:0]   | Status        | See "Hypercall Status".                                               |
| OUT2[62:0]  | Waking Vector | The value of the FACS waking vector                                   |
| OUT2[63:62] | Waking Mode   | The desired execution mode, only Real Mode (0) is supported right now |

## machine_ctrl_update_microcode

The `machine_ctrl_update_microcode` system call performs the microcode update
supplied as a physical address.

Although the microcode update size needs to be passed to avoid page-faults in
the kernel, there are no sanity checks that this size is actually correct.

It is up to the userspace to match the correct microcode update to the current
platform, the kernel does no additional checks.

**Note that this functionality is inherently insecure and needs to be used with
caution. The kernel also does not rediscover features after the update was
applied.**

### In

| *Register*  | *Content*          | *Description*                                   |
|-------------|--------------------|-------------------------------------------------|
| ARG1[7:0]   | System Call Number | Needs to be `HC_MACHINE_CTRL`.                  |
| ARG1[9:8]   | Sub-operation      | Needs to be `HC_MACHINE_CTRL_UPDATE_MICROCODE`. |
| ARG1[11:10] | Ignored            | Should be set to zero.                          |
| ARG1[52:12] | Update BLOB size   | Size of the complete update BLOB.               |
| ARG1[63:10] | Ignored            | Should be set to zero.                          |
| ARG2        | Update address     | Physical address of the update BLOB.            |

### Out

| *Register* | *Content* | *Description*                                |
|------------|-----------|----------------------------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status".                      |

## sm_ctrl

The `sm_ctrl`-syscall consists of the two sub calls `sm_ctrl_up` and `sm_ctrl_down`.

### Sub-operations

| *Constant*        | *Value* |
|-------------------|---------|
| `HC_SM_CTRL_UP`   | 0       |
| `HC_SM_CTRL_DOWN` | 1       |

### In

| *Register* | *Content*          | *Description*                                                                   |
|------------|--------------------|---------------------------------------------------------------------------------|
| ARG1[7:0]  | System Call Number | Needs to be `HC_SM_CTRL`.                                                       |
| ARG1[9:8]  | Sub-operation      | Needs to be one of `HC_SM_CTRL_*` to select one of the `sm_ctrl_*` calls below. |
| ...        | ...                |                                                                                 |

### Out

See the specific `sm_ctrl` sub-operation.

## sm_ctrl_up
Performs an "up" operation on the underlying semaphore.

### In

| *Register*  | *Content*                     | *Description*                         |
|-------------|-------------------------------|---------------------------------------|
| ARG1[7:0]   | System Call Number            | Needs to be `HC_SM_CTRL`.             |
| ARG1[8:8]   | Sub-operation                 | Needs to be `SM_CTRL_UP`.             |
| ARG1[11:9]  | Ignored                       | Should be set to zero.                |
| ARG1[63:12] | SM selector                   | Capability selector of the semaphore. |

### Out

| *Register* | *Content* | *Description*                                |
|------------|-----------|----------------------------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status".                      |

## sm_ctrl_down
Performs a "down" operation on the underlying semaphore. The timeout parameter can be disabled
by setting it to zero. Setting it to a value different from zero enables the usage of a semaphore
as timer based on clock ticks.

### In

| *Register*  | *Content*                     | *Description*                         |
|-------------|-------------------------------|---------------------------------------|
| ARG1[7:0]   | System Call Number            | Needs to be `HC_SM_CTRL`.             |
| ARG1[8:8]   | Sub-operation                 | Needs to be `SM_CTRL_DOWN`.           |
| ARG1[11:9]  | Ignored                       | Should be set to zero.                |
| ARG1[63:12] | SM selector                   | Capability selector of the semaphore. |
| ARG2[31:0]  | TSC Deadline Timeout (Higher) | Higher 32-bits of the timeout.        |
| ARG2[63:32] | Ignored                       | Should be set to zero.                |
| ARG3[31:0]  | TSC Deadline Timeout (Lower)  | Lower 32-bits of the timeout.         |
| ARG3[63:32] | Ignored                       | Should be set to zero.                |

### Out

| *Register* | *Content* | *Description*                                |
|------------|-----------|----------------------------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status".                      |

## create_kp

Create a new kernel page object. This object is used for shared memory
between kernel and user space. Kernel pages can be mapped to user space
using `kp_ctrl`.

### In

| *Register*  | *Content*            | *Description*                                                                    |
|-------------|----------------------|----------------------------------------------------------------------------------|
| ARG1[7:0]   | System Call Number   | Needs to be `HC_CREATE_KP`.                                                      |
| ARG1[63:12] | Destination Selector | A capability selector in the current PD that will point to the newly created KP. |
| ARG2        | Owner PD             | A capability selector to a PD that owns the KP.                                  |

### Out

| *Register* | *Content* | *Description*           |
|------------|-----------|-------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status". |

## kp_ctrl

The `kp_ctrl` system calls allow modification of kernel page kernel objects.

### Sub-operations

| *Constant*      | *Value* |
|-----------------|---------|
| `KP_CTRL_MAP`   | 0       |
| `KP_CTRL_UNMAP` | 1       |

### In

| *Register* | *Content*          | *Description*                                                                   |
|------------|--------------------|---------------------------------------------------------------------------------|
| ARG1[7:0]  | System Call Number | Needs to be `HC_KP_CTRL`.                                                       |
| ARG1[9:8]  | Sub-operation      | Needs to be one of `HC_KP_CTRL_*` to select one of the `kp_ctrl_*` calls below. |
| ...        | ...                |                                                                                 |

### Out

See the specific `kp_ctrl` sub-operation.

## kp_ctrl_map

This system call allows mapping a kernel page into the host address space.
Kernel pages can only be mapped _once_. Afterwards, mapping attempts will fail.

### In

| *Register*  | *Content*           | *Description*                                                                           |
|-------------|---------------------|-----------------------------------------------------------------------------------------|
| ARG1[7:0]   | System Call Number  | Needs to be `HC_KP_CTRL`.                                                               |
| ARG1[9:8]   | Sub-operation       | Needs to be `HC_KP_CTRL_MAP`.                                                           |
| ARG1[63:12] | KP Selector         | A capability selector in the current PD that points to a KP.                            |
| ARG2        | Destination PD      | A capability selector for the destination PD that will receive the kernel page mapping. |
| ARG3        | Destination Address | The page aligned virtual address in user space where the kernel page will be mapped.    |

### Out

| *Register* | *Content* | *Description*           |
|------------|-----------|-------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status". |

## `kp_ctrl_unmap`

Unmap kernel pages from user space. A kernel page can only be mapped in a single
location. Calling `kp_ctrl_unmap` to unmap a kernel page is a necessary prerequisite
to mapping the kernel page again using `kp_ctrl_map`.

Although this system call returns a `BAD_PAR` status when called with a kernel page
that is not mapped, it **cannot detect** whether an existing mapping has been
unmapped/overmapped using `pd_ctrl_delegate`.

### In

| *Register*  | *Content*          | *Description*                                                |
|-------------|--------------------|--------------------------------------------------------------|
| ARG1[7:0]   | System Call Number | Needs to be `HC_KP_CTRL`.                                    |
| ARG1[9:8]   | Sub-operation      | Needs to be `HC_KP_CTRL_UNMAP`.                              |
| ARG1[63:12] | KP Selector        | A capability selector in the current PD that points to a KP. |

### Out

| *Register* | *Content* | *Description*           |
|------------|-----------|-------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status". |

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

Only PDs with passthrough permissions (see `create_pd`) can invoke
this system call.

**Access to `irq_ctrl` is inherently insecure and should not be
granted to untrusted userspace PDs.**

### Sub-operations

| *Constant*                   | *Value* |
|------------------------------|---------|
| `IRQ_CTRL_CONFIGURE_VECTOR`  | 0       |
| `IRQ_CTRL_ASSIGN_IOAPIC_PIN` | 1       |
| `IRQ_CTRL_MASK_IOAPIC_PIN`   | 2       |
| `IRQ_CTRL_ASSIGN_MSI`        | 3       |
| `IRQ_CTRL_ASSIGN_LVT`        | 4       |
| `IRQ_CTRL_MASK_LVT`          | 5       |

### In

| *Register* | *Content*          | *Description*                                                                     |
|------------|--------------------|-----------------------------------------------------------------------------------|
| ARG1[7:0]  | System Call Number | Needs to be `HC_IRQ_CTRL`.                                                        |
| ARG1[11:8] | Sub-operation      | Needs to be one of `HC_IRQ_CTRL_*` to select one of the `irq_ctrl_*` calls below. |
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
will at least receive one `up` event per 0 -> 1 transition of the
specified kpage bit.

If userspace uses the same kpage for multiple interrupts, the
suggested usage is to use a `down_zero` operation on the sempahore and
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
| ARG1[11:8]  | Sub-operation      | Needs to be `HC_IRQ_CTRL_VECTOR`.                                                         |
| ARG1[19:12] | Vector             | The host vector that is configured.                                                       |
| ARG1[35:20] | CPU number         | The CPU number on which the vector is configured.                                         |
| ARG1[63:36] | Unused             | Must be zero.                                                                             |
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
interrupt will be whitelisted in the IOMMU Interrupt Remapping Table.

Only a single interrupt can be assigned to a single CPU and vector
pair.  If another IOAPIC pin was previously configured as
level-triggered and assigned to the given CPU and vector pair, it
**must** be masked via `irq_ctrl_mask_ioapic_pin` to avoid interrupt
storms.

If Hedron was booted with IOMMU support and another IOAPIC pin or MSI
was previously assigned to the given CPU and vector pair, its IOMMU
Interrupt Remapping Table entry will be removed.

When a level triggered interrupt arrives, the corresponding IOAPIC pin
will be masked. To unmask this particular interrupt again, the pin
must be unmasked using `irq_ctrl_mask_ioapic_pin`.

When this function returns, the respective IOAPIC pin is initially
unmasked.

### In

| *Register*  | *Content*              | *Description*                                                      |
|-------------|------------------------|--------------------------------------------------------------------|
| ARG1[7:0]   | System Call Number     | Needs to be `HC_IRQ_CTRL`.                                         |
| ARG1[11:8]  | Sub-operation          | Needs to be `HC_IRQ_CTRL_ASSIGN_IOAPIC_PIN`.                       |
| ARG1[19:12] | Vector                 | The host vector this interrupt should be directed to.              |
| ARG1[35:20] | CPU number             | The CPU number this GSI should be routed to.                       |
| ARG1[36]    | Interrupt Trigger Mode | The trigger mode setting of the interrupt (level=1/edge=0);        |
| ARG1[37]    | Interrupt Polarity     | The polarity setting of the interrupt (low=1/high=0).              |
| ARG1[63:38] | Unused                 | Must be zero.                                                      |
| ARG2[3:0]   | IOAPIC ID              | The ID of the associated IOAPIC device.                            |
| ARG2[11:4]  | IOAPIC PIN             | The PIN index of the interrupt line on the selected IOAPIC device. |

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
| ARG1[11:8]  | Sub-operation      | Needs to be `HC_IRQ_CTRL_MASK_IOAPIC_PIN`.                                                         |
| ARG1[12]    | Mask/Unmask        | The setting of the mask bit (masked=1/unmasked=0)                                                  |
| ARG1[63:13] | Unused             | Must be zero.                                                                                      |
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

If Hedron was booted with IOMMU support and another IOAPIC pin or MSI
was previously assigned to the given CPU and vector pair, its IOMMU
Interrupt Remapping Table entry will be removed.

If a IOAPIC pin was previously configured as level-triggered and
assigned to the given CPU and vector pair, it **must** be masked via
`irq_ctrl_mask_ioapic_pin` to avoid interrupt storms.

Hedron can not automatically clean up IOMMU Interrupt Remapping Table
Entries for MSIs that are not used anymore. It is up to userspace to
remove entries in the IOMMU using `irq_ctrl_configure_vector`. Failing
to remove entries can result in PCI devices being able to trigger
interrupts that they should not be able to.

### In

| *Register*  | *Content*               | *Description*                                                                              |
|-------------|-------------------------|--------------------------------------------------------------------------------------------|
| ARG1[7:0]   | System Call Number      | Needs to be `HC_ASSIGN_MSI`.                                                               |
| ARG1[11:8]  | Sub-operation           | Needs to be `HC_IRQ_CTRL_ASSIGN_MSI`.                                                      |
| ARG1[19:12] | Vector                  | The host vector this interrupt should be directed to.                                      |
| ARG1[35:20] | CPU number              | The CPU number this MSI should be routed to.                                               |
| ARG2[11:0]  | Ignored                 | Should be set to zero.                                                                     |
| ARG2[63:12] | Device Config/MMIO Page | The host-linear address of the PCI configuration space or HPET MMIO region as page number. |

### Out

| *Register* | *Content*          | *Description*                                               |
|------------|--------------------|-------------------------------------------------------------|
| OUT1[7:0]  | Status             | See "Hypercall Status".                                     |
| OUT2       | MSI address        | The MSI address to program into the device.                 |
| OUT3       | MSI data           | The MSI data to program into the device                     |

## `irq_ctrl_assign_lvt`

Configures a Local APIC Local Vector Table (LVT) entry on the
**current CPU** to arrive at the given vector. See the Intel
SDM Vol. 3 Section 10.5.1 "Local Vector Table" for information about
LVTs in general.

The LVT numbers of LVT entries that can be modified using this
interface are as follows:

| *Number* | *LVT Entry*    |
|----------|----------------|
| 0        | Thermal Sensor |

These concrete LVT entry numbers are Hedron-specific. Other LVT
entries may be supported in the future. Configuring LVT entries that
the platform does not support results in a `BAD_PAR` return value.

When this function returns, the respective LVT entry is unmasked.

### In

| *Register*  | *Content*          | *Description*                                                           |
|-------------|--------------------|-------------------------------------------------------------------------|
| ARG1[7:0]   | System Call Number | Needs to be `HC_ASSIGN_MSI`.                                            |
| ARG1[11:8]  | Sub-operation      | Needs to be `HC_IRQ_CTRL_ASSIGN_LVT`.                                   |
| ARG1[19:12] | Vector             | The host vector this interrupt should be directed to.                   |
| ARG1[63:20] | Unused             | Must be zero.                                                           |
| ARG2[7:0]   | LVT Entry Number   | The number of the LVT entry that should be assigned. (See table above.) |

### Out

| *Register* | *Content*          | *Description*                                               |
|------------|--------------------|-------------------------------------------------------------|
| OUT1[7:0]  | Status             | See "Hypercall Status".                                     |

## `irq_ctrl_mask_lvt`

Mask or unmask a Local APIC LVT entry on the **current** CPU.

Unmasking LVT entries that have not been configured with
`irq_ctrl_assign_lvt` leads to interrupts arriving at Hedron and being
ignored.

Configuring LVT entries that the platform does not support results in
a `BAD_PAR` return value.

### In

| *Register*  | *Content*          | *Description*                                                                |
|-------------|--------------------|------------------------------------------------------------------------------|
| ARG1[7:0]   | System Call Number | Needs to be `HC_ASSIGN_MSI`.                                                 |
| ARG1[11:8]  | Sub-operation      | Needs to be `HC_IRQ_CTRL_MASK_LVT`.                                          |
| ARG1[12]    | Mask/Unmask        | The setting of the mask bit (masked=1/unmasked=0)                            |
| ARG1[63:13] | Unused             | Must be zero.                                                                |
| ARG2[7:0]   | LVT Entry Number   | The number of the LVT entry is masked/unmasked. (See `irq_ctrl_assign_lvt`.) |

### Out

| *Register* | *Content*          | *Description*                                               |
|------------|--------------------|-------------------------------------------------------------|
| OUT1[7:0]  | Status             | See "Hypercall Status".                                     |

## `create_vcpu`

`create_vcpu` creates a vCPU kernel object and a capability pointing to the
newly created kernel object. A vCPU corresponds to a VMCS in Hedron. Each vCPU
executes with the nested page tables of its parent PD.

The vCPU can only be run on the CPU that is given during creation.

### Layout of the vCPU State Page

The layout of the vCPU State Page is a superset of the current UTCB. The UTCB
header is not used.

The vCPU State Page contains an exit reason field that contains the content of
the `Exit reason` VMCS field for Intel CPUs.

### Layout of the FPU State Page

The FPU state page contains the state of the vCPU's FPU as if saved by `XSAVE`.
The layout of this region is determined by hardware. See the Intel SDM Vol. 1
Chapter 13.4 "XSAVE Area".

### Layout of the vLAPIC Page

The vLAPIC page contains the state of the virtual LAPIC as it is needed for
hardware-accelerated Local APIC emulation. The layout of this page is
determined by hardware. When a vLAPIC page is provided, the vCPU will also
respect the APIC access page. See the Intel SDM Vol. 3 Chapter 29 "APIC
Virtualization and Virtual Interrupts".

### Initial State

This section describes the initial state of a vCPU:

- **VMX-preemption timer value**. Set to the maximum value.
- **Pin-Based VM-Execution Controls**. The following controls are enabled by
  default and cannot be disabled by the VMM:
    - External-interrupt exiting
    - NMI exiting
    - Virtual NMIs
    - Activate VMX-preemption timer
- **Processor-Based VM-Execution Controls**. The following controls are enabled
  by default and cannot be disabled by the VMM:
    - HLT exiting
    - Unconditional I/O exiting
    - Activate secondary controls
    - Enable VPID (if available and not disabled using the `novpid` command-line
      parameter)
    - Unrestricted guest

### In

| *Register*  | *Content*                 | *Description*                                                                      |
|-------------|---------------------------|------------------------------------------------------------------------------------|
| ARG1[3:0]   | System Call Number        | Needs to be `HC_CREATE_VCPU`.                                                      |
| ARG1[7:4]   | Reserved                  | Must be zero.                                                                      |
| ARG1[63:8]  | Destination Selector      | A capability selector in the current PD that will point to the newly created vCPU. |
| ARG2[11:0]  | CPU number                | The CPU this vCPU will run on.                                                     |
| ARG2[63:12] | Parent PD                 | A capability selector to a PD domain in which the vCPU will execute in.            |
| ARG3        | vCPU State KPage Selector | A selector of a KPage that is used for vCPU state                                  |
| ARG4        | vLAPIC KPage Selector     | A selector of a KPage that is used as the vLAPIC page                              |
| ARG5        | FPU State KPage Selector  | A selector of a KPAge that is used for FPU state (XSAVE Area)                      |

### Out

| *Register* | *Content* | *Description*           |
|------------|-----------|-------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status". |

## `vcpu_ctrl`

The `vcpu_ctrl` system call allows to interact with vCPU objects.

### Sub-operations

| *Constant*          | *Value* |
|---------------------|---------|
| `HC_VCPU_CTRL_RUN`  | 0       |
| `HC_VCPU_CTRL_POKE` | 1       |

### In

| *Register* | *Content*          | *Description*                                                                       |
|------------|--------------------|-------------------------------------------------------------------------------------|
| ARG1[3:0]  | System Call Number | Needs to be `HC_VCPU_CTRL`.                                                         |
| ARG1[5:4]  | Sub-operation      | Needs to be one of `HC_VCPU_CTRL_*` to select one of the `vcpu_ctrl_*` calls below. |
| ARG1[63:8] | vCPU Selector      | A capability selector in the current PD that points to a vCPU.                      |
| ...        | ...                |                                                                                     |

### Out

See the specific `vcpu_ctrl` sub-operation.

## `vcpu_ctrl_run`

This system call runs the given vCPU until a VM exit happens or the vCPU is
poked with `vcpu_ctrl_poke`. The MTD parameter controls which state the
hypervisor needs to copy into the underlying hardware data structures.

When this system call returns successfully, the exit reason can be read from
the vCPU state page. vCPUs **will** spuriously exit with preemption timer exits.

Assuming the vCPU is not in a faulty state, a poked vCPU will always be entered
to make sure that event injection will always be performed.

vCPUs can be executed using this system call from any EC that runs on
the same CPU the vCPU was created for, but only one at a
time. Attempts to run the same vCPU object concurrently or from
different CPUs will fail.

### In

| *Register* | *Content*          | *Description*                                                           |
|------------|--------------------|-------------------------------------------------------------------------|
| ARG1[3:0]  | System Call Number | Needs to be `HC_VCPU_CTRL`.                                             |
| ARG1[5:4]  | Sub-operation      | Needs to be `HC_VCPU_CTRL_RUN`.                                         |
| ARG1[63:8] | vCPU Selector      | A capability selector in the current PD that points to a vCPU.          |
| ARG2       | Modified State MTD | A MTD bitfield that has set bits for each vCPU state that was modified. |

### Out

| *Register* | *Content* | *Description*           |
|------------|-----------|-------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status". |

## `vcpu_ctrl_poke`

Causes the specified vCPU to exit as soon as possible. If the vCPU exits due to
the poke, the exit reason will be `Poked` (255). Please note that this is a
Hedron-specific exit reason. If the CPU was already on its VM exit path, the
poke will not alter the exit reason. Thus the VMM **must not** rely on getting
a specific exit reason after a poke.

### In

| *Register* | *Content*          | *Description*                                                  |
|------------|--------------------|----------------------------------------------------------------|
| ARG1[3:0]  | System Call Number | Needs to be `HC_VCPU_CTRL`.                                    |
| ARG1[5:4]  | Sub-operation      | Needs to be `HC_VCPU_CTRL_POKE`.                               |
| ARG1[63:8] | vCPU Selector      | A capability selector in the current PD that points to a vCPU. |

### Out

| *Register* | *Content* | *Description*           |
|------------|-----------|-------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status". |
