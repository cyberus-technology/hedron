/*
 * Virtual CPU
 *
 * Copyright (C) 2022 Sebastian Eydam, Cyberus Technology GmbH.
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

#include "vcpu.hpp"

#include "cpu.hpp"
#include "hip.hpp"
#include "space_obj.hpp"

INIT_PRIORITY(PRIO_SLAB)
Slab_cache Vcpu::cache(sizeof(Vcpu), 32);

Vcpu::Vcpu(const Vcpu_init_config& init_cfg)
    : Typed_kobject(static_cast<Space_obj*>(init_cfg.owner_pd), init_cfg.cap_selector, Vcpu::PERM_ALL, free),
      pd(init_cfg.owner_pd), kp_vcpu_state(init_cfg.kp_vcpu_state), kp_vlapic_page(init_cfg.kp_vlapic_page),
      kp_fpu_state(init_cfg.kp_fpu_state), cpu_id(Cpu::id()), fpu(kp_fpu_state.get())
{
    assert(Hip::feature() & Hip::FEAT_VMX);

    // Vcpu::run has to do the following things:
    // - set a proper RSP
    // - set the host CR3
    // - Ec::regs.nst_ctrl<VMCS>();

    const mword io_bitmap{pd->Space_pio::walk()};
    vmcs = make_unique<Vmcs>(0, io_bitmap, 0, pd->ept, Cpu::id());

    // We restore the host MSRs in the VM Exit path, thus the VM Exit shouldn't restore any MSRs
    Vmcs::write(Vmcs::EXI_MSR_LD_ADDR, 0);
    Vmcs::write(Vmcs::EXI_MSR_LD_CNT, 0);

    // Allocate and register the guest MSR area, i.e. the area to load MSRs from during a VM Entry and to
    // store MSRs to during a VM Exit.
    guest_msr_area = make_unique<Msr_area>();
    const mword guest_msr_area_phys = Buddy::ptr_to_phys(guest_msr_area.get());
    Vmcs::write(Vmcs::ENT_MSR_LD_ADDR, guest_msr_area_phys);
    Vmcs::write(Vmcs::ENT_MSR_LD_CNT, Msr_area::MSR_COUNT);
    Vmcs::write(Vmcs::EXI_MSR_ST_ADDR, guest_msr_area_phys);
    Vmcs::write(Vmcs::EXI_MSR_ST_CNT, Msr_area::MSR_COUNT);

    // Allocate and configure a default MSR bitmap.
    msr_bitmap = make_unique<Vmx_msr_bitmap>();

    static const Msr::Register guest_accessible_msrs[] = {
        Msr::Register::IA32_FS_BASE,
        Msr::Register::IA32_GS_BASE,
        Msr::Register::IA32_KERNEL_GS_BASE,
        Msr::Register::IA32_TSC_AUX,

        // This is a read-only MSR that indicates which CPU vulnerability mitigations are not required.
        // This register should be configurable by userspace. See #124.
        Msr::Register::IA32_ARCH_CAP,

        // This is a read-write register that toggles speculation-related features on the current hyperthread.
        // This register is context-switched. See vmresume for why this doesn't happen via guest_msr_area.
        Msr::Register::IA32_SPEC_CTRL,

        // This is a write-only MSR without state that can be used to issue commands to the branch predictor.
        // So far this is used to trigger barriers for indirect branch prediction (see IBPB).
        Msr::Register::IA32_PRED_CMD,

        // This is a write-only MSR without state that can be used to invalidate CPU structures. This is (so
        // far) only used to flush the L1D cache.
        Msr::Register::IA32_FLUSH_CMD,
    };

    for (auto msr : guest_accessible_msrs) {
        msr_bitmap->set_exit(msr, Vmx_msr_bitmap::exit_setting::EXIT_NEVER);
    }

    Vmcs::write(Vmcs::MSR_BITMAP, msr_bitmap->phys_addr());

    // Register the virtual LAPIC page.
    Vmcs::write(Vmcs::APIC_VIRT_ADDR, Buddy::ptr_to_phys(kp_vlapic_page->data_page()));

    // Register the APIC access page
    Vmcs::write(Vmcs::APIC_ACCS_ADDR, Buddy::ptr_to_phys(pd->get_access_page()));

    vmcs->clear();
}
