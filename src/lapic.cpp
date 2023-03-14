/*
 * Local Advanced Programmable Interrupt Controller (Local APIC)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
 *
 * Copyright (C) 2017-2018 Markus Partheymüller, Cyberus Technology GmbH.
 * Copyright (C) 2017-2018 Thomas Prescher, Cyberus Technology GmbH.
 * Copyright (C) 2018 Stefan Hertrampf, Cyberus Technology GmbH.
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

#include "lapic.hpp"
#include "acpi.hpp"
#include "cmdline.hpp"
#include "ec.hpp"
#include "msr.hpp"
#include "rcu.hpp"
#include "stdio.hpp"
#include "timeout.hpp"
#include "vectors.hpp"

unsigned Lapic::freq_tsc;
unsigned Lapic::freq_bus;
bool Lapic::use_tsc_timer{false};
unsigned Lapic::cpu_park_count;

static char __start_cpu_backup[128];

void Lapic::setup()
{
    Paddr apic_base = Msr::read(Msr::IA32_APIC_BASE);

    Pd::kern->claim_mmio_page(CPU_LOCAL_APIC, apic_base & ~PAGE_MASK);

    // We execute this code once on the BSP before we start to program the LAPIC. Rescue the LAPIC
    // configuration for potential use by a passthrough VMM.
    Cpu::bsp_lapic_svr = read(LAPIC_SVR);
    Cpu::bsp_lapic_lint0 = read(LAPIC_LVT_LINT0);
}

uint32 Lapic::prepare_cpu_boot(cpu_boot_type type)
{
    assert(static_cast<size_t>(__start_cpu_end - __start_cpu) < sizeof(__start_cpu_backup));

    char* const low_memory{static_cast<char*>(Hpt::remap(CPUBOOT_ADDR))};

    memcpy(__start_cpu_backup, low_memory, sizeof(__start_cpu_backup));
    memcpy(low_memory, __start_cpu, sizeof(__start_cpu_backup));

    uint32 jmp_dst = 0;

    // Setup the correct initialization function depending on the boot type.
    switch (type) {
    case cpu_boot_type::AP:
        jmp_dst = static_cast<uint32>(reinterpret_cast<mword>(__start_all));
        break;
    case cpu_boot_type::BSP:
        jmp_dst = static_cast<uint32>(reinterpret_cast<mword>(__resume_bsp));
        break;
    }

    memcpy(low_memory + __start_cpu_patch_jmp_dst, &jmp_dst, sizeof(jmp_dst));

    // Apply relocations when the hypervisor wasn't loaded at its link address.
    for (uint32 const* rel = __start_cpu_patch_rel; rel != __start_cpu_patch_rel_end; ++rel) {
        int32 ptr_val;

        memcpy(&ptr_val, low_memory + *rel, sizeof(ptr_val));
        ptr_val += PHYS_RELOCATION;
        memcpy(low_memory + *rel, &ptr_val, sizeof(ptr_val));
    }

    return CPUBOOT_ADDR;
}

void Lapic::restore_low_memory()
{
    memcpy(Hpt::remap(CPUBOOT_ADDR), __start_cpu_backup, sizeof(__start_cpu_backup));
}

void Lapic::init()
{
    Paddr apic_base = Msr::read(Msr::IA32_APIC_BASE);

    Msr::write(Msr::IA32_APIC_BASE, apic_base | 0x800);

    assert_slow(Cpu::find_by_apic_id(id()) == Optional{Cpu::id()});

    uint32 svr = read(LAPIC_SVR);
    if (!(svr & 0x100))
        write(LAPIC_SVR, svr | 0x100);

    use_tsc_timer = Cpu::feature(Cpu::FEAT_TSC_DEADLINE) && !Cmdline::nodl;

    switch (lvt_max()) {
    default:
        // This vector can be enabled by user space using irq_ctrl_assign_lvt. We keep it masked until then.
        set_lvt(LAPIC_LVT_THERM, DLV_FIXED, 0, MASKED);
        [[fallthrough]];
    case 4:
        set_lvt(LAPIC_LVT_PERFM, DLV_FIXED, VEC_LVT_PERFM);
        [[fallthrough]];
    case 3:
        set_lvt(LAPIC_LVT_ERROR, DLV_FIXED, VEC_LVT_ERROR);
        [[fallthrough]];
    case 2:
        set_lvt(LAPIC_LVT_LINT1, DLV_NMI, 0);
        [[fallthrough]];
    case 1:
        set_lvt(LAPIC_LVT_LINT0, DLV_EXTINT, 0, 1U << 16);
        [[fallthrough]];
    case 0:
        set_lvt(LAPIC_LVT_TIMER, DLV_FIXED, VEC_LVT_TIMER, 0);
        break;
    }

    write(LAPIC_TPR, 0x10);
    write(LAPIC_TMR_DCR, 0xb);

    if ((Cpu::bsp() = apic_base & 0x100)) {
        uint32 const boot_addr = prepare_cpu_boot(cpu_boot_type::AP);

        send_ipi(0, 0, DLV_INIT, DSH_EXC_SELF);

        write(LAPIC_TMR_ICR, ~0U);

        uint32 v1 = read(LAPIC_TMR_CCR);
        uint32 t1 = static_cast<uint32>(rdtsc());
        Acpi::delay(10);
        uint32 v2 = read(LAPIC_TMR_CCR);
        uint32 t2 = static_cast<uint32>(rdtsc());

        freq_tsc = (t2 - t1) / 10;
        freq_bus = (v1 - v2) / 10;

        trace(TRACE_APIC, "TSC:%u kHz BUS:%u kHz", freq_tsc, freq_bus);

        // The AP boot code needs to lie at a page boundary below 1 MB.
        assert((boot_addr & PAGE_MASK) == 0 and boot_addr < (1 << 20));

        send_ipi(0, boot_addr >> PAGE_BITS, DLV_SIPI, DSH_EXC_SELF);
        Acpi::delay(1);
        send_ipi(0, boot_addr >> PAGE_BITS, DLV_SIPI, DSH_EXC_SELF);
    }

    set_lvt(LAPIC_LVT_TIMER, DLV_FIXED, VEC_LVT_TIMER, use_tsc_timer ? 2U << 17 : 0);

    write(LAPIC_TMR_ICR, 0);

    trace(TRACE_APIC, "APIC:%#lx ID:%#x VER:%#x LVT:%#x (%s Mode)", apic_base & ~PAGE_MASK, id(), version(),
          lvt_max(), freq_bus ? "OS" : "DL");
}

void Lapic::send_ipi(unsigned cpu, unsigned vector, Delivery_mode dlv, Shorthand dsh)
{
    while (EXPECT_FALSE(read(LAPIC_ICR_LO) & 1U << 12)) {
        relax();
    }

    // Intel SDM Vol. 3 Chap. 10.6 - Issuing Interprocessor Interrupts
    // The destination shorthand can be used to send an IPI using a single write to the low doubleword of the
    // ICR, because it is used in place of the 8-bit destination field.
    if (EXPECT_TRUE(dsh == Shorthand::DSH_NONE)) {
        // If no shorthand is used, we have to write the upper part of the ICR.
        write(LAPIC_ICR_HI, Cpu::apic_id[cpu] << 24);
    }
    write(LAPIC_ICR_LO, dsh | 1U << 14 | dlv | vector);
}

void Lapic::send_self_ipi(unsigned vector)
{
    send_ipi(0 /* unused */, vector, Delivery_mode::DLV_FIXED, Shorthand::DSH_SELF);
}

void Lapic::park_all_but_self(park_fn fn)
{
    assert(Atomic::load(cpu_park_count) == 0);
    assert(Cpu::online > 0);

    Atomic::store(park_function, fn);
    Atomic::store(cpu_park_count, Cpu::online - 1);

    send_ipi(0, VEC_IPI_PRK, DLV_FIXED, DSH_EXC_SELF);

    while (Atomic::load(cpu_park_count) != 0) {
        relax();
    }

    park_function();
}

void Lapic::perfm_handler() {}

void Lapic::error_handler()
{
    write(LAPIC_ESR, 0);
    write(LAPIC_ESR, 0);
}

void Lapic::timer_handler()
{
    bool expired = (use_tsc_timer ? Msr::read(Msr::IA32_TSC_DEADLINE) : read(LAPIC_TMR_CCR)) == 0;
    if (expired)
        Timeout::check();

    Rcu::update();
}

void Lapic::lvt_vector(unsigned vector)
{
    switch (vector) {
    case VEC_LVT_TIMER:
        timer_handler();
        break;
    case VEC_LVT_ERROR:
        error_handler();
        break;
    case VEC_LVT_PERFM:
        perfm_handler();
        break;
    }

    eoi();
}

void Lapic::park_handler()
{
    park_function();

    Atomic::sub(cpu_park_count, 1U);
    shutdown();
}

void Lapic::ipi_vector(unsigned vector)
{
    switch (vector) {
    case VEC_IPI_RRQ:
        Sc::rrq_handler();
        break;
    case VEC_IPI_RKE:
        Sc::rke_handler();
        break;
    case VEC_IPI_IDL:
        Ec::idl_handler();
        break;
    case VEC_IPI_PRK:
        park_handler();
        break;
    }

    eoi();
}
