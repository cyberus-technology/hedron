/*
 * Linker Script
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
 *
 * Copyright (C) 2017-2018 Thomas Prescher, Cyberus Technology GmbH.
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

#include "memory.hpp"

OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH("i386:x86-64")
ENTRY(__start_bsp)

PHDRS
{
    init    PT_LOAD;
    kern    PT_LOAD;
}

SECTIONS
{
    /*
     * The hypervisor is loaded at LOAD_ADDR (subject to relocation by the boot loader).
     * During the boot process, we set up page tables to map LINK_ADDR to LOAD_ADDR.
     */

    .init LOAD_ADDR :
    {
        *(.init.multiboot)
        *(.init)
    } : init = 0x90909090
    . = ALIGN(4K);

    PROVIDE (LOAD_E = .);

    INIT_LEN = LOAD_E - LOAD_ADDR;

    .text LINK_ADDR + INIT_LEN : AT (LOAD_ADDR + INIT_LEN)
    {
        *(.text.hot .text.hot.*)
        *(.text .text.* .gnu.linkonce.t.*)
    } : kern = 0x90909090

    .rodata : AT (VIRT_TO_PHYS_NORELOC(ADDR (.rodata)))
    {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
    } : kern

    .fixup : AT (VIRT_TO_PHYS_NORELOC(ADDR (.fixup)))
    {
        PROVIDE (FIXUP_S = .);
        KEEP (*(.fixup))
        PROVIDE (FIXUP_E = .);
    } : kern

    .init_array : AT (VIRT_TO_PHYS_NORELOC(ADDR (.init_array)))
    {
        PROVIDE (CTORS_C = .);
        KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.65534) SORT_BY_INIT_PRIORITY(.ctors.00001)))
        PROVIDE (CTORS_G = .);
        KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP (*(.init_array .ctors))
        PROVIDE (CTORS_E = .);
    } : kern

    .data : AT (VIRT_TO_PHYS_NORELOC(ADDR (.data)))
    {
        *(.data .data.* .gnu.linkonce.d.*)
    } : kern

    .bss : AT (VIRT_TO_PHYS_NORELOC(ADDR (.bss)))
    {
        *(.bss .bss.* .gnu.linkonce.b.*)

        . = ALIGN(4K);
        _mempool_l = .;

        PROVIDE (PAGE_0 = .); . += 4K;
        PROVIDE (PAGE_1 = .); . += 4K;
        PROVIDE (PAGE_H = .); . += 4K;

        PROVIDE (PDBRV = .);
        PROVIDE (PDBR  = VIRT_TO_PHYS_NORELOC(.));
        PROVIDE (LVL4  = VIRT_TO_PHYS_NORELOC(.)); . += 4K;
        PROVIDE (LVL3L = VIRT_TO_PHYS_NORELOC(.)); . += 4K;
        PROVIDE (LVL3H = VIRT_TO_PHYS_NORELOC(.)); . += 4K;
        PROVIDE (LVL2L = VIRT_TO_PHYS_NORELOC(.)); . += 4K;
        PROVIDE (LVL2H = VIRT_TO_PHYS_NORELOC(.)); . += 4K;
        _mempool_f = .;

        . += CONFIG_KERNEL_MEMORY;
        . = ALIGN(4M);

        PROVIDE (LOAD_END = VIRT_TO_PHYS_NORELOC(.));

        PROVIDE (_mempool_e = .);

    } : kern

    /DISCARD/ :
    {
        *(.note.GNU-stack)
        *(.comment*)
    }
}
