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
 * This file is part of the Hedron hypervisor.
 *
 * Copyright (C) 2017-2019 Markus Partheymüller, Cyberus Technology GmbH.
 * Copyright (C) 2018 Thomas Prescher, Cyberus Technology GmbH.
 * Copyright (C) 2018 Stefan Hertrampf, Cyberus Technology GmbH.
 * Copyright (C) 2019 Julian Stecklina, Cyberus Technology GmbH.
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

#include "cpu.hpp"
#include "cmdline.hpp"
#include "compiler.hpp"
#include "counter.hpp"
#include "ec.hpp"
#include "fpu.hpp"
#include "gdt.hpp"
#include "hip.hpp"
#include "idt.hpp"
#include "lapic.hpp"
#include "mca.hpp"
#include "msr.hpp"
#include "pd.hpp"
#include "stdio.hpp"
#include "tss.hpp"
#include "vcpu.hpp"
#include "vmx.hpp"
#include "x86.hpp"

char const* const Cpu::vendor_string[] = {"Unknown", "GenuineIntel"};

static bool probe_spec_ctrl()
{
    uint64 ignore;

    // Intel does not have a single CPUID bit that indicates whether SPEC_CTRL
    // is available. Instead there are three so far (Cpu::FEAT_IBRS_IBPB,
    // Cpu::FEAT_STIBP, Cpu::FEAT_SSBD). To avoid the situation where Intel
    // decides to add more bits, we just probe for the existence of the MSR.
    return Msr::read_safe(Msr::IA32_SPEC_CTRL, ignore);
}

Cpu_info Cpu::check_features()
{
    Cpu_info cpu_info{};
    unsigned top, tpp = 1, cpp = 1;

    uint32 eax, ebx, ecx, edx;

    cpuid(0, eax, ebx, ecx, edx);

    size_t v;
    for (v = sizeof(vendor_string) / sizeof(*vendor_string); --v;)
        if (*reinterpret_cast<uint32 const*>(vendor_string[v] + 0) == ebx &&
            *reinterpret_cast<uint32 const*>(vendor_string[v] + 4) == edx &&
            *reinterpret_cast<uint32 const*>(vendor_string[v] + 8) == ecx)
            break;

    cpu_info.vendor = Cpu_vendor(v);

    if (cpu_info.vendor == Cpu_vendor::INTEL) {
        Msr::write(Msr::IA32_BIOS_SIGN_ID, 0);
        cpu_info.platform = static_cast<unsigned>(Msr::read(Msr::IA32_PLATFORM_ID) >> 50) & 7;
    }

    // We only support 64-bit Intel CPUs. This means they do support PAE. For these systems, the Intel SDM
    // states that they at least support 36 bits of physical memory. See Intel SDM Vol. 3 Section 4.1.4
    // "Enumeration of Paging Features by CPUID".
    maxphyaddr_ord() = 36;

    // EAX contains the highest supported CPUID leaf. Fall through from the
    // highest supported to the lowest CPUID leaf.
    switch (static_cast<uint8>(eax)) {
    default:
        [[fallthrough]];
    case 0xD:
        cpuid(0xD, 1, features()[6], ebx, ecx, edx);
        [[fallthrough]];
    case 0x7 ... 0xC:
        cpuid(0x7, 0, eax, features()[3], ecx, features()[7]);
        [[fallthrough]];
    case 0x6:
        cpuid(0x6, features()[2], ebx, ecx, edx);
        [[fallthrough]];
    case 0x4 ... 0x5:
        cpuid(0x4, 0, eax, ebx, ecx, edx);
        cpp = (eax >> 26 & 0x3f) + 1;
        [[fallthrough]];
    case 0x1 ... 0x3:
        cpuid(0x1, eax, ebx, features()[1], features()[0]);
        cpu_info.family = (eax >> 8 & 0xf) + (eax >> 20 & 0xff);
        cpu_info.model = (eax >> 4 & 0xf) + (eax >> 12 & 0xf0);
        cpu_info.stepping = eax & 0xf;
        cpu_info.brand = ebx & 0xff;
        top = ebx >> 24;
        tpp = ebx >> 16 & 0xff;
        break;
    }

    cpu_info.patch = static_cast<unsigned>(Msr::read(Msr::IA32_BIOS_SIGN_ID) >> 32);

    cpuid(0x80000000, eax, ebx, ecx, edx);

    if (eax & 0x80000000) {
        auto& name{cpu_info.name};

        switch (static_cast<uint8>(eax)) {
        default:
            [[fallthrough]];
        case 0x8:
            cpuid(0x80000008, eax, ebx, ecx, edx);
            maxphyaddr_ord() = eax & 0xff;
            [[fallthrough]];
        case 0x4 ... 0x7:
            cpuid(0x80000004, name[8], name[9], name[10], name[11]);
            [[fallthrough]];
        case 0x3:
            cpuid(0x80000003, name[4], name[5], name[6], name[7]);
            [[fallthrough]];
        case 0x2:
            cpuid(0x80000002, name[0], name[1], name[2], name[3]);
            [[fallthrough]];
        case 0x1:
            cpuid(0x80000001, eax, ebx, features()[5], features()[4]);
            break;
        }
    }

    if (feature(FEAT_CMP_LEGACY))
        cpp = tpp;

    unsigned tpc = tpp / cpp;
    unsigned long t_bits = bit_scan_reverse(tpc - 1) + 1;
    unsigned long c_bits = bit_scan_reverse(cpp - 1) + 1;

    cpu_info.thread = top & ((1u << t_bits) - 1);
    cpu_info.core = top >> t_bits & ((1u << c_bits) - 1);
    cpu_info.package = top >> (t_bits + c_bits);

    set_feature(FEAT_IA32_SPEC_CTRL, probe_spec_ctrl());

    // Disable features based on command line arguments
    if (EXPECT_FALSE(Cmdline::nopcid)) {
        defeature(FEAT_PCID);
    }

    // Goldmont has a bug where writes to monitored cache lines might not wake up MWAIT.
    //
    // See https://lkml.org/lkml/2016/7/6/469
    if (EXPECT_FALSE(cpu_info.vendor == Cpu_vendor::INTEL and cpu_info.family == 6 and
                     cpu_info.model == 0x5c)) {
        trace(TRACE_CPU, "Disabling MONITOR/MWAIT due to CPU bug on Intel Goldmont platforms");
        defeature(FEAT_MONITOR);
    }

    return cpu_info;
}

void Cpu::update_features()
{
    set_feature(FEAT_IA32_SPEC_CTRL, probe_spec_ctrl());

    trace(TRACE_CPU, "SPEC_CTRL available: %d", feature(FEAT_IA32_SPEC_CTRL));

    // According to the Intel specification chapter 9.11.6.3 "Update in a System Supporting Intel
    // Hyperthreading Technology" microcode updates need to be loaded only once for each CPU core. This means
    // that the user is free to load them from any hyperthread on the core. The effect of the update is
    // visible for all hyperthreads, though. This means, we need to update our feature bitmap on all sibling
    // cores as well.
    auto set_sibling_features = [](unsigned long sibling_id, const Hip_cpu& cpu_desc) {
        trace(TRACE_CPU, "CPU %u:%u:%u updated CPU features", cpu_desc.package, cpu_desc.core,
              cpu_desc.thread);
        for (size_t i{0u}; i < array_size(Cpu::features()); ++i) {
            Atomic::store(Cpulocal::get_remote(static_cast<unsigned>(sibling_id)).cpu_features[i],
                          Cpu::features()[i]);
        }
    };

    Hip::for_each_sibling(Cpu::id(), set_sibling_features);
}

void Cpu::setup_thermal() { Msr::write(Msr::IA32_THERM_INTERRUPT, 0x10); }

void Cpu::setup_msrs()
{
    Msr::write(Msr::IA32_TSC_AUX, Cpu::id());

    Msr::write(Msr::IA32_STAR, static_cast<mword>(SEL_USER_CODE) << 48 | static_cast<mword>(SEL_KERN_CODE)
                                                                             << 32);

    // Given what we program into IA32_STAR above, we need to uphold the following invariants:
    static_assert(SEL_USER_CODE + 8 == SEL_USER_DATA);
    static_assert(SEL_USER_CODE + 16 == SEL_USER_CODE_L);
    static_assert(SEL_KERN_CODE + 8 == SEL_KERN_DATA);

    Msr::write(Msr::IA32_LSTAR, reinterpret_cast<mword>(&entry_sysenter));

    // RFLAGS bits TF, NT, DF, and IF need to be disabled when entering the kernel. Clearing everything else
    // is not harmful, so don't be picky here.
    Msr::write(Msr::IA32_FMASK, ~static_cast<mword>(0));
}

Optional<unsigned> Cpu::find_by_apic_id(unsigned id)
{
    for (unsigned i = 0; i < NUM_CPU; i++) {
        if (apic_id[i] == id) {
            return i;
        }
    }

    return {};
}

Cpu_info Cpu::init()
{
    // We go through this function on resume as well. We could skip over certain
    // initializations and probes, but for now it seems less error-prone to just
    // rediscover everything.

    Tss::build();
    Gdt::load();

    // The TSS might be busy on the resume path.
    Gdt::unbusy_tss();
    Tss::load();
    Idt::load();

    // Initialize CPU number and check features
    Cpu_info const cpu_info{check_features()};

    Lapic::init();

    if (Cpu::bsp()) {
        Fpu::probe();

        Hpt::set_supported_leaf_levels(feature(FEAT_1GB_PAGES) ? 3 : 2);
    }

    if (EXPECT_TRUE(feature(FEAT_ACPI)))
        setup_thermal();

    setup_msrs();

    uint64 cr4{get_cr4()};

    if (feature(FEAT_PCID)) {
        cr4 |= Cpu::CR4_PCIDE;
    }
    if (feature(FEAT_SMEP)) {
        cr4 |= Cpu::CR4_SMEP;
    }
    if (feature(FEAT_SMAP)) {
        cr4 |= Cpu::CR4_SMAP;
    }

    set_cr4(cr4);

    Vmcs::init();
    Vcpu::init();

    Mca::init(cpu_info);

    trace(TRACE_CPU, "CORE:%x:%x:%x %x:%x:%x:%x [%x] %.48s", cpu_info.package, cpu_info.core, cpu_info.thread,
          cpu_info.family, cpu_info.model, cpu_info.stepping, cpu_info.platform, cpu_info.patch,
          reinterpret_cast<char const*>(cpu_info.name));

    Fpu::init();

    return cpu_info;
}
