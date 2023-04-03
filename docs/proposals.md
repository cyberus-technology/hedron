# Proposals

## Hedron without Interrupts

This document outlines the API changes necessary to run Hedron without
claiming the Local APIC. The intended audience are developers of
Hedron and the corresponding passthrough VMM.

### Problem Statement

In our current usecase, it is mostly a liability when Hedron claims
the interrupt controllers. The passthrough VMM must virtualizate the
Local APIC and all MSI sources (PCI devices, HPET) even though it just
passes interrupts through. This creates a performance and
compatibility problem.

As an alternative, we plan to remove interrupt handling from Hedron
and let the Passthrough VM handle interrupts directy.

### High-Level Impact

The high-level impact of this change is that Hedron will not be able
to receive (normal maskable) interrupts anymore. For its own purposes,
Hedron will start to use NMIs. NMIs will be used to:

- implement remote TLB shootdowns (now `VEC_IPI_RKE`),
- implement vCPU poke (now `VEC_IPI_RKE`),
- accelerate RCU for idles cores (now `VEC_IPI_IDL`),
- enqueue SCs on remote cores (now `VEC_IPI_RRQ`),
- park cores during suspend (now `VEC_IPI_PRK`).

All of the functionality continue to work as is, just with higher overhead.

Hedron will not be able to drive a timer anymore. This means that
preemptive scheduling is not possible. All time slices will be
effectively infinite.

The passthrough VMM is now in complete control of PCI and IOMMU and is
expected to pass access directly to the passthrough VM. The respective
IOMMU code will be removed from Hedron. It's up to the passthrough VMM
to enforce any DMA restrictions.

### Changes to the System Call API

#### Changed Syscall: `create_sc`

The time quantum parameter will be removed. SCs will have infinite time quantums.

#### New Syscall: `ec_ctrl_yield_current`

The new system call yields the time slice of the currently executing
SC and schedules the next unblocked global EC.

#### Removed Syscall: `irq_ctrl_*`

This family of system calls will be removed. The passthrough VMM has
access to the Local APIC itself.

While Hedron executes host code, interrupts will be disabled (`RFLAGS.IF == 0`).

#### Changed Syscall: `sm_ctrl_down`

The timeout parameters will be removed. Semaphores will not have
timeouts anymore.

#### Removed Syscall: `ec_ctrl_recall`

This system call will be removed. While this system call could be
implemented, its functionality is currently unused and we see no
usecase.

#### Changed Syscall: `pd_ctrl_delegate` and IPC mappings

Hedron will not handle device memory mappings any more. The `Device`
delegate flag will be removed.

### Changes to Guest Execution

For the passthrough VM, external interrupt exiting is not enabled. Any
pending interrupt will be delivered directly to the passthrough VM.

For other VMs, external interrupts exit, but the interrupt is not
acknowledged. This means that pending interrupts will be delivered when
the passthrough VM is executed.

### Changes to the HIP

Hedron will stop reporting the following information in the HIP:

- the IOMMU feature bit,
- any IOAPIC information,
- `num_user_vectors`,
- `pci_bus_start`,
- MCFG region information,
- DMAR table information,
- HPET information,
- BSP LAPIC information (the LAPIC will unconditionally be enabled).

### Issues

Hedron will use the ICR to send NMIs. In xAPIC mode, Hedron will
restore `ICR_HIGH` to its previous value. The `ICR_LOW` value will be
clobbered. This should be harmless. If the passthrough VMM can
guarantee that the passthrough VM never exits between writing
`ICR_HIGH` and `ICR_LOW`, Hedron would not need to restore
`ICR_LOW`. In x2APIC mode, Hedron will clobber the full `ICR`.

Hedron needs to know whether the platform operates in xAPIC or x2APIC
mode to be able send NMIs. Checking this before sending NMI may be
wasteful. Resolving this will be part of enabling x2APIC support
([#212](https://gitlab.vpn.cyberus-technology.de/supernova-core/hedron/-/issues/212)).

## Flexible Heap Size for Hedron

This document outlines a design proposal for the Hedron heap. Hedron
and bootloader developers are the intended audience. This document
assumes knowledge about Hedron's boot flow.

### Problem Statement

Hedron memory allocations are served by a buddy allocator that uses
statically allocated memory as a backing store. The size of this
backing store is a compile-time constant (`HEAP_SIZE_MB`). We call
this memory the _heap_.

Hedron consumes heap memory for the allocation of kernel objects and
for page table memory. Generally in our usage patterns, the number of
kernel objects scales with the number of CPUs. The amount of memory
used for page tables scales with the amount of physical
memory.

Because the amount of memory for kernel objects is relatively small,
we will ignore it for the rest of the discussion and focus on page
table memory.

To map 1 GiB of memory in 4K pieces we need 2 MiB of page table
memory. This comes down to roughly 0.195%. Considering that userspace
eventually wants to map all available memory at least once, Hedron
needs 0.195% of free memory as heap. For our production usecase, we
actually need _two_ mappings, which doubles this tax to 0.39%.

The hardware we currently consider for production ranges from 2 GiB of
RAM to 256 GiB of RAM. If we scale the heap for the worst case, it
would need to be roughly 1 GiB large (0.39% of 256 GiB). But this
would consume half of all available memory on the system with 2 GiB
RAM, which would need only 8 MiB of heap.

Another issue is that, Hedron's ELF image must be unpacked below 1 GiB
in physical memory. If the ELF image contains 1 GiB of BSS segment
for the compile time heap, the bootloader will not find enough space,
because low memory is usually fragmented.

### Assumptions

For the solution below, we assume the memory map of the system is
already immutable when Hedron starts.

This is technically only true when we boot via UEFI when we late-load
Hedron when UEFI boot services are shut down. The reason is that
before Hedron starts, components may have already obtained a memory
map.

### Solution Outline

We have the following goals:

- We want the Hedron image to be small, so the bootloader has no
  problem finding space for it.
- We want the Hedron heap size to be a runtime decision that can
  ideally be configured.
- We want a solution that doesn't require Hedron to modify the memory
  map.

#### UEFI Boot

For UEFI, we already require a separate loader to boot Hedron. This
loader is either a UEFI or Multiboot2 application and loads Hedron via
Multiboot2.

This loader is the natural location to:

1. Make the policy decision what size the heap is, either via
   heuristic or via config file,
2. Allocate the heap in high physical memory.
3. Communicate the heap location to Hedron.

To make a good decision about the total size of Hedron heap, the
loader needs to know the size of the built-in heap in Hedron. To do
this, Hedron will announce its built-in heap size via a Multiboot2
tag.

To keep things simple, the heap region is a single contiguous piece of
memory. As we mostly need this when the system has more than 4 GiB of
RAM and the space beyond 4 GiB is usually not fragmented, the loader
should not have a problem allocating this memory. If this proves a
problem, the design can naturally evolve to multiple heap regions later.

Communicating the heap location from UEFI loader to Hedron can happen
via either
[vendor-specific](https://uefi.org/specs/ACPI/6.4/15_System_Address_Map_Interfaces/uefi-getmemorymap-boot-services-function.html)
memory map entries or via a custom tag in the Multiboot2 [boot
information](https://www.gnu.org/software/grub/manual/multiboot2/html_node/Boot-information-format.html)
structure.

Hedron will then discover this new heap and enable it for allocations
next to its built-in heap. Hedron also needs to prevent userspace from
accessing this memory region.

In case Hedron does not find the special memory region, it will
continue relying on its (now smaller) built-in heap.

#### BIOS Boot (Recommended)

For BIOS boot, we technically don't need a specific loader, but
[Bender](https://github.com/blitz/bender) usage is near universal.

We extend Bender to support Multiboot 2 support and add the same logic
as the UEFI loader has. This is also the path to deprecating and
removing Multiboot 1 support in Hedron, because Bender will act as the
crutch for Multiboot 1 compatibility in the meantime.

#### BIOS Boot (Alternative 1)

We build the same logic into Bender as for the UEFI loader. Bender
communicates the location of the heap via a special memory region in
the Multiboot1 memory map. Bender is also a good location for making
the size of the heap configurable.

#### BIOS Boot (Alternative 2)

If building the heap allocation logic into Bender is undesirable, an
alternative is to build this logic into Hedron itself. For Multiboot1
Hedron then claims a location in memory for the heap.

This is undesirable, because depending on how our stack is set up, the
decision how much heap to allocate would happen in different stages of
the boot process. It also complicates the Hedron implementation, which
is where we need complexity the least.

### Implementation Outline

This section gives a rough idea how certain parts of the above
solution can be implemented.

#### Virtual-to-Physical Translations (and vice versa)

Hedron currently translates virtual to physical addresses (and back)
using simple arithmetic. Once we add a dynamically allocated heap this
becomes more slightly more complicated.

For a contiguous externally allocated heap, the translation is
similar. Hedron maps this heap to a fixed virtual address region in
the kernel address space. Hedron also keeps the physical base address
and size around. With this information, virtual and physical addresses
can be easily translated.

In case Hedron has both the built-in heap and the external heap, the
heaps need to advertise which virtual and physical address regions
they are responsible for. Then a facade can decide where to direct
allocation and deallocation requests and which allocator can translate
addresses.
