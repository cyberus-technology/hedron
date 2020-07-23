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
                        mov     $STACK, %esp
.endm

.macro                  INIT_PAGING
                        mov     %eax, %edi /* backup - used by multiboot */
                        mov     $0xc0000080, %ecx
                        rdmsr
                        or      $0x901, %eax
                        wrmsr
                        mov     $0x6b8, %edx
                        mov     %edi, %eax /* restore - used by multiboot */
                        mov     %edx, %cr4
                        mov     $PDBR, %edx
                        mov     %edx, %cr3
                        mov     $0x80010023, %edx
                        mov     %edx, %cr0
.endm

/*
 * BSP Startup Code
 */
.code32

__start_bsp:
                        mov     $(LVL3L + 0x27), %ecx
                        mov     %ecx, LVL4  + PTR_SIZE * (LOAD_ADDR >> (3 * PTE_BPL + PAGE_BITS) & ((1 << PTE_BPL) - 1))
                        mov     $(LVL2L + 0x27), %ecx
                        mov     %ecx, LVL3L + PTR_SIZE * (LOAD_ADDR >> (2 * PTE_BPL + PAGE_BITS) & ((1 << PTE_BPL) - 1))

                        mov     $(LVL3H + 0x27), %ecx
                        mov     %ecx, LVL4  + PTR_SIZE * (LINK_ADDR >> (3 * PTE_BPL + PAGE_BITS) & ((1 << PTE_BPL) - 1))
                        mov     $(LVL2H + 0x27), %ecx
                        mov     %ecx, LVL3H + PTR_SIZE * (LINK_ADDR >> (2 * PTE_BPL + PAGE_BITS) & ((1 << PTE_BPL) - 1))
                        mov     $0xe3, %ecx
                        mov     $LVL2L, %edi
1:                      mov     %ecx, (%edi)
                        add     $(1 << (PTE_BPL + PAGE_BITS)), %ecx
                        add     $PTR_SIZE, %edi
                        cmp     $LOAD_E, %ecx
                        jb      1b

                        mov     $(LINK_P + 0x1e3), %ecx
                        mov     $(LVL2H + PTR_SIZE * (LINK_ADDR >> (PTE_BPL + PAGE_BITS) & ((1 << PTE_BPL) - 1))), %edi
1:                      mov     %ecx, (%edi)
                        add     $(1 << (PTE_BPL + PAGE_BITS)), %ecx
                        add     $PTR_SIZE, %edi
                        cmp     $LINK_E, %ecx
                        jb      1b

                        INIT_PAGING
                        lgdt    __boot_gdt + 2
                        ljmp    $SEL_KERN_CODE, $__start_all

/*
 * Common Startup Code
 */
.code64

__start_all:            INIT_STATE
                        test    %REG(bx), %REG(bx)
                        je      2f

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

.text

/*
 * AP Startup Code
 *
 * This code needs to be position independent. It will be copied by
 * the LAPIC code that brings up APs to low memory.
 */
.code16

.globl __start_ap
.globl __start_ap_end

__start_ap:             INIT_PAGING
                        lgdtl   %cs:__gdt_desc - __start_ap
                        ljmpl   $SEL_KERN_CODE, $__start_all

__gdt_desc:             .word   __boot_gdt__ - __boot_gdt - 1
                        .long   __boot_gdt
__start_ap_end:

/*
 * BSP Resume Code
 */
.section .init.multiboot, "ax"
.code64

.globl resume_bsp_long

resume_bsp_long:        INIT_STATE

                        call    setup_cpulocal
                        mov     %REG(ax), %REG(sp)

                        call    resume_bsp

                        call    bootstrap
                        ud2a

.text

/*
 * BSP Resume Code Real Mode Entry
 *
 * This code needs to be position independent. It will be copied by
 * the LAPIC code.
 */
.code16

.globl __resume_bsp
.globl __resume_bsp_end

__resume_bsp:           INIT_PAGING
                        lgdtl   %cs:__resume_gdt_desc - __resume_bsp
                        ljmpl   $SEL_KERN_CODE, $resume_bsp_long

__resume_gdt_desc:      .word   __boot_gdt__ - __boot_gdt - 1
                        .long   __boot_gdt

__resume_bsp_end:
