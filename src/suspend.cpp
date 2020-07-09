/*
 * ACPI Sleep State Support
 *
 * Copyright (C) 2020 Julian Stecklina, Cyberus Technology GmbH.
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
#include "acpi_facs.hpp"
#include "acpi_madt.hpp"
#include "atomic.hpp"
#include "ec.hpp"
#include "hip.hpp"
#include "hpt.hpp"
#include "ioapic.hpp"
#include "lapic.hpp"
#include "pic.hpp"
#include "suspend.hpp"
#include "vmx.hpp"
#include "x86.hpp"

void Suspend::suspend(uint8 slp_typa, uint8 slp_typb)
{
    if (not Acpi::valid_sleep_type (slp_typa, slp_typb) or
        // We don't support suspending with IOMMU yet. See issue #120.
        (Hip::feature() & Hip::FEAT_IOMMU)) {
        return;
    }

    if (Atomic::exchange(Suspend::in_progress, true)) {
        // Someone else is already trying to sleep.
        return;
    }

    // Put all CPUs in a shutdown-ready state and then park all processors
    // except the one we are running on.
    //
    // After this call, we are also going to execute on the boot page table.
    Lapic::park_all_but_self(prepare_cpu_for_suspend);

    // We are the only CPU running. Userspace cannot touch memory anymore
    // (except via DMA).

    saved_facs = Acpi::get_facs();
    Ioapic::save_all();
    Cpu::initial_tsc = rdtsc();

    // Prepare resume code. Need restore_low_memory later!
    Lapic::prepare_bsp_resume();
    Acpi::set_waking_vector(APBOOT_ADDR, Acpi::Wake_mode::REAL_MODE);

    // Flush the cache as mandated by the ACPI specification.
    wbinvd();

    // Instruct the hardware to actually turn off the power. We might run for a
    // bit afterwards, so just go into a CLI/HLT loop until the lights are out.
    Acpi::enter_sleep_state(slp_typa, slp_typb);
    shutdown();

    // Not reached.
}

void Suspend::prepare_cpu_for_suspend()
{
    // Manually context-switch to the idle EC to trigger both FPU state saving
    // and switching to the boot page table.
    Ec::idle_ec()->make_current();

    if (Hip::feature() & Hip::FEAT_VMX) {
        if (Vmcs::current()) {
            Vmcs::current()->clear();
        }

        Vmcs::vmxoff();
    }
}

void Suspend::resume_bsp()
{
    // Restore the memory that we temporarily used to store our assembly resume
    // trampoline. We have to restore it early before the LAPIC code will use
    // the same memory to host its application processor boot code.
    Lapic::restore_low_memory();

    if (Acpi_table_madt::pic_present) {
        Pic::init();
    }

    Ioapic::restore_all();
    Acpi::set_facs (saved_facs);

    Atomic::store (Suspend::in_progress, false);
}
