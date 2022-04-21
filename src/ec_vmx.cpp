/*
 * Execution Context
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
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

#include "console.hpp"
#include "dmar.hpp"
#include "ec.hpp"
#include "lapic.hpp"
#include "vectors.hpp"
#include "vmx.hpp"
#include "vmx_preemption_timer.hpp"

void Ec::vmx_exception()
{
    mword vect_info = Vmcs::read(Vmcs::IDT_VECT_INFO);

    if (vect_info & 0x80000000) {

        Vmcs::write(Vmcs::ENT_INTR_INFO, vect_info & ~0x1000);

        if (vect_info & 0x800)
            Vmcs::write(Vmcs::ENT_INTR_ERROR, Vmcs::read(Vmcs::IDT_VECT_ERROR));

        if ((vect_info >> 8 & 0x7) >= 4 && (vect_info >> 8 & 0x7) <= 6)
            Vmcs::write(Vmcs::ENT_INST_LEN, Vmcs::read(Vmcs::EXI_INST_LEN));
    };

    mword intr_info = Vmcs::read(Vmcs::EXI_INTR_INFO);

    switch (intr_info & 0x7ff) {

    default:
        current()->regs.dst_portal = Vmcs::VMX_EXC_NMI;
        break;

    case 0x202: // NMI
        asm volatile("int $0x2" : : : "memory");
        ret_user_vmresume();
    }

    send_msg<ret_user_vmresume>();
}

void Ec::vmx_extint()
{
    unsigned vector = Vmcs::read(Vmcs::EXI_INTR_INFO) & 0xff;

    if (vector >= VEC_IPI)
        Lapic::ipi_vector(vector);
    else if (vector >= VEC_MSI)
        Dmar::vector(vector);
    else if (vector >= VEC_LVT)
        Lapic::lvt_vector(vector);
    else if (vector >= VEC_USER)
        // This will be implemented in subsequent commits.
        Console::panic("Unimplemented user interrupt handling");

    ret_user_vmresume();
    UNREACHED;
}

void Ec::handle_vmx()
{
    // To defend against Spectre v2 other kernels would stuff the return stack
    // buffer (RSB) here to avoid the guest injecting branch targets. This is
    // not necessary for us, because we start from a fresh stack and do not
    // execute RET instructions without having a matching CALL.

    // See the corresponding check in ret_user_vmresume for the rationale of
    // manually context switching IA32_SPEC_CTRL.
    if (EXPECT_TRUE(Cpu::feature(Cpu::FEAT_IA32_SPEC_CTRL))) {
        mword const guest_spec_ctrl = Msr::read(Msr::IA32_SPEC_CTRL);

        current()->regs.spec_ctrl = guest_spec_ctrl;

        // Don't leak the guests SPEC_CTRL settings into the host and disable
        // all hardware-based mitigations.  We do this early to avoid
        // performance penalties due to enabled mitigation features.
        if (guest_spec_ctrl != 0) {
            Msr::write(Msr::IA32_SPEC_CTRL, 0);
        }
    }

    Cpu::hazard() |= HZD_DS_ES | HZD_TR;
    Cpu::setup_msrs();
    Fpu::restore_xcr0();

    mword reason = Vmcs::read(Vmcs::EXI_REASON) & 0xff;

    switch (reason) {
    case Vmcs::VMX_EXC_NMI:
        vmx_exception();
    case Vmcs::VMX_EXTINT:
        vmx_extint();
    case Vmcs::VMX_PREEMPT:
        // Whenever a preemption timer exit occurs we set the value to the
        // maximum possible. This allows to always keep the preemption
        // timer active while keeping spurious timer exits for the user to
        // a minimum in case the user does not program the timer. Not
        // re-setting the timer leads to continuous timeout exits because
        // the timer value stays at 0. Keeping the timer active all the
        // time has the advantage of minimizing high-latency VMCS updates.
        vmx_timer::set(~0ull);
        break;
    }

    current()->regs.dst_portal = reason;

    send_msg<ret_user_vmresume>();
}
