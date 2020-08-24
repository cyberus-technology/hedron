# Hedron: Upcoming Changes to the System Call Interface

This document contains documentation for yet-to-be-implemented Hedron
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
| `HC_PD_CTRL_MSR_ACCESS`      | _to be removed_ |
|------------------------------|-----------------|
| `HC_MACHINE_CTRL_MSR_ACCESS` | 1               |

## pd_ctrl_msr_access

_Will move to `machine_ctrl_msr_access`._

## machine_ctrl_msr_access

_Moved without changes (except the hypercall and sub-operation
identifiers) from `pd_ctrl_msr_access`._
