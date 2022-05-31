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

#pragma once

#include "acpi_gas.hpp"
#include "algorithm.hpp"
#include "atomic.hpp"
#include "config.hpp"
#include "cpu.hpp"
#include "extern.hpp"

struct Cpu_info;

// The description of a single CPU (hardware thread) in the HIP.
class Hip_cpu
{
public:
    uint8 flags;
    uint8 thread;
    uint8 core;
    uint8 package;
    uint8 acpi_id;
    uint8 reserved[3];
    Cpu::lapic_info_t lapic_info;
};

// A memory area that is in use when the kernel passes control to the roottask.
//
// The kind of the region is given by its type field. This field is an E820 map
// type. In addition to standard E820 memory types, the type can also be
// MB_MODULE or HYPERVISOR.
//
// MB_MODULE indicates that this memory module is a Multiboot module. HYPERVISOR
// is a physical memory region that is claimed by the kernel and cannot be used
// by userspace.
class Hip_mem
{
public:
    enum
    {
        HYPERVISOR = -1u,
        MB_MODULE = -2u
    };

    // The start address of the module in physical memory.
    uint64 addr;
    uint64 size;

    // The type of the memory region. See description above.
    uint32 type;

    // For Multiboot modules, the aux field is the physical address of a
    // C-style string. This string is the command line of the multiboot
    // module. If it is zero, there is no command line. For other types of
    // memory regions, this field is not used and must be zero.
    uint32 aux;
};

class Hip_ioapic
{
public:
    uint32 id;
    uint32 version;
    uint32 gsi_base;
    uint32 base;
};

class Hip
{
private:
    uint32 signature; // 0x0
    uint16 checksum;  // 0x4
    uint16 length;    // 0x6

    uint16 cpu_offs;    // 0x8
    uint16 cpu_size;    // 0xa
    uint16 ioapic_offs; // 0xc
    uint16 ioapic_size; // 0xe

    uint16 mem_offs; // 0x10
    uint16 mem_size; // 0x12
    uint32 api_flg;  // 0x14

    uint32 api_ver; // 0x18
    uint32 sel_num; // 0x1c

    uint32 sel_exc; // 0x20
    uint32 sel_vmi; // 0x24

    uint32 sel_gsi;  // 0x28
    uint32 cfg_page; // 0x2c

    uint32 cfg_utcb; // 0x30
    uint32 freq_tsc; // 0x34

    uint32 freq_bus;      // 0x38
    uint32 pci_bus_start; // 0x3c

    uint64 mcfg_base; // 0x40
    uint64 mcfg_size; // 0x48

    uint64 dmar_table; // 0x50
    uint64 hpet_base;  // 0x58

    uint64 cap_vmx_sec_exec; // 0x60

    uint64 xsdt_rdst_table; // 0x68

    Acpi_gas pm1a_cnt; // 0x70
    Acpi_gas pm1b_cnt; // 0x7c

    Hip_cpu cpu_desc[NUM_CPU];
    Hip_ioapic ioapic_desc[NUM_IOAPIC];
    Hip_mem mem_desc[];

public:
    enum Feature
    {
        FEAT_IOMMU = 1U << 0,
        FEAT_VMX = 1U << 1,
        FEAT_SVM = 1U << 2,
        FEAT_UEFI = 1U << 3,
    };

    static mword root_addr;
    static mword root_size;

    static inline Hip* hip() { return reinterpret_cast<Hip*>(PAGE_H); }

    static uint32 feature() { return hip()->api_flg; }

    static void set_feature(Feature f)
    {
        Atomic::set_mask(hip()->api_flg, static_cast<decltype(hip()->api_flg)>(f));
    }

    static void clr_feature(Feature f)
    {
        Atomic::clr_mask(hip()->api_flg, static_cast<decltype(hip()->api_flg)>(f));
    }

    static bool cpu_online(unsigned long cpu) { return cpu < NUM_CPU && hip()->cpu_desc[cpu].flags & 1; }

    static void set_secondary_vmx_caps(uint64 caps) { Atomic::store(hip()->cap_vmx_sec_exec, caps); }

    static void build(mword, mword);

    static void build_mbi1(Hip_mem*&, mword);

    static void build_mbi2(Hip_mem*&, mword);

    // Call the given function on a cpu descriptor.
    //
    // The cpu_id parameter is the cpu number for which the sibling cores needs to be found
    // and the function should be applied to.
    template <typename FN> static void for_each_sibling(unsigned long cpu_id, FN func)
    {
        if (cpu_id < array_size(hip()->cpu_desc)) {
            for (size_t i{0u}; i < array_size(hip()->cpu_desc); ++i) {
                if (cpu_online(i) and hip()->cpu_desc[cpu_id].package == hip()->cpu_desc[i].package and
                    hip()->cpu_desc[cpu_id].core == hip()->cpu_desc[i].core and
                    hip()->cpu_desc[cpu_id].thread != hip()->cpu_desc[i].thread) {
                    func(i, hip()->cpu_desc[i]);
                }
            }
        }
    }

    // Add a memory region description to the HIP.
    //
    // The mem parameter is an output parameter and the resulting memory map
    // item, is added there.
    template <typename T> static void add_mem(Hip_mem*& mem, T const* map);

    // Add a special memory region describing a multiboot module to the HIP.
    //
    // The aux parameter is the physical address of the command line. See
    // add_mem for the mem parameter.
    template <typename T> static void add_mod(Hip_mem*& mem, T const* mod, uint32 aux);

    // Add a special memory region describing memory claimed by the kernel
    // itself to the HIP.
    //
    // This function must be called exactly once. See add_mem for the mem parameter.
    static void add_mhv(Hip_mem*& mem);

    static void add_cpu(Cpu_info const&);

    // Finalize the HIP.
    //
    // This function adds any missing information and the checksum. Further
    // modifications to the HIP are not possible after finalizing it.
    static void finalize();
};
static_assert(sizeof(Hip) <= PAGE_SIZE, "HIP cannot be larger than one page");
