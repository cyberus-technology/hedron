# NOVA: System Call Interface

**This document is currently work-in-progress. Information in this
document should be correct, but the document itself is incomplete.**

This document describes the system call interface for the NOVA microhypervisor.

# Data Structures

## Capability Range Descriptor (CRD)

A CRD is a 64-bit value.

| *Field*   | *Content* | *Description*                                         |
|-----------|-----------|-------------------------------------------------------|
| `CRD[1:0]`  | Type      | Selects which type of capabilities the CRD refers to. |
| `CRD[63:2]` | ...       | Depends on capability type.                           |

### Memory CRD

| *Field*      | *Content*          | *Description*                                                                                    |
|--------------|--------------------|--------------------------------------------------------------------------------------------------|
| `CRD[1:0]`   | Type               | Needs to be `1` for memory capbilities.                                                          |
| `CRD[2]`     | Read Permission    |                                                                                                  |
| `CRD[3]`     | Write Permission   |                                                                                                  |
| `CRD[4]`     | Execute Permission |                                                                                                  |
| `CRD[11:7]`  | Order              | Describes the size of this memory region as power-of-two of pages of memory.                     |
| `CRD[63:12]` | Base               | The page number of the first page in the region. This number must be naturally aligned by order. |

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
| `OUT1`         | `RDI`             |

## Hypercall Numbers

Hypercalls are identified by these values.

| *Constant*            | *Value* |
|-----------------------|---------|
| `HC_REVOKE`           | 7       |
| `HC_PD_CTRL`          | 8       |
| `HC_PD_CTRL_DELEGATE` | 2       |


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
