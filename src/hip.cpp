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
 * This file is part of the Hedron hypervisor.
 *
 * Hedron is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Hedron is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 */

#include "hip.hpp"
#include "acpi.hpp"
#include "acpi_rsdp.hpp"
#include "console.hpp"
#include "cpu.hpp"
#include "hpt.hpp"
#include "lapic.hpp"
#include "multiboot.hpp"
#include "multiboot2.hpp"
#include "pci.hpp"
#include "space_obj.hpp"
#include "stdio.hpp"

mword Hip::root_addr;
mword Hip::root_size;

template <typename T> static void assert_in_hip(T const* ptr)
{
    void const* ptr_start{ptr};
    void const* ptr_end{reinterpret_cast<char const*>(ptr) + sizeof(T)};

    assert(ptr_start >= PAGE_H);
    assert(ptr_end <= &PAGE_H[PAGE_SIZE]);
}

void Hip::build(mword magic, mword addr)
{
    Hip* h = hip();

    h->signature = 0x4e524448;
    h->cpu_offs = reinterpret_cast<mword>(h->cpu_desc) - reinterpret_cast<mword>(h);
    h->cpu_size = static_cast<uint16>(sizeof(Hip_cpu));
    h->ioapic_offs = reinterpret_cast<mword>(h->ioapic_desc) - reinterpret_cast<mword>(h);
    h->ioapic_size = static_cast<uint16>(sizeof(Hip_ioapic));
    h->mem_offs = reinterpret_cast<mword>(h->mem_desc) - reinterpret_cast<mword>(h);
    h->mem_size = static_cast<uint16>(sizeof(Hip_mem));
    // Other flags may have been added already earlier in the boot process, so
    // we preserve them. These flags will be modified again when the processor
    // initialization finds certain features to be missing/unusable.
    h->api_flg |= FEAT_VMX;
    h->api_ver = CFG_VER;
    h->sel_num = Space_obj::caps;
    h->sel_exc = NUM_EXC;
    h->sel_vmi = NUM_VMI;
    h->cfg_page = PAGE_SIZE;
    h->cfg_utcb = PAGE_SIZE;

    Hip_mem* mem = h->mem_desc;

    switch (magic) {
    case Multiboot::MAGIC:
        build_mbi1(mem, addr);
        break;
    case Multiboot2::MAGIC:
        build_mbi2(mem, addr);
        break;
    default:
        panic("Unknown multiboot magic number");
        break;
    }

    add_mhv(mem);

    h->length = static_cast<uint16>(reinterpret_cast<mword>(mem) - reinterpret_cast<mword>(h));

    h->num_user_vectors = ~0u;
    h->freq_bus = ~0u;
    h->pci_bus_start = ~0u;
    h->hip_base = ~0ull;
    h->bsp_lapic_svr = ~0u;
    h->bsp_lapic_lint0 = ~0u;

    memset(&h->ioapic_desc[0].deprecated, ~0, sizeof(Hip_ioapic) * 16);
}

void Hip::build_mbi1(Hip_mem*& mem, mword addr)
{
    Multiboot const* mbi = static_cast<Multiboot const*>(Hpt::remap(addr));

    uint32 flags = mbi->flags;
    uint32 mmap_addr = mbi->mmap_addr;
    uint32 mmap_len = mbi->mmap_len;
    uint32 mods_addr = mbi->mods_addr;
    uint32 mods_count = mbi->mods_count;

    if (flags & Multiboot::MEMORY_MAP) {
        char const* remap = static_cast<char const*>(Hpt::remap(mmap_addr));
        mbi->for_each_mem(remap, mmap_len, [&mem](Multiboot_mmap const* mmap) { Hip::add_mem(mem, mmap); });
    }

    if (flags & Multiboot::MODULES) {
        Multiboot_module* mod = static_cast<Multiboot_module*>(Hpt::remap(mods_addr));
        for (unsigned i = 0; i < mods_count; i++, mod++)
            add_mod(mem, mod, mod->cmdline);
    }
}

void Hip::build_mbi2(Hip_mem*& mem, mword addr)
{
    Multiboot2::Header const* mbi = static_cast<Multiboot2::Header const*>(Hpt::remap(addr));

    mbi->for_each_tag([&mem, mbi, addr](Multiboot2::Tag const* tag) {
        if (tag->type == Multiboot2::TAG_MEMORY)
            tag->for_each_mem([&mem](Multiboot2::Memory_map const* mmap) { Hip::add_mem(mem, mmap); });

        if (tag->type == Multiboot2::TAG_MODULE) {
            // MBI2 embeds command line directly without the indirection of a pointer, so
            // we need to calculate the physical address.
            auto cmdline_offset =
                reinterpret_cast<mword>(tag->module()->string) - reinterpret_cast<mword>(mbi);
            auto cmdline_phys_addr = static_cast<uint32>(addr + cmdline_offset);
            Hip::add_mod(mem, tag->module(), cmdline_phys_addr);
        }

        if (tag->type == Multiboot2::TAG_EFI_ST) {
            set_feature(FEAT_UEFI);
        }
    });
}

template <typename T> void Hip::add_mod(Hip_mem*& mem, T const* mod, uint32 aux)
{
    assert_in_hip(mem);

    if (!root_addr) {
        root_addr = mod->s_addr;
        root_size = mod->e_addr - mod->s_addr;
    }

    mem->addr = mod->s_addr;
    mem->size = mod->e_addr - mod->s_addr;
    mem->type = Hip_mem::MB_MODULE;
    mem->aux = aux;
    mem++;
}

template <typename T> void Hip::add_mem(Hip_mem*& mem, T const* map)
{
    assert_in_hip(mem);

    mem->addr = map->addr;
    mem->size = map->len;
    mem->type = map->type;
    mem->aux = 0;
    mem++;
}

void Hip::add_mhv(Hip_mem*& mem)
{
    assert_in_hip(mem);

    // We describe memory that is used by the hypervisor. These memory
    // descriptors need to match Pd::Pd (the constructor of the hypervisor PD)
    // and our linker script.

    mem->addr = LOAD_ADDR + PHYS_RELOCATION;
    mem->size = reinterpret_cast<mword>(&LOAD_END) - LOAD_ADDR;
    mem->type = Hip_mem::HYPERVISOR;
    mem++;
}

void Hip::add_cpu(Cpu_info const& cpu_info)
{
    Hip_cpu* cpu = hip()->cpu_desc + Cpu::id();

    cpu->acpi_id = Cpu::acpi_id[Cpu::id()];
    cpu->apic_id = Cpu::apic_id[Cpu::id()];
    cpu->package = static_cast<uint8>(cpu_info.package);
    cpu->core = static_cast<uint8>(cpu_info.core);
    cpu->thread = static_cast<uint8>(cpu_info.thread);
    cpu->flags = 1;
}

void Hip::finalize()
{
    Hip* h = hip();

    h->freq_tsc = Lapic::freq_tsc;

    h->mcfg_base = Pci::cfg_base;
    h->mcfg_size = Pci::cfg_size;

    h->dmar_table = Acpi::dmar;

    // Userspace needs to read the table's signature to figure out what it got.
    h->xsdt_rdst_table = Acpi::xsdt ? Acpi::xsdt : Acpi::rsdt;

    h->pm1a_cnt = Acpi::pm1a_cnt;
    h->pm1b_cnt = Acpi::pm1b_cnt;

    uint16 c = 0;
    for (uint16 const* ptr = reinterpret_cast<uint16 const*>(PAGE_H);
         ptr < reinterpret_cast<uint16 const*>(PAGE_H + h->length); c = static_cast<uint16>(c - *ptr++))
        ;

    h->checksum = c;
}
