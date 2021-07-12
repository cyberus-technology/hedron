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

# New Kernel Object: `KPAGE`

See #99 for context. KPAGE is a kernel object that wraps access to a
single page in the kernel heap. These pages have so far been
implicitly created for, e.g. UTCBs or vLAPIC pages. Creating a
dedicated kernel object for these pages would:

- clear up existing code paths around creation and destruction of threads/vCPUS and
- allow new uses, such as exposing thread FPU state to userspace.

It would also allow for safely reclaiming shared memory between
userspace and the kernel.

## Design Issues

### Permission Bit Shortage

PDs use their 5 permission bits in a capability pointer to check
whether a PD can create a specific kernel object type. These 5 bits
are already occupied (SM, PT, SC, EC, PD). To solve this, there are
several options.

We either need to start to bundle these bits (PT+EC+SC sounds like a
reasonable combination or just have a single permission bit that
governs creation of any kernel objects).

We can widen the permission bits to 6 bits. This wastes space in the
hypervisor's capability spaces, because permission bits are kept in
the lower bits of pointers to kernel objects. At the moment, kernel
objects are aligned to 32 bytes, which leaves 5 available bits. To
provide another bit, we need to increase kernel object alignment to
64 byte.

We can also make capabilities consist of two 64-bit values, this would
give us 64 + 5 permission bits, which seems very future proof.

All the above options are straight-forward to implement. On the more
radical side, we can make system calls be normal capabilities, which
is what L4Re does (I think). This means we do not need permission
bits, as you can achieve the same by just not handing out certain
capabilities. This is probably out of scope though.

Opinions are welcome!

### Hypercall Number Shortage

The hypercall ABI reserves 4 bits for hypercall numbers in the first
hypercall argument (`RDI`). The remaining bits in `RDI` are used for
flags (4 bits) and a capability selector (56 bits). The 4-bit
hypercall numbers allow for 16 different hypercalls, which are already
all used. This leaves no space to add the two new hypercalls required
for KPages.

I propose to extend the hypercall number part of `RDI` to 8 bits. This
still leaves 52 bits for capability selectors on 64-bit
machines. (Incidentally, this also matches with the former use of
capability selectors as page/frame numbers.)

## New System Call: `create_kp`

Create a new kernel page object. This object is used for shared memory
between kernel and userspace. Kernel pages can be mapped to userspace
using `kp_ctrl`.

### In

| *Register*   | *Content*            | *Description*                                                                    |
|--------------|----------------------|----------------------------------------------------------------------------------|
| ARG1[x:0]    | System Call Number   | Needs to be `HC_CREATE_KP`.                                                      |
| ARG1[63:x+4] | Destination Selector | A capability selector in the current PD that will point to the newly created KP. |
| ARG2         | Owner PD             | A capability selector to a PD that is used for permission checking.              |

### Out

| *Register* | *Content* | *Description*           |
|------------|-----------|-------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status". |

## New System Call: `kp_ctrl`

Map kernel pages into userspace. This system call allows mapping a
kernel page into the host address space. Mappings into a device or
guest page table will fail. KPages can only be mapped
_once_. Afterwards, mapping attempts will fail.

### In

| *Register*   | *Content*          | *Description*                                                                           |
|--------------|--------------------|-----------------------------------------------------------------------------------------|
| ARG1[x:0]    | System Call Number | Needs to be `HC_KP_CTRL`.                                                               |
| ARG1[x+1:x]  | Sub-operation      | Needs to be `HC_KP_CTRL_MAP`.                                                           |
| ARG1[63:x+4] | Destination PD     | A capability selector for the destination PD that will receive the kernel page mapping. |
| ARG2         | Destination CRD    | A capability range descriptor describing the receive window in the destination PD.      |

### Out

| *Register* | *Content* | *Description*           |
|------------|-----------|-------------------------|
| OUT1[7:0]  | Status    | See "Hypercall Status". |

## Modified System Call: `create_ec`

`create_ec` is modified to take KP selectors instead of pointers for
UTCB and vLAPIC page. In a second step, the `XSAVE` Area (FPU content)
can also be put on a kernel page.

The User Page Destination (also called `MAP_USER_PAGE_IN_OWNER` in the
code), which allows selecting which PD a UTCB/vLAPIC is mapped in,
will be removed. Its functionality can be achieved by userspace
choosing where to map the kernel page.
