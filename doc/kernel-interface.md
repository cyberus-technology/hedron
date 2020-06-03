# NOVA: System Call Interface

**This document is currently work-in-progress. Information in this
document should be correct, but the document itself is incomplete.**

This document describes the system call interface for the NOVA microhypervisor.

# Data Structures

## Hypervisor Information Page (HIP)

The HIP is a region of memory exported by the microhypervisor to the
roottask. It contains system information that the roottask can
otherwise not obtain.

Check `include/hip.hpp` for its layout.

## Capabilities

A capability is a reference to a kernel object plus associated access
permissions. Capabilities are opaque and immutable to the user. They
cannot be inspected, modified or addressed directly. Instead user
programs access a capability via a capability selector.

There are three different kinds of capabilities:

- memory capabilites,
- port I/O capabilites, and
- object capabilities.

Each kind of capability has unique capability selectors that form the
respective capability _space_ for this kind. Which kind of
capabilities a selector refers to depends on context.

Memory and port I/O capabilities are created by the
microhypervisor. Object capabilities can be created using `create_*`
system calls, see below. When capabilities are created they hold all
permissions. When capabilities are delegated their permissions can be
downgraded, but never upgraded.

## Capability Permissions

Each type of object capability has a different set of permission
bits. There are five permission bits in total. Permissions marked with
0 are unused and should be specified as zero.

### Memory Capabilities

| 4 | 3 | 2 | 1 | 0 |
|---|---|---|---|---|
| 0 | 0 | x | w | r |

Memory capabilities can control eXecute, Write, and Read
permissions. Depending on the platform execute permissions may imply
read permissions.

### Port I/O Capabilities

| 4 | 3 | 2 | 1 | 0 |
|---|---|---|---|---|
| 0 | 0 | 0 | 0 | a |

I/O port capabilities only have a single permission that allows
reading and writing the I/O port.

### Protection Domain (PD) Object Capability

| 4  | 3  | 2  | 1  | 0  |
|----|----|----|----|----|
| sm | pt | sc | ec | pd |

A Protection Domain capability has a permission bit for each object
capability. If the bit is set, only then can this PD be used to create
the corresponding object capability type with a `create_*` system call.

### Execution Context (EC) Object Capability

| 4 | 3  | 2  | 1 | 0  |
|---|----|----|---|----|
| 0 | pt | sc | 0 | ct |

If `ct` is set, the `ec_ctrl` system call is permitted.

If `sc` is set, `create_sc` is allowed to bind a scheduling context.

if `pt` is set, `create_pt` can bind a portal.

### Scheduling Context (SC) Object Capability

| 4 | 3 | 2 | 1 | 0  |
|---|---|---|---|----|
| 0 | 0 | 0 | 0 | ct |

If `ct` is set, the `sc_ctrl` system call is permitted.

### Portal (PT) Object Capability

| 4 | 3 | 2 | 1    | 0  |
|---|---|---|------|----|
| 0 | 0 | 0 | call | ct |

If `ct` is set, the `pt_ctrl` system call is permitted.

If `call` is set, the portal can be traversed using `call`.

### Semaphore (SM) Object Capability

| 4 | 3 | 2 | 1    | 0  |
|---|---|---|------|----|
| 0 | 0 | 0 | down | up |

If `up` is set, the `sm_ctrl` system call is permitted to do an "up" operation.

If `down` is set, the `sm_ctrl` system call is permitted to do a "down" operation.

## Capability Range Descriptor (CRD)

A CRD is a 64-bit value that describes a range of capabilities of a
specific kind. The three kinds of capabilities have slightly different
layouts for their CRDs.

### Null CRD

A null CRD does not refer to any capabilites.

| *Field*     | *Content* | *Description*                   |
|-------------|-----------|---------------------------------|
| `CRD[1:0]`  | Kind      | Needs to be `0` for a Null CRD. |
| `CRD[63:2]` | Ignored   | Should be set to zero.          |

### Memory CRD

A memory CRD refers to pages of address space.

| *Field*      | *Content*   | *Description*                                                                                    |
|--------------|-------------|--------------------------------------------------------------------------------------------------|
| `CRD[1:0]`   | Kind        | Needs to be `1` for memory capabilities.                                                         |
| `CRD[6:2]`   | Permissions | See memory capability permissions above.                                                         |
| `CRD[11:7]`  | Order       | Describes the size of this memory region as power-of-two of pages of memory.                     |
| `CRD[63:12]` | Base        | The page number of the first page in the region. This number must be naturally aligned by order. |

### Port I/O CRD

A port I/O CRD refers to a range of x86 I/O ports.

| *Field*      | *Content*   | *Description*                                                             |
|--------------|-------------|---------------------------------------------------------------------------|
| `CRD[1:0]`   | Kind        | Needs to be `2` for port I/O capabilities.                                |
| `CRD[6:2]`   | Permissions | See I/O port capability permissions above.                                |
| `CRD[11:7]`  | Order       | Describes the size of the range as power-of-two of individual I/O ports.  |
| `CRD[63:12]` | Base        | The address of the first I/O port. It must be naturally aligned by order. |
|              |             |                                                                           |

### Object CRD

| *Field*      | *Content*   | *Description*                                                                            |
|--------------|-------------|------------------------------------------------------------------------------------------|
| `CRD[1:0]`   | Kind        | Needs to be `3` for object capabilities.                                                 |
| `CRD[6:2]`   | Permissions | Permissions are specific to each object capability type. See the relevant section above. |
| `CRD[11:7]`  | Order       | Describes the size of this region as power-of-two of individual capabilities.            |
| `CRD[63:12]` | Base        | The first capability selector. This number must be naturally aligned by order.           |

## Hotspot

A hotspot is used to disambiguate send and receive windows for
delegations. The hotspot carries additional information for some types
of mappings as well.

| *Field*      | *Content*  | *Description*                                                                                                  |
|--------------|------------|----------------------------------------------------------------------------------------------------------------|
| `HOT[0]`     | Type       | Must be `1`                                                                                                    |
| `HOT[7:1]`   | Reserved   | Must be `0`                                                                                                    |
| `HOT[8]`     | !Host      | Mapping needs to go into (0) / not into (1) host page table. Only valid for memory and I/O delegations.        |
| `HOT[9]`     | Guest      | Mapping needs to go into (1) / not into (0) guest page table / IO space. Valid for memory and I/O delegations. |
| `HOT[10]`    | Device     | Mapping needs to go into (1) / not into (0) device page table. Only valid for memory delegations.              |
| `HOT[11]`    | Hypervisor | Source is actually hypervisor PD. Only valid when used by the roottask, silently ignored otherwise.            |
| `HOT[63:12]` | Hotspot    | The hotspot used to disambiguate send and receive windows.                                                     |

## User Thread Control Block (UTCB)

UTCBs belong to Execution Contexts. Each EC representing an ordinary
thread (as opposed to a vCPU) always has an associated UTCB. It is
used to send and receive message data and architectural state via IPC.

The UTCB is 4KiB in size. It's detailed layout is given in
`include/utcb.hpp`.

## Virtual LAPIC (vLAPIC) Page

vLAPIC pages belong to Execution Contexts. A vCPU may have exactly one
vLAPIC page. A vCPU never has an UTCB. See `create_ec` for the
creation of vCPUs.

A vLAPIC page is exactly 4KiB in size. The content of the vLAPIC page
is given by the Intel Architecture. The Intel Software Development
Manual Vol. 3 describes its content and layout in the "APIC
Virtualization and Virtual Interrupts" chapter. In the Intel
documentation, this page is called "virtual-APIC page".

## APIC Access Page

The APIC Access Page is a page of memory that is used to mark the
location of the Virtual LAPIC in a VM. Use of the APIC Access Page can
be controlled on a per-vCPU basis (see `create_ec`).

The Intel Software Development Manual Vol. 3 gives further information
on how the APIC Access Page changes the operation of Intel VT in the
"APIC Virtualization and Virtual Interrupts" chapter. In the Intel
documentation, this page is called "APIC-access page".

# System Call Binary Interface for x86_64

## Register Usage

System call parameters are passed in registers. The following register names are used in the System Call Reference below.

| *Logical Name* | *Actual Register* |
|----------------|-------------------|
| `ARG1`         | `RDI`             |
| `ARG2`         | `RSI`             |
| `ARG3`         | `RDX`             |
| `ARG4`         | `RAX`             |
| `ARG5`         | `R8`              |
|----------------|-------------------|
| `OUT1`         | `RDI`             |
| `OUT2`         | `RSI`             |

## Hypercall Numbers

Hypercalls are identified by these values.

| *Constant*              | *Value* |
|-------------------------|---------|
| `HC_CREATE_PD`          | 2       |
| `HC_CREATE_EC`          | 3       |
| `HC_REVOKE`             | 7       |
| `HC_PD_CTRL`            | 8       |
|-------------------------|---------|
| `HC_PD_CTRL_DELEGATE`   | 2       |
| `HC_PD_CTRL_MSR_ACCESS` | 3       |

## Hypercall Status

Most hypercalls return a status value in OUT1. The following status values are defined:

| *Status*  | *Value* | *Description*                                                    |
|-----------|---------|------------------------------------------------------------------|
| `SUCCESS` | 0       | The operation completed successfully                             |
| `TIMEOUT` | 1       | The operation timed out                                          |
| `ABORT`   | 2       | The operation was aborted                                        |
| `BAD_HYP` | 3       | An invalid hypercall was called                                  |
| `BAD_CAP` | 4       | A hypercall referred to an empty or otherwise invalid capability |
| `BAD_PAR` | 5       | A hypercall used invalid parameters                              |
| `BAD_FTR` | 6       | An invalid feature was requested                                 |
| `BAD_CPU` | 7       | A portal capability was used on the wrong CPU                    |
| `BAD_DEV` | 8       | An invalid device ID was passed                                  |

# System Call Reference

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
| ARG1[3:0]   | System Call Number    | Needs to be `HC_CREATE_EC`.                                                                                 |
| ARG1[4]     | Global EC             | If set, create a global EC, otherwise a local EC.                                                           |
| ARG1[5]     | vCPU                  | If set, a vCPU is constructed, otherwise a normal EC.                                                       |
| ARG1[6]     | Use APIC Access Page  | Whether a vCPU should respect the APIC Access Page. Ignored for non-vCPUs or if no vLAPIC page is created.  |
| ARG1[7]     | User Page Destination | If 0, the UTCB / vLAPIC page will be mapped in the parent PD, otherwise it's mapped in the current PD.      |
| ARG1[63:8]  | Destination Selector  | A capability selector in the current PD that will point to the newly created EC.                            |
| ARG2        | Parent PD             | A capability selector to a PD domain in which the new EC will execute in.                                   |
| ARG3[11:0]  |                       | Unused. Needs to be zero.                                                                                   |
| ARG3[63:12] | UTCB / vLAPIC Page    | A page number where the UTCB / vLAPIC page will be created. Page 0 means no vLAPIC page or UTCB is created. |
| ARG4        | Stack Pointer         | The initial stack pointer for normal ECs. Ignored for vCPUs.                                                |
| ARG5        | Event Base            | The Event Base of the newly created EC.                                                                     |

### Out

| *Register* | *Content* | *Description*           |
|------------|-----------|-------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status". |

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
created by the microhypervisor is privileged in that it directly
delegates arbitrary physical memory, I/O ports, and interrupts. This
property cannot be passed on.

The other special kind of PD is a _passthrough_ PD that has special
hardware access. The roottask is such a passthrough PD and can pass
this right on via the corresponding flag.

**Passthrough access is inherently insecure and should not be granted to
untrusted userspace PDs.**

### In

| *Register* | *Content*            | *Description*                                                                                                      |
|------------|----------------------|--------------------------------------------------------------------------------------------------------------------|
| ARG1[3:0]  | System Call Number   | Needs to be `HC_CREATE_PD`.                                                                                        |
| ARG1[4]    | Passthrough Access   | If set and calling PD has the same right, create a PD with special passthrough permissions. See above for details. |
| ARG1[7:5]  | Ignored              | Should be set to zero.                                                                                             |
| ARG1[63:8] | Destination Selector | A capability selector in the current PD that will point to the newly created PD.                                   |
| ARG2       | Parent PD            | A capability selector to the parent PD.                                                                            |
| ARG3       | CRD                  | A capability range descriptor. If this is not empty, the capabilities will be delegated from parent to new PD.     |

### Out

| *Register* | *Content* | *Description*           |
|------------|-----------|-------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status". |

## pd_ctrl

The `pd_ctrl` system call allows modification of protection domain
kernel objects and rights transfers.

### In

| *Register* | *Content*          | *Description*                                                                   |
|------------|--------------------|---------------------------------------------------------------------------------|
| ARG1[3:0]  | System Call Number | Needs to be `HC_PD_CTRL`.                                                       |
| ARG1[5:4]  | Sub-operation      | Needs to be one of `HC_PD_CTRL_*` to select one of the `pd_ctrl_*` calls below. |
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

### In

| *Register* | *Content*          | *Description*                                                                                      |
|------------|--------------------|----------------------------------------------------------------------------------------------------|
| ARG1[3:0]  | System Call Number | Needs to be `HC_PD_CTRL`.                                                                          |
| ARG1[5:4]  | Sub-operation      | Needs to be `HC_PD_CTRL_DELEGATE`.                                                                 |
| ARG1[63:8] | Source PD          | A capability selector for the source protection domain to copy access rights and capabilites from. |
| ARG2       | Destination PD     | A capability selector for the destination protection domain that will receive these rights.        |
| ARG3       | Source CRD         | A capability range descriptor describing the send window in the source PD.                         |
| ARG4       | Hotspot            | The hotspot to disambiguate when send and receive windows have a different size.                   |
| ARG5       | Destination CRD    | A capability range descriptor describing the receive window in the destination PD.                 |

### Out

| *Register* | *Content* | *Description*           |
|------------|-----------|-------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status". |

## pd_ctrl_msr_access

`pd_ctrl_msr_access` allows access to MSRs from passthrough PDs (see
`create_pd`). Several MSRs that are critical to the operation of the
microhypervisor are not accessible.

**MSR access is inherently insecure and should not be granted to
untrusted userspace PDs.**

### In

| *Register* | *Content*          | *Description*                                                         |
|------------|--------------------|-----------------------------------------------------------------------|
| ARG1[3:0]  | System Call Number | Needs to be `HC_PD_CTRL`.                                             |
| ARG1[5:4]  | Sub-operation      | Needs to be `HC_PD_CTRL_MSR_ACCESS`.                                  |
| ARG1[6]    | Write              | If set, the access is a write to the MSR. Otherwise, the MSR is read. |
| ARG1[7]    | Ignored            | Should be set to zero.                                                |
| ARG1[63:8] | MSR Index          | The MSR to read or write.                                             |
| ARG2       | MSR Value          | If the operation is a write, the value to write, otherwise ignored.   |

### Out

| *Register* | *Content* | *Description*                                |
|------------|-----------|----------------------------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status".                      |
| OUT2       | MSR Value | MSR value when the operation is a read. |

## revoke

The `revoke` system call is used to remove capabilities from a
protection domain. For object capabilities and I/O ports, capabilities
are recursively revoked for every PD that received their mapping from
the given capability range.

Usage with memory CRDs is **deprecated** and currently limited to
revoking all rights at the same time. It will be removed, use
`pd_ctrl_delegate` instead.

### In

| *Register* | *Content*          | *Description*                                                                             |
|------------|--------------------|-------------------------------------------------------------------------------------------|
| ARG1[3:0]  | System Call Number | Needs to be `HC_REVOKE`.                                                                  |
| ARG1[4]    | Self               | If set, the capability is also revoked in the current PD. Ignored for memory revocations. |
| ARG1[5]    | Remote             | If set, the given PD is used instead of the current one.                                  |
| ARG1[63:8] | Semaphore Selector | **Deprecated**, specify as 0.                                                             |
| ARG2       | CRD                | The capability range descriptor describing the region to be removed.                      |
| ARG3       | PD                 | If remote is set, this is the PD to revoke rights from.                                   |

### Out

| *Register* | *Content* | *Description*           |
|------------|-----------|-------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status". |
