/*
 * Central Processing Unit (CPU)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
 * Copyright (C) 2015 Alexander Boettcher, Genode Labs GmbH
 *
 * This file is part of the NOVA microhypervisor.
 *
 * Copyright (C) 2017-2019 Markus Partheymüller, Cyberus Technology GmbH.
 * Copyright (C) 2018 Thomas Prescher, Cyberus Technology GmbH.
 * Copyright (C) 2018 Stefan Hertrampf, Cyberus Technology GmbH.
 * Copyright (C) 2019 Julian Stecklina, Cyberus Technology GmbH.
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

#include "bits.hpp"
#include "cmdline.hpp"
#include "cpu.hpp"
#include "counter.hpp"
#include "fpu.hpp"
#include "gdt.hpp"
#include "hip.hpp"
#include "idt.hpp"
#include "lapic.hpp"
#include "mca.hpp"
#include "msr.hpp"
#include "pd.hpp"
#include "stdio.hpp"
#include "svm.hpp"
#include "tss.hpp"
#include "vmx.hpp"
#include "x86.hpp"

char const * const Cpu::vendor_string[] =
{
    "Unknown",
    "GenuineIntel",
    "AuthenticAMD"
};

mword       Cpu::boot_lock;

// Order of these matters
unsigned    Cpu::online;
uint8       Cpu::acpi_id[NUM_CPU];
uint8       Cpu::apic_id[NUM_CPU];
Cpu::lapic_info_t Cpu::lapic_info[NUM_CPU];

unsigned    Cpu::package;
unsigned    Cpu::core;
unsigned    Cpu::thread;

Cpu_vendor  Cpu::vendor;
unsigned    Cpu::platform;
unsigned    Cpu::family;
unsigned    Cpu::model;
unsigned    Cpu::stepping;
unsigned    Cpu::brand;
unsigned    Cpu::patch;
unsigned    Cpu::row;

uint32      Cpu::name[12];
uint32      Cpu::features[7];
bool        Cpu::bsp;
bool        Cpu::preemption;

void Cpu::check_features()
{
    unsigned top, tpp = 1, cpp = 1;

    uint32 eax, ebx, ecx, edx;

    cpuid (0, eax, ebx, ecx, edx);

    size_t v;
    for (v = sizeof (vendor_string) / sizeof (*vendor_string); --v;)
        if (*reinterpret_cast<uint32 const *>(vendor_string[v] + 0) == ebx &&
            *reinterpret_cast<uint32 const *>(vendor_string[v] + 4) == edx &&
            *reinterpret_cast<uint32 const *>(vendor_string[v] + 8) == ecx)
            break;

    vendor = Cpu_vendor (v);

    if (vendor == Cpu_vendor::INTEL) {
        Msr::write<uint64>(Msr::IA32_BIOS_SIGN_ID, 0);
        platform = static_cast<unsigned>(Msr::read<uint64>(Msr::IA32_PLATFORM_ID) >> 50) & 7;
    }

    switch (static_cast<uint8>(eax)) {
        default:
            FALL_THROUGH;
        case 0xD:
            cpuid(0xD, 1, features[6], ebx, ecx, edx);
            FALL_THROUGH;
        case 0x7 ... 0xC:
            cpuid (0x7, 0, eax, features[3], ecx, edx);
            FALL_THROUGH;
        case 0x6:
            cpuid (0x6, features[2], ebx, ecx, edx);
            FALL_THROUGH;
        case 0x4 ... 0x5:
            cpuid (0x4, 0, eax, ebx, ecx, edx);
            cpp = (eax >> 26 & 0x3f) + 1;
            FALL_THROUGH;
        case 0x1 ... 0x3:
            cpuid (0x1, eax, ebx, features[1], features[0]);
            family   = (eax >> 8 & 0xf) + (eax >> 20 & 0xff);
            model    = (eax >> 4 & 0xf) + (eax >> 12 & 0xf0);
            stepping =  eax & 0xf;
            brand    =  ebx & 0xff;
            top      =  ebx >> 24;
            tpp      =  ebx >> 16 & 0xff;
            FALL_THROUGH;
    }

    patch = static_cast<unsigned>(Msr::read<uint64>(Msr::IA32_BIOS_SIGN_ID) >> 32);

    cpuid (0x80000000, eax, ebx, ecx, edx);

    if (eax & 0x80000000) {
        switch (static_cast<uint8>(eax)) {
            default:
                cpuid (0x8000000a, Vmcb::svm_version, ebx, ecx, Vmcb::svm_feature);
                FALL_THROUGH;
            case 0x4 ... 0x9:
                cpuid (0x80000004, name[8], name[9], name[10], name[11]);
                FALL_THROUGH;
            case 0x3:
                cpuid (0x80000003, name[4], name[5], name[6], name[7]);
                FALL_THROUGH;
            case 0x2:
                cpuid (0x80000002, name[0], name[1], name[2], name[3]);
                FALL_THROUGH;
            case 0x1:
                cpuid (0x80000001, eax, ebx, features[5], features[4]);
                FALL_THROUGH;
        }
    }

    if (feature (FEAT_CMP_LEGACY))
        cpp = tpp;

    unsigned tpc = tpp / cpp;
    unsigned long t_bits = bit_scan_reverse (tpc - 1) + 1;
    unsigned long c_bits = bit_scan_reverse (cpp - 1) + 1;

    thread  = top            & ((1u << t_bits) - 1);
    core    = top >>  t_bits & ((1u << c_bits) - 1);
    package = top >> (t_bits + c_bits);

    // Disable C1E on AMD Rev.F and beyond because it stops LAPIC clock
    if (vendor == Cpu_vendor::AMD)
        if (family > 0xf || (family == 0xf && model >= 0x40))
            Msr::write (Msr::AMD_IPMR, Msr::read<uint32>(Msr::AMD_IPMR) & ~(3ul << 27));

    // Disable features based on command line arguments
    if (EXPECT_FALSE (Cmdline::nopcid))  { defeature (FEAT_PCID);  }
    if (EXPECT_FALSE (Cmdline::noxsave)) { defeature (FEAT_XSAVE); }
}

void Cpu::setup_thermal()
{
    Msr::write (Msr::IA32_THERM_INTERRUPT, 0x10);
}

void Cpu::setup_sysenter()
{
    Msr::write<mword>(Msr::IA32_STAR,  static_cast<mword>(SEL_USER_CODE) << 48 | static_cast<mword>(SEL_KERN_CODE) << 32);
    Msr::write<mword>(Msr::IA32_LSTAR, reinterpret_cast<mword>(&entry_sysenter));
    Msr::write<mword>(Msr::IA32_FMASK, Cpu::EFL_DF | Cpu::EFL_IF);
}

void Cpu::init()
{
    for (void (**func)() = &CTORS_L; func != &CTORS_C; (*func++)()) ;

    Gdt::build();
    Tss::build();

    // Initialize exception handling
    Gdt::load();
    Tss::load();
    Idt::load();

    // Initialize CPU number and check features
    check_features();

    Lapic::init();

    if (Cpu::bsp) {
        Fpu::probe();
    }

    row = Console_vga::con.spinner (id());

    Paddr phys; mword attr;
    Pd::kern->Space_mem::loc[id()] = Hptp (Hpt::current());
    Pd::kern->Space_mem::loc[id()].lookup (CPU_LOCAL_DATA, phys, attr);
    Pd::kern->Space_mem::insert (HV_GLOBAL_CPUS + id() * PAGE_SIZE, 0, Hpt::HPT_NX | Hpt::HPT_G | Hpt::HPT_W | Hpt::HPT_P, phys);
    Hpt::ord = min (Hpt::ord, feature (FEAT_1GB_PAGES) ? 26UL : 17UL);

    if (EXPECT_TRUE (feature (FEAT_ACPI)))
        setup_thermal();

    if (EXPECT_TRUE (feature (FEAT_SEP)))
        setup_sysenter();

    uint64 cr4 {get_cr4()};

    if (feature (FEAT_PCID)) { cr4 |= Cpu::CR4_PCIDE; }
    if (feature (FEAT_SMEP)) { cr4 |= Cpu::CR4_SMEP;  }
    if (feature (FEAT_SMAP)) { cr4 |= Cpu::CR4_SMAP;  }

    set_cr4 (cr4);

    // Some BIOSes don't enable VMX and lock the feature control MSR
    auto feature_ctrl = Msr::read<uint32>(Msr::IA32_FEATURE_CONTROL);
    if (!(feature_ctrl & Msr::FEATURE_LOCKED)) {
        Msr::write<uint32>(Msr::IA32_FEATURE_CONTROL, feature_ctrl | Msr::FEATURE_VMX_O_SMX | Msr::FEATURE_LOCKED);
    }

    Vmcs::init();
    Vmcb::init();

    Mca::init();

    trace (TRACE_CPU, "CORE:%x:%x:%x %x:%x:%x:%x [%x] %.48s", package, core, thread, family, model, stepping, platform, patch, reinterpret_cast<char *>(name));

    Fpu::init();

    Hip::add_cpu();

    boot_lock++;
}
