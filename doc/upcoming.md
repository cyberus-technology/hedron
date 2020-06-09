# NOVA: Upcoming Changes to the System Call Interface

This document contains documentation for yet-to-be-implemented NOVA
features. Once features are done, the description will move from this
document to `kernel-interface.md`. Sections are written in a way that
they can be merged into the main document, i.e. their top-level
structure should match the one in the main kernel interface
documentation.

Consider each section of this document a design proposal.

# New System Call: `machine_ctrl`

## Hypercall Numbers

| *Constant*                   | *Value*         |
|------------------------------|-----------------|
| `HC_MACHINE_CTRL`            | 15              |
|------------------------------|-----------------|
| `HC_PD_CTRL_MSR_ACCESS`      | _to be removed_ |
|------------------------------|-----------------|
| `HC_MACHINE_CTRL_SUSPEND`    | 0               |
| `HC_MACHINE_CTRL_MSR_ACCESS` | 1               |

## pd_ctrl_msr_access

_Will move to `machine_ctrl_msr_access`._

## machine_ctrl

The `machine_ctrl` system call is used to perform global operations on
the machine the microhypervisor is running on. Individual operations
are sub-operations of this system call.

Each PD with passthrough permissions (see `create_pd`) can invoke this
system call.

**Access to `machine_ctrl` is inherently insecure and should not be
granted to untrusted userspace PDs.**

### In

| *Register* | *Content*          | *Description*                                                                             |
|------------|--------------------|-------------------------------------------------------------------------------------------|
| ARG1[3:0]  | System Call Number | Needs to be `HC_MACHINE_CTRL`.                                                            |
| ARG1[5:4]  | Sub-operation      | Needs to be one of `HC_MACHINE_CTRL_*` to select one of the `machine_ctrl_*` calls below. |
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

Userspace **must not** attempt to put the system into sleep states S1 to
S3 without using this system call, because it will put the system in
an undefined state.

Userspace **should not** call this function concurrently. All
invocations except of one will fail in this case.

The ACPI specification knows two variants of entering S4 (see ACPI
Specification 6.2 Section 16.1.4). This system call does not support
either variant. Userspace can trigger a OSPM-initiated S4 transition
directly and needs to make sure that NOVA is reloaded when the system
resumes. How this is performed is out-of-scope for this document. In
this case, NOVA will not retain state. Platform firmware-initiated S4
transitions are not supported in general.

Hardware-reduced ACPI platforms are **not** supported.

### In

| *Register*  | *Content*          | *Description*                               |
|-------------|--------------------|---------------------------------------------|
| ARG1[3:0]   | System Call Number | Needs to be `HC_MACHINE_CTRL`.              |
| ARG1[5:4]   | Sub-operation      | Needs to be `HC_MACHINE_CTRL_SUSPEND`.      |
| ARG1[7:6]   | Ignored            | Should be set to zero.                      |
| ARG1[15:8]  | PM1a_CNT.SLP_TYP   | The value to write into `PM1a_CNT.SLP_TYP`. |
| ARG1[23:16] | PM1b_CNT.SLP_TYP   | The value to write into `PM1b_CNT.SLP_TYP`. |

### Out

| *Register* | *Content* | *Description*                                |
|------------|-----------|----------------------------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status".                      |

## machine_ctrl_msr_access

_Moved without changes (except the hypercall and sub-operation
identifiers) from `pd_ctrl_msr_access`._
