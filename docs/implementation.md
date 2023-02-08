# Implementation Details

The purpose of this document is to describe the implementation of the
Hedron hypervisor. This document is meant for the person hacking on
Hedron source code. It's not meant as user documentation.

## Initialization Flow

Hedron shares much of its initialization flow between initial boot-up
and resume (as in ACPI sleep states).

All entry points are located in `src/start.S`. `__start_bsp` is the
initial entry point of the whole hypervisor. `__resume_bsp` is
called on a resume from ACPI sleep states. They start the boostrap
processor (BSP). `__start_cpu` is the 16-bit entry point to bootstrap
CPUs. It is called during initial boot to bring up additional
application processors (APs) by calling into `__start_all`. On resume,
`__start_cpu` is configured to call first into `__resume_bsp` and then
into `__start_all`.

The `boot_lock` spin lock serializes most of the initialization of the
processors. This spin lock is necessary, because early during boot all
processors boot on the same stack and and access the same data
structures. For the BSP, `boot_lock` starts out locked and is released
once the APs have started. Then one AP at a time manages to grab the
`boot_lock` and proceeds through its initialization before it releases
it. Then the next AP configures itself.

Finally, all processors end up at a barrier and wait until all
processors have checked in. When the barrier releases all processors,
the kernel configures the TSC on each processor.

Finally, each processor calls `schedule()` and normal operation can
begin.

```mermaid
graph TD
  A[__start_bsp] --> B[__start_all]
  B -->|if BSP| C["init()"]
  C --> D["setup_cpulocal()"]
  D --> E["bootstrap()"]
  E -->|if BSP| F[start APs]
  F -->|release boot_lock| G[barrier]
  G -->|set TSC| H["schedule()"]

  A2[__start_cpu] --> B
  B -->|if AP, grab boot_lock| D
  E -->|if AP, release boot_lock| G

  A3[__resume_bsp] --> D
  D --> |if resume| E3["resume_bsp()"]
  E3 --> E
```

## The Boot Page Table

The hypervisor supports booting via Multiboot, version 1 and 2. In
both cases, execution starts at `__start_bsp` in 32-bit Protected Mode
without paging enabled.

We allow Multiboot 2 loaders to relocate the hypervisor in physical
memory. Because paging is not enabled, this means that on boot the
hypervisor executes at an initially unknown location. This is
inconvenient, because 32-bit code doesn't support instruction pointer
relative addressing.

To create the boot page table, we use statically allocated memory (see
the linker script). We first create a 2 MiB 1:1 mapping for the
initialization code. This is just used to enable paging and 64-bit
mode and be able to jump into high memory.

We also create a 2 MiB 1:1 mapping at address 0 to facilitate 16-bit
code execution. This is needed to boot APs and boot the BSP after
resume. See the initialization flow above.

Finally, we map the whole hypervisor image again at `LINK_ADDR`. This
is the part of the address space that will be re-used for all future
page tables that are created.

![](images/memlayout.png)

## Memory Reclamation and Read-Copy Update (RCU)

Hedron manages freeing of kernel memory using reference counts and a
variant of Read-Copy-Update (RCU). There are [great resources on
RCU](https://en.wikipedia.org/wiki/Read-copy-update) that give an
overview of RCU itself. Despite RCU being a synchronization mechanism,
Hedron uses it mostly to defer reclamation of resources until the
hypervisor does not hold references anymore. Hedron uses transitions
to userspace as quiescent states.

The main design goal of RCU in Hedron is to minimize costly atomic
reference count updates for normal kernel operations. Reference counts
to objects do not need to be adjusted, if the caller does not generate
new references to these objects that persist across exiting the kernel
to userspace.

All kernel objects that participate in RCU have an
[intrusive](https://www.data-structures-in-practice.com/intrusive-linked-lists/)
RCU list pointer via `Rcu_elem`. This class also adds two callbacks
(`free` and `pre_free`) that can perform cleanup at different
times. These callbacks will be explained below.

Objects that are managed via RCU also typically carry an intrusive
reference count via `Refcount`. When references to these objects are
created/destroyed, the reference count needs to be manually adjusted
via `add_ref`, `del_ref`, or `del_rcu`. See their respective
descriptions for concrete usage instructions.

Once the `Refcount` operations indicate that the object is ready for
destruction, the caller needs to hand it to `Rcu::call()`, which
places it in the hands of the RCU subsystem for cleanup. `Rcu::call()`
will immediately call the `pre_free` callback. At this point, other
parts of the hypervisor **might still have references** to the object
until they exit to userspace.

Once RCU has observed all cores exiting to userspace (or being idle),
it will call the `free` callback of the object. The `free` callback is
in charge of actually deleting the object and thus reclaiming its
memory for further use.

## Building the Hypervisor Information Page (HIP)

The HIP is built in several stages to reflect the information required
by the user land to perform properly. During ACPI table parsing, the
IOMMU feature flag is set according to the hardware and command-line
parameter presence.

The next stage is the explicit `Hip::build()` function that is called
from the `init()` function after all the Multiboot information is set
and previous initialization/discovery (e.g., of the I/O APICs). This
step populates the default fields like signature, version, offsets,
and the I/O APIC as well as memory descriptors. The flags field is
also adjusted to reflect the boot platform (UEFI or not) and the
virtualization flag (VMX) is initialized to being present.

In a third stage, each processor goes through its initialization
routine and removes any features that are not present or usable. For
example, if a processor has no VMX capability, or lacks
essential features (e.g., EPT, URG, etc. in the VMX case), the
corresponding flag in the HIP is cleared at this point. Lastly,
it adds itself to the HIP's CPU descriptors before the bootstrap
processor finalizes the HIP to indicate no further modifications are
accepted because that step calculates the checksum.

**Note that the finalization is currently not enforced in any way
  other than the checksum!**

## Kernel Memory Layout

... write me ...

## Page Table Management

... write me ...

## Error Handling

Given the history of Hedron as research project, robust error handling
was initially not a focus. Memory allocation failures lead to kernel
panics in most of the code base. Many error conditions, such as
invalid memory delegations, are silent for the user, because they are
not propagated to the system call layer.

Robust error handling is important. Not propagating errors and instead
ignoring them or panicing has implications for Hedron development and
production. In development, silent errors often lead to developer
confusion. In production, panics turn recoverable errors into
denial-of-service.

To facilitate convenient and robust error handling, Hedron code can
(and should) use the `Result` type (see
`include/result.hpp`). `Result` offers [Rust-style error
handling](https://doc.rust-lang.org/book/ch09-02-recoverable-errors-with-result.html).
Developers are encouraged to familiarize themselves with Rust-style
error handling to be able to write readable and concise error handling
code.

So far the error handling in Hedron is transitional. We have many
places that panic on errors, but shouldn't. To find these places
search the code base for calls to `unwrap` on results.
