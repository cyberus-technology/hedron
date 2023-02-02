# Upcoming Changes to the System Call Interface

This document contains documentation for yet-to-be-implemented Hedron
features. Once features are done, the description will move from this
document to `kernel-interface.md`. Sections are written in a way that
they can be merged into the main document, i.e. their top-level
structure should match the one in the main kernel interface
documentation.

Consider each section of this document a design proposal.

## New System Call: `machine_ctrl`

### Hypercall Numbers

| *Constant*                   | *Value*         |
|------------------------------|-----------------|
| `HC_PD_CTRL_MSR_ACCESS`      | _to be removed_ |
|------------------------------|-----------------|
| `HC_MACHINE_CTRL_MSR_ACCESS` | 1               |

### pd_ctrl_msr_access

_Will move to `machine_ctrl_msr_access`._

### machine_ctrl_msr_access

_Moved without changes (except the hypercall and sub-operation
identifiers) from `pd_ctrl_msr_access`._

## New Kernel Object: `KPAGE`

See #99 for context. KPAGE is a kernel object that wraps access to a
single page in the kernel heap. These pages have so far been
implicitly created for, e.g. UTCBs or vLAPIC pages. Creating a
dedicated kernel object for these pages would:

- clear up existing code paths around creation and destruction of threads/vCPUS and
- allow new uses, such as exposing thread FPU state to userspace.

It would also allow for safely reclaiming shared memory between
userspace and the kernel.

### Modified System Call: `create_ec`

`create_ec` is modified to take KP selectors instead of pointers for
UTCB and vLAPIC page. In a second step, the `XSAVE` Area (FPU content)
can also be put on a kernel page.

The User Page Destination (also called `MAP_USER_PAGE_IN_OWNER` in the
code), which allows selecting which PD a UTCB/vLAPIC is mapped in,
will be removed. Its functionality can be achieved by userspace
choosing where to map the kernel page.

## New Kernel Object: `VCPU`

Execution Contexts (ECs) currently do double-duty as both normal host
threads and vCPUs. Whether a EC represents a host thread or vCPU
depends on the `vCPU` flag when the EC is created. Interaction with
both types is largely similar. Whereas normal threads signal page
faults and other exceptions via exception IPC, vCPUs use exception IPC
to deliver VM exits.

Receiving VM exits as IPC leads to convoluted code in userspace to set
up vCPUs. Each possible VM exit reason needs its own portal, even
though most VM exit handlers will go through common code before VM
exit type specific code runs. An additional issue with the current
design is that these VM exit handlers have no good point where the
libc can setup thread-local storage for them.

The largest issue with the current vCPU design is the inability to
execute different vCPUs on the same thread. At the moment, when SVP
wants to execute a different vCPU in the context of a VM Exit handler,
we go through a complicated dance of copying UTCB content and
semaphores.

The solution we have arrived in informal discussion between @gonzo and
@jstecklina is to abandon the concept of these "active" vCPUs and go
to a model that resembles what other hypervisors are doing. In this
"passive" mode, vCPUs are kernel objects that are distinct from ECs
and only run when a thread calls a `run_vcpu` hypercall on them.

This simplifies a list of things:

- vCPUs can float between physical CPUs and userspace does not have to
  worry about TLB flushing issues when vCPUs migrate,
- the `create_ec` API (and its implementation) becomes simpler,
  because it only deals with normal threads,
- userspace does not need to setup any portals or local ECs for
  handling VM exits,
- userspace can setup a vCPU with a handful lines of code,
- userspace can freely choose different vCPUs to run on the same EC,
- the Hedron-defined startup and recall VM exit reasons go away,
- the UTCB does not need to do double-duty as VMCS storage area,
- KPages can be used to hold vCPU state, vLAPIC page and guest FPU
  state,
- the `Mtd::FPU` functionality is not necessary anymore, because
  userspace can access FPU state directly,
- ...

### Design Issues

Introducing a vCPU kernel object type triggers the same design issues
around hypercall IDs and permission bits as introducing the KPage
object type. As KPages are somehwat required to introduce vCPU
objects, we assume these have found a decent solution.

We need to align on how the `recall` operation works with vCPUs. One
option is to modify `recall` to work on vCPU objects as well. But
since event injection into vCPUs should just make the vCPU exit and
not the thread go through its recall portal, this seems like an
awkward solution. As such, I propose a new `vcpu_ctrl_poke` system
call. This system call makes the affected vCPU exit to userspace. The
exit that is actually encountered in userspace may be any VM exit that
currently happened and picked up the "poke" request or if no exit was
happening, userspace would see a host IRQ VM exit.

Support for vCPU objects will be done for Intel VT only. AMD SVM
support will be largely disabled initially. This might not be a
completely bad thing, because AMD SVM has been unused in Hedron for a
long time and should not be used without inspecting the existing code
for correctness.

We need to decide how to handle partial VMCS updates. This is
currently handled by `Mtd` bits. One option is to abandon `Mtd`s for
state transfer from VMCS to host (this assumes reading state is cheap)
and only use a bitmap of updated fields when invoking `run_vcpu`. If
we want to retain functionality to partially copy state from VMCS to
the host, we can offer a `vcpu_ctrl` option to configure the bitmap
per VM exit.

### New System Call: `create_vcpu`

This system call creates a new vCPU object. vCPUs correspond to a VMCS
in Hedron. Each vCPU executes with the guest page table of its parent
PD.

#### Layout of the vCPU State Page

The layout of the vCPU State Page will be similar to the current UTCB
layout for vCPUs to ease transition. The UTCB header will not be used.

An exit reason field is added that contains the content of the exit
reason VMCS field for Intel CPUs.

#### Layout of the FPU State Page

The FPU state page contains the state of the vCPU's FPU as if saved by
`XSAVE`. The layout of this region is determined by hardware. See the
Intel SDM Vol. 1 Chapter 13.4 "XSAVE Area".

#### Layout of the vLAPIC Page

The vLAPIC page contains the state of the virtual LAPIC as it is
needed for hardware-accelerated Local APIC emulation. The layout of
this page is determined by hardware. When a vLAPIC page is provided,
the vCPU will also respect the APIC access page. See the Intel SDM
Vol. 3 Chapter 29 "APIC Virtualization and Virtual Interrupts".

#### In

| *Register* | *Content*                 | *Description*                                                                             |
|------------|---------------------------|-------------------------------------------------------------------------------------------|
| ARG1[3:0]  | System Call Number        | Needs to be `HC_CREATE_VCPU`.                                                             |
| ARG1[7:4]  | Reserved                  | Must be zero.                                                                             |
| ARG1[63:8] | Destination Selector      | A capability selector in the current PD that will point to the newly created vCPU.          |
| ARG2       | Parent PD                 | A capability selector to a PD domain in which the vCPU will execute in.                   |
| ARG3       | vCPU State KPage Selector | A selector of a KPage that is used for vCPU state                                         |
| ARG4       | vLAPIC KPage Selector     | A selector of a KPage that is used as the vLAPIC page                                     |
| ARG5       | FPU State KPage Selector  | A selector of a KPAge that is used for FPU state (XSAVE Area)                             |

#### Out

| *Register* | *Content* | *Description*           |
|------------|-----------|-------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status". |

### New System Call: `vcpu_ctrl`

The `vcpu_ctrl` system call allows to interact with vCPU objects.

#### In

| *Register* | *Content*          | *Description*                                                                       |
|------------|--------------------|-------------------------------------------------------------------------------------|
| ARG1[3:0]  | System Call Number | Needs to be `HC_VCPU_CTRL`.                                                         |
| ARG1[5:4]  | Sub-operation      | Needs to be one of `HC_VCPU_CTRL_*` to select one of the `vcpu_ctrl_*` calls below. |
| ARG1[63:8] | vCPU Selector      | A capability selector in the current PD that points to a vCPU.                      |
| ...        | ...                |                                                                                     |

#### Out

See the specific `vcpu_ctrl` sub-operation.

### New System Call: `vcpu_ctrl_run`

This system call runs the given vCPU until a vCPU exit happens or the
vCPU is poked with `vcpu_ctrl_poke`. The MTD parameter controls which
state the hypervisor needs to copy into the underlying hardware data
structures.

When this system call returns successfully, the exit reason can be
read from the vCPU state page.

vCPUs can be executed using this system call from any EC on any host
CPU, but only one at a time. Attempts to run the same vCPU object
concurrently will fail.

#### In

| *Register* | *Content*          | *Description*                                                           |
|------------|--------------------|-------------------------------------------------------------------------|
| ARG1[3:0]  | System Call Number | Needs to be `HC_VCPU_CTRL`.                                             |
| ARG1[5:4]  | Sub-operation      | Needs to be `HC_VCPU_CTRL_RUN`.                                  |
| ARG1[63:8] | vCPU Selector      | A capability selector in the current PD that points to a vCPU.          |
| ARG2       | Modified State MTD | A MTD bitfield that has set bits for each vCPU state that was modified. |

#### Out

| *Register* | *Content* | *Description*           |
|------------|-----------|-------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status". |

### New System Call: `vcpu_ctrl_poke`

Causes the specified vCPU to exit as soon as possible. The exit reason
may be any exit reason including host IRQ exit.

#### In

| *Register* | *Content*          | *Description*                                                  |
|------------|--------------------|----------------------------------------------------------------|
| ARG1[3:0]  | System Call Number | Needs to be `HC_VCPU_CTRL`.                                    |
| ARG1[5:4]  | Sub-operation      | Needs to be `HC_VCPU_CTRL_POKE`.                               |
| ARG1[63:8] | vCPU Selector      | A capability selector in the current PD that points to a vCPU. |

#### Out

| *Register* | *Content* | *Description*           |
|------------|-----------|-------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status". |

### Modified System Call: `create_ec`

The vCPU flag is removed together with all vCPU related functionality.

### Modified System Call: `ec_ctrl_recall`

This system call will not work on vCPU objects.
