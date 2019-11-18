/*
 * Hypervisor Information Page (HIP)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
 *
 * Copyright (C) 2017-2018 Florian Pester, Cyberus Technology GmbH.
 * Copyright (C) 2017-2018 Markus Partheymüller, Cyberus Technology GmbH.
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

#include "acpi.hpp"
#include "console.hpp"
#include "cpu.hpp"
#include "hip.hpp"
#include "hpet.hpp"
#include "hpt_new.hpp"
#include "ioapic.hpp"
#include "lapic.hpp"
#include "multiboot.hpp"
#include "pci.hpp"
#include "space_obj.hpp"

mword Hip::root_addr;
mword Hip::root_size;

void Hip::build (mword addr)
{
    Hip *h = hip();

    h->signature         = 0x41564f4e;
    h->cpu_offs          = reinterpret_cast<mword>(h->cpu_desc) - reinterpret_cast<mword>(h);
    h->cpu_size          = static_cast<uint16>(sizeof (Hip_cpu));
    h->ioapic_offs       = reinterpret_cast<mword>(h->ioapic_desc) - reinterpret_cast<mword>(h);
    h->ioapic_size       = static_cast<uint16>(sizeof (Hip_ioapic));
    h->mem_offs          = reinterpret_cast<mword>(h->mem_desc) - reinterpret_cast<mword>(h);
    h->mem_size          = static_cast<uint16>(sizeof (Hip_mem));
    h->api_flg           = FEAT_VMX | FEAT_SVM;
    h->api_ver           = CFG_VER;
    h->sel_num           = Space_obj::caps;
    h->sel_gsi           = NUM_GSI;
    h->sel_exc           = NUM_EXC;
    h->sel_vmi           = NUM_VMI;
    h->cfg_page          = PAGE_SIZE;
    h->cfg_utcb          = PAGE_SIZE;
    h->initial_pat       = Msr::read<uint64> (Msr::IA32_CR_PAT);
    h->msr_platform_info = Msr::read_safe<uint64> (Msr::MSR_PLATFORM_INFO);

    Hip_ioapic *ioapic = h->ioapic_desc;
    Ioapic::add_to_hip(ioapic);
    if (reinterpret_cast<mword>(ioapic) > reinterpret_cast<mword>(h->mem_desc)) {
        Console::panic("Could not add all I/O APICs to Hip!");
    }

    Multiboot *mbi = static_cast<Multiboot *>(Hpt_new::remap (addr));

    uint32 flags       = mbi->flags;
    uint32 mmap_addr   = mbi->mmap_addr;
    uint32 mmap_len    = mbi->mmap_len;
    uint32 mods_addr   = mbi->mods_addr;
    uint32 mods_count  = mbi->mods_count;

    Hip_mem *mem = h->mem_desc;

    if (flags & Multiboot::MEMORY_MAP)
        add_mem (mem, mmap_addr, mmap_len);

    if (flags & Multiboot::MODULES)
        add_mod (mem, mods_addr, mods_count);

    add_mhv (mem);

    h->length = static_cast<uint16>(reinterpret_cast<mword>(mem) - reinterpret_cast<mword>(h));
}

void Hip::add_mem (Hip_mem *&mem, mword addr, size_t len)
{
    char *mmap_addr = static_cast<char *>(Hpt_new::remap (addr));

    for (char *ptr = mmap_addr; ptr < mmap_addr + len; mem++) {

        Multiboot_mmap *map = reinterpret_cast<Multiboot_mmap *>(ptr);

        mem->addr = map->addr;
        mem->size = map->len;
        mem->type = map->type;
        mem->aux  = 0;

        ptr += map->size + 4;
    }
}

void Hip::add_mod (Hip_mem *&mem, mword addr, size_t count)
{
    Multiboot_module *mod = static_cast<Multiboot_module *>(Hpt_new::remap (addr));

    if (count) {
        root_addr = mod->s_addr;
        root_size = mod->e_addr - mod->s_addr;
    }

    for (unsigned i = 0; i < count; i++, mod++, mem++) {
        mem->addr = mod->s_addr;
        mem->size = mod->e_addr - mod->s_addr;
        mem->type = Hip_mem::MB_MODULE;
        mem->aux  = mod->cmdline;
    }
}

void Hip::add_mhv (Hip_mem *&mem)
{
    mem->addr = reinterpret_cast<mword>(&LINK_P);
    mem->size = reinterpret_cast<mword>(&LINK_E) - mem->addr;
    mem->type = Hip_mem::HYPERVISOR;
    mem++;
}

void Hip::add_cpu(Cpu_info const &cpu_info)
{
    Hip_cpu *cpu = hip()->cpu_desc + Cpu::id();

    cpu->acpi_id = Cpu::acpi_id[Cpu::id()];
    cpu->package = static_cast<uint8>(cpu_info.package);
    cpu->core    = static_cast<uint8>(cpu_info.core);
    cpu->thread  = static_cast<uint8>(cpu_info.thread);
    cpu->flags   = 1;
    cpu->lapic_info = Cpu::lapic_info[Cpu::id()];
}

void Hip::add_check()
{
    Hip *h = hip();

    h->freq_tsc = Lapic::freq_tsc;
    h->freq_bus = Lapic::freq_bus;

    h->pci_bus_start = Pci::bus_base;
    h->mcfg_base     = Pci::cfg_base;
    h->mcfg_size     = Pci::cfg_size;

    h->dmar_table    = Acpi::dmar;
    h->hpet_base     = Hpet::list == nullptr ? 0 : Hpet::list->phys;

    uint16 c = 0;
    for (uint16 const *ptr = reinterpret_cast<uint16 const *>(PAGE_H);
                       ptr < reinterpret_cast<uint16 const *>(PAGE_H + h->length);
                       c = static_cast<uint16>(c - *ptr++)) ;

    h->checksum = c;
}
