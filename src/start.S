/*
 * Startup Code
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
 *
 * Copyright (C) 2017-2018 Markus Partheymüller, Cyberus Technology GmbH.
 * Copyright (C) 2017-2018 Thomas Prescher, Cyberus Technology GmbH.
 * Copyright (C) 2019 Julian Stecklina, Cyberus Technology GmbH.
 *
 * This file is part of the NOVA microhypervisor.
 *
 * NOVA is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NOVA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 */

#include "arch.hpp"
#include "memory.hpp"
#include "selectors.hpp"

#define MULTIBOOT2_MAGIC         0x36d76289
#define MULTIBOOT2_TAG_END       0
#define MULTIBOOT2_TAG_LOAD_BASE 21
/*
 * The boot stack that we used during early CPU boot.
 */
.bss
.align PAGE_SIZE
.zero PAGE_SIZE
BOOT_STACK:

/*
 * The relocation of the hypervisor binary in bytes. This is a signed integer.
 */
.bss
.globl PHYS_RELOCATION
.align 4
PHYS_RELOCATION:        .zero 4

/*
 * Initialization Code
 */
.section .init.multiboot, "ax"

.globl                  __start_bsp

/*
 * Multiboot v1 Header
 */
__boot_header_mbi1:     .long   0x1badb002
                        .long   0x0
                        .long  -0x1badb002
/*
 * Multiboot v2 Header
 */
                        .long   0x0            /* align to 8 byte for mbi2 */
__boot_header_mbi2_s:   .long   0xe85250d6
                        .long   0x0
                        .long   (__boot_header_mbi2_e - __boot_header_mbi2_s)
                        .long  -(0xe85250d6 + (__boot_header_mbi2_e - __boot_header_mbi2_s))
                        /* relocatable tag */
                        .word   0xa /* type = relocatable */
                        .word   0x0 /* flags */
                        .long   0x18 /* size */
                        .long   LOAD_ADDR_MIN
                        .long   LOAD_ADDR_MAX
                        .long   LOAD_ADDR_ALIGN
                        .long   2 /* prefer load at highest possible address */
                        /* end tag */
                        .word   0x0 /* type */
                        .word   0x0 /* flags */
                        .long   0x8 /* size */
__boot_header_mbi2_e:

/*
 * Global Descriptor Table (GDT)
 */
.align                  8, 0x90
__boot_gdt:             .word   0
                        .word   __boot_gdt__ - __boot_gdt - 1
                        .long   __boot_gdt
                        .quad   0x00a09b0000000000
                        .quad   0x00a0930000000000
                        .quad   0x00a0fb0000000000
                        .quad   0x00a0f30000000000
__boot_gdt__:

.macro                  INIT_STATE
                        mov     $SEL_KERN_DATA, %ecx
                        mov     %ecx, %ss
                        mov     $SEL_USER_DATA, %ecx
                        mov     %ecx, %ds
                        mov     %ecx, %es
                        mov     %ecx, %fs
                        mov     %ecx, %gs
                        lea     BOOT_STACK, %rsp
.endm

/*
 * Set up Long Mode (64-bit) by enabling paging and toggling the relevant MSR.
 *
 * rel needs to contain the relative load offset.
 */
.macro                  INIT_PAGING rel
                        mov     %eax, %edi /* backup - used by multiboot */
                        mov     $0xc0000080, %ecx
                        rdmsr
                        or      $0x901, %eax
                        wrmsr
                        mov     $0x6b8, %edx
                        mov     %edi, %eax /* restore - used by multiboot */
                        mov     %edx, %cr4
                        lea     PDBR(\rel), %edx // XXX This is probably not good for anything except boot
                        mov     %edx, %cr3
                        mov     $0x80010023, %edx
                        mov     %edx, %cr0
.endm

/*
 * BSP Startup Code
 */
.code32

/*
 * Compute the page table index for an address in the low address space.
 *
 * Low memory mappings are mostly 1:1 so we need to compensate for
 * relocation. rel needs to contain the relative load offset. Virtual
 * addresses are relocated by adding rel.
 *
 * We assume that the top 32-bit of vaddr are not set. vaddr is adjusted
 * for the physical load offset.
 */
.macro                  PGIDX_LO rel vaddr lvl target
.if (\lvl * PTE_BPL + PAGE_BITS) >= 32
                        xor     \target, \target
.else
                        lea     \vaddr(\rel), \target
                        shr     $(\lvl * PTE_BPL + PAGE_BITS), \target
                        and     $0x1ff, \target
.endif
.endm

/*
 * Compute the page table index for an address in the high address space.
 *
 * We assume that the top 32-bit of vaddr are all set. vaddr is not
 * adjusted for the load address.
 */
.macro                  PGIDX_HI vaddr lvl target
.if (\lvl * PTE_BPL + PAGE_BITS) >= 32
                        mov     $0x1ff, \target
.else
                        mov     $\vaddr, \target
                        stc
                        sar     $(\lvl * PTE_BPL + PAGE_BITS), \target
                        and     $0x1ff, \target
.endif
.endm

__start_bsp:
                        /*
                         * When we enter here, we do not know our physical location in memory.
                         * The bootloader may have relocated us.
                         *
                         * Until we jump into C++ code, we keep track of our relocation in EBP.
                         */

                        xor     %ebp, %ebp

                        /*
                         * Only Multiboot2-loaders can relocate us and they provide the new
                         * load base in the Multiboot information structure.
                         *
                         * The information structure is a list of variable length tagged
                         * entries.
                         */

                        cmp     $MULTIBOOT2_MAGIC, %eax
                        jne     .not_relocated

                        lea     8(%ebx), %esi
.next_tag:              cmpl    $MULTIBOOT2_TAG_END, (%esi)
                        je      .not_relocated

                        cmpl    $MULTIBOOT2_TAG_LOAD_BASE, (%esi)
                        je      .found_load_base_tag

                        add     4(%esi), %esi

                        /* The size doesn't include padding to 8-byte alignment. */
                        add     $0x7, %esi
                        and     $~0x7, %esi

                        jmp     .next_tag

.found_load_base_tag:
                        mov     8(%esi), %ebp
                        sub     $LOAD_ADDR, %ebp
.not_relocated:
                        /*
                         * EBP now contains the difference between LOAD_ADDR and our true load
                         * address. We now start creating our boot page table in preallocated
                         * memory. A high-level description of this code lives in
                         * doc/implementation.md.
                         *
                         * Prepare the low 1 GiB of virtual memory.
                         */

                        lea     (LVL3L + 0x27)(%ebp), %ecx // PML4 entry
                        PGIDX_LO %ebp, LOAD_ADDR, 3, %edi
                        mov     %ecx, LVL4(%ebp, %edi, 8)

                        lea     (LVL2L + 0x27)(%ebp), %ecx // PDPT entry
                        PGIDX_LO %ebp, LOAD_ADDR, 2, %edi
                        mov     %ecx, LVL3L(%ebp, %edi, 8)

                        /*
                         * Provide a mapping for the low 2 MiB to allow booting. This mapping
                         * is needed by __start_cpu.
                         *
                         * This currently needs to be on the same PDT as the entry for this code. So we
                         * end up with a window of 1 GiB for now.
                         */

                        mov     $0xe3, %ecx // PDT entry
                        xor     %edi, %edi
                        PGIDX_LO %edi, CPUBOOT_ADDR, 1, %edi
                        mov     %ecx, LVL2L(%ebp, %edi, 8)

                        /*
                         * Create identity map for LOAD_ADDR + PHYS_RELOCATION.
                         */

                        lea     (LOAD_ADDR + 0xe3)(%ebp), %ecx // PDT entry
                        PGIDX_LO %ebp, LOAD_ADDR, 1, %edi
                        mov     %ecx, LVL2L(%ebp, %edi, 8)

                        /*
                         * Map LINK_ADDR to LINK_END in 2 MiB pages. This creates the
                         * high memory mappings.
                         */

                        lea     (LVL3H + 0x27)(%ebp), %ecx
                        PGIDX_HI LINK_ADDR, 3, %edi
                        mov     %ecx, LVL4(%ebp, %edi, 8)

                        lea     (LVL2H + 0x27)(%ebp), %ecx
                        PGIDX_HI LINK_ADDR, 2, %edi
                        mov     %ecx, LVL3H(%ebp, %edi, 8)

                        /*
                         * Loop for each PDT entry.
                         */

                        mov     $LOAD_END, %edx
                        sub     $LOAD_ADDR, %edx
                        shr     $21, %edx

                        lea     (LOAD_ADDR + 0xe3)(%ebp), %ecx
                        PGIDX_HI LINK_ADDR, 1, %edi

.next_pdt_entry:
                        mov     %ecx, LVL2H(%ebp, %edi, 8)

                        inc     %edi // Next PDT entry
                        add     $(1 << 21), %ecx // Next 2MB page

                        dec     %edx
                        jnz .next_pdt_entry

                        /*
                         * We need to manually adjust the GDT pointer and jump
                         * to 64-bit code to correct for relocation.
                         */

                        add     %ebp, (4 + __boot_gdt)(%ebp)
                        lgdt    (__boot_gdt + 2)(%ebp)

                        add     %ebp, 1f(%ebp)

                        INIT_PAGING %ebp

                        /*
                         * Far jmp from 32-bit mode. The destination will be updated
                         * to reflect relocation. See above.
                         */
                        .byte 0xea
1:
                        .long __start_all
                        .word SEL_KERN_CODE


/*
 * Trampolines in 32-bit reachable virtual memory.
 */
.code64

.globl __start_all
__start_all:
                        movabs  $__start_all_hi, %rdx
                        jmp     *%rdx

.globl __resume_bsp
__resume_bsp:
                        movabs  $__resume_bsp_hi, %rdx
                        jmp     *%rdx

.text

/*
 * Common BSP/AP Startup Code
 */
__start_all_hi:
                        INIT_STATE
                        test    %REG(bx), %REG(bx)
                        je      2f

                        /*
                         * EBP still contains our load offset.
                         */
                        mov     %ebp, PHYS_RELOCATION

                        mov     %REG(ax), %REG(di)
                        mov     %REG(bx), %REG(si)

                        call    init
                        jmp     3f

1:                      pause
2:                      xchg    %REG(bx), boot_lock
                        test    %REG(bx), %REG(bx)
                        je      1b

3:
                        call    setup_cpulocal
                        mov     %REG(ax), %REG(sp)

                        call    bootstrap
                        ud2a

/*
 * BSP Resume Code
 */

.globl __resume_bsp_hi
__resume_bsp_hi:        INIT_STATE

                        call    setup_cpulocal
                        mov     %REG(ax), %REG(sp)

                        call    resume_bsp

                        call    bootstrap
                        ud2a

/*
 * CPU Startup Code
 *
 * This code needs to be position independent. It will be copied by
 * the LAPIC code that brings up APs to low memory. It's also used
 * during resume.
 */
.code16

.globl __start_cpu
.globl __start_cpu_end

__start_cpu:
                        /*
                         * We patch in the physical relocation offset. The following
                         * is a: mov $0, %ebp
                         *
                         * We encode it manually to have enough space in the immediate
                         * value for patching.
                         */
                        .byte 0x66, 0xbd
__patch_rel_phys_offset:
                        .long 0x0

                        INIT_PAGING %ebp
                        lgdtl   %cs:__gdt_desc - __start_cpu

                        /*
                         * Far jmp from 16-bit mode with 32-bit immediate (hence the size
                         * prefix). The destination will be patched in place.
                         *
                         * See Lapic::prepare_cpu_boot.
                         * Spoiler: It's either __start_all or __resume_bsp.
                         */
                        .byte 0x66, 0xea
__patch_rel_jmp_dst:    .long 0
                        .word SEL_KERN_CODE

__gdt_desc:             .word   __boot_gdt__ - __boot_gdt - 1
__patch_rel_gdt_addr:   .long   __boot_gdt
__start_cpu_end:

.section .rodata

/*
 * The location of the jump target in the CPU bootup code.
 */
 .globl __start_cpu_patch_jmp_dst
.align 4
__start_cpu_patch_jmp_dst:
                        .long __patch_rel_jmp_dst     - __start_cpu

/*
 * Offsets in the CPU boot code where the physical load offset needs
 * to be added to. All locations are 32-bit integers.
 */
.globl __start_cpu_patch_rel, __start_cpu_patch_rel_end
.align 4
__start_cpu_patch_rel:
                        .long __patch_rel_phys_offset - __start_cpu
                        .long __patch_rel_jmp_dst     - __start_cpu
                        .long __patch_rel_gdt_addr    - __start_cpu
__start_cpu_patch_rel_end:
