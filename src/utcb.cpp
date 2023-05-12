/*
 * User Thread Control Block (UTCB)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 *
 * Copyright (C) 2017-2018 Markus Partheymüller, Cyberus Technology GmbH.
 * Copyright (C) 2017-2018 Thomas Prescher, Cyberus Technology GmbH.
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

#include "utcb.hpp"
#include "barrier.hpp"
#include "config.hpp"
#include "cpu.hpp"
#include "ec.hpp"
#include "mtd.hpp"
#include "regs.hpp"
#include "vmx.hpp"
#include "vmx_preemption_timer.hpp"
#include "x86.hpp"

bool Utcb::load_exc(Cpu_regs* regs)
{
    mword m = regs->mtd;

    if (m & Mtd::GPR_ACDB) {
        rax = regs->rax;
        rcx = regs->rcx;
        rdx = regs->rdx;
        rbx = regs->rbx;
    }

    if (m & Mtd::GPR_BSD) {
        rbp = regs->rbp;
        rsi = regs->rsi;
        rdi = regs->rdi;
    }

    if (m & Mtd::GPR_R8_R15) {
        r8 = regs->r8;
        r9 = regs->r9;
        r10 = regs->r10;
        r11 = regs->r11;
        r12 = regs->r12;
        r13 = regs->r13;
        r14 = regs->r14;
        r15 = regs->r15;
    }

    if (m & Mtd::RSP)
        rsp = regs->rsp;

    if (m & Mtd::RIP_LEN)
        rip = regs->rip;

    if (m & Mtd::RFLAGS)
        rflags = regs->rfl;

    if (m & Mtd::QUAL) {
        qual[0] = regs->err;
        qual[1] = regs->cr2;
    }

    barrier();
    mtd = m;
    items = sizeof(Utcb_data) / sizeof(mword);

    return m & Mtd::FPU;
}

bool Utcb::save_exc(Cpu_regs* regs)
{
    if (mtd & Mtd::GPR_ACDB) {
        regs->rax = rax;
        regs->rcx = rcx;
        regs->rdx = rdx;
        regs->rbx = rbx;
    }

    if (mtd & Mtd::GPR_BSD) {
        regs->rbp = rbp;
        regs->rsi = rsi;
        regs->rdi = rdi;
    }

    if (mtd & Mtd::GPR_R8_R15) {
        regs->r8 = r8;
        regs->r9 = r9;
        regs->r10 = r10;
        regs->r11 = r11;
        regs->r12 = r12;
        regs->r13 = r13;
        regs->r14 = r14;
        regs->r15 = r15;
    }

    if (mtd & Mtd::RSP)
        regs->rsp = rsp;

    if (mtd & Mtd::RIP_LEN)
        regs->rip = rip;

    if (mtd & Mtd::RFLAGS)
        regs->rfl = (rflags & ~(Cpu::EFL_VIP | Cpu::EFL_VIF | Cpu::EFL_VM | Cpu::EFL_RF | Cpu::EFL_IOPL)) |
                    Cpu::EFL_IF;

    return mtd & Mtd::FPU;
}

void Utcb::load_vmx(Cpu_regs* regs)
{
    mword m = regs->mtd;

    if (m & Mtd::GPR_ACDB) {
        rax = regs->rax;
        rcx = regs->rcx;
        rdx = regs->rdx;
        rbx = regs->rbx;
    }

    if (m & Mtd::GPR_BSD) {
        rbp = regs->rbp;
        rsi = regs->rsi;
        rdi = regs->rdi;
    }

    if (m & Mtd::GPR_R8_R15) {
        r8 = regs->r8;
        r9 = regs->r9;
        r10 = regs->r10;
        r11 = regs->r11;
        r12 = regs->r12;
        r13 = regs->r13;
        r14 = regs->r14;
        r15 = regs->r15;
    }

    regs->vmcs->make_current();

    if (m & Mtd::RSP)
        rsp = Vmcs::read(Vmcs::GUEST_RSP);

    if (m & Mtd::RIP_LEN) {
        rip = Vmcs::read(Vmcs::GUEST_RIP);
        inst_len = Vmcs::read(Vmcs::EXI_INST_LEN);
    }

    if (m & Mtd::RFLAGS)
        rflags = Vmcs::read(Vmcs::GUEST_RFLAGS);

    if (m & Mtd::DS_ES) {
        ds.set_vmx(Vmcs::read(Vmcs::GUEST_SEL_DS), Vmcs::read(Vmcs::GUEST_BASE_DS),
                   Vmcs::read(Vmcs::GUEST_LIMIT_DS), Vmcs::read(Vmcs::GUEST_AR_DS));
        es.set_vmx(Vmcs::read(Vmcs::GUEST_SEL_ES), Vmcs::read(Vmcs::GUEST_BASE_ES),
                   Vmcs::read(Vmcs::GUEST_LIMIT_ES), Vmcs::read(Vmcs::GUEST_AR_ES));
    }

    if (m & Mtd::FS_GS) {
        fs.set_vmx(Vmcs::read(Vmcs::GUEST_SEL_FS), Vmcs::read(Vmcs::GUEST_BASE_FS),
                   Vmcs::read(Vmcs::GUEST_LIMIT_FS), Vmcs::read(Vmcs::GUEST_AR_FS));
        gs.set_vmx(Vmcs::read(Vmcs::GUEST_SEL_GS), Vmcs::read(Vmcs::GUEST_BASE_GS),
                   Vmcs::read(Vmcs::GUEST_LIMIT_GS), Vmcs::read(Vmcs::GUEST_AR_GS));
    }

    if (m & Mtd::CS_SS) {
        cs.set_vmx(Vmcs::read(Vmcs::GUEST_SEL_CS), Vmcs::read(Vmcs::GUEST_BASE_CS),
                   Vmcs::read(Vmcs::GUEST_LIMIT_CS), Vmcs::read(Vmcs::GUEST_AR_CS));
        ss.set_vmx(Vmcs::read(Vmcs::GUEST_SEL_SS), Vmcs::read(Vmcs::GUEST_BASE_SS),
                   Vmcs::read(Vmcs::GUEST_LIMIT_SS), Vmcs::read(Vmcs::GUEST_AR_SS));
    }

    if (m & Mtd::TR)
        tr.set_vmx(Vmcs::read(Vmcs::GUEST_SEL_TR), Vmcs::read(Vmcs::GUEST_BASE_TR),
                   Vmcs::read(Vmcs::GUEST_LIMIT_TR), Vmcs::read(Vmcs::GUEST_AR_TR));

    if (m & Mtd::LDTR)
        ld.set_vmx(Vmcs::read(Vmcs::GUEST_SEL_LDTR), Vmcs::read(Vmcs::GUEST_BASE_LDTR),
                   Vmcs::read(Vmcs::GUEST_LIMIT_LDTR), Vmcs::read(Vmcs::GUEST_AR_LDTR));

    if (m & Mtd::GDTR)
        gd.set_vmx(0, Vmcs::read(Vmcs::GUEST_BASE_GDTR), Vmcs::read(Vmcs::GUEST_LIMIT_GDTR), 0);

    if (m & Mtd::IDTR)
        id.set_vmx(0, Vmcs::read(Vmcs::GUEST_BASE_IDTR), Vmcs::read(Vmcs::GUEST_LIMIT_IDTR), 0);

    if (m & Mtd::CR) {
        cr0 = regs->read_cr<Vmcs>(0);
        cr2 = regs->read_cr<Vmcs>(2);
        cr3 = regs->read_cr<Vmcs>(3);
        cr4 = regs->read_cr<Vmcs>(4);
        xcr0 = regs->xcr0;
        spec_ctrl = regs->spec_ctrl;
    }

    if (m & Mtd::DR)
        dr7 = Vmcs::read(Vmcs::GUEST_DR7);

    if (m & Mtd::SYSENTER) {
        sysenter_cs = Vmcs::read(Vmcs::GUEST_SYSENTER_CS);
        sysenter_rsp = Vmcs::read(Vmcs::GUEST_SYSENTER_ESP);
        sysenter_rip = Vmcs::read(Vmcs::GUEST_SYSENTER_EIP);
    }

    if (m & Mtd::QUAL) {
        qual[0] = Vmcs::read(Vmcs::EXI_QUALIFICATION);
        qual[1] = Vmcs::read(Vmcs::INFO_PHYS_ADDR);
    }

    if (m & Mtd::INJ) {
        if (regs->dst_portal == Vmcs::VMX_FAIL_STATE || regs->dst_portal == Vmcs::VMX_POKED) {
            intr_info = static_cast<uint32>(Vmcs::read(Vmcs::ENT_INTR_INFO));
            intr_error = static_cast<uint32>(Vmcs::read(Vmcs::ENT_INTR_ERROR));
        } else {
            intr_info = static_cast<uint32>(Vmcs::read(Vmcs::EXI_INTR_INFO));
            intr_error = static_cast<uint32>(Vmcs::read(Vmcs::EXI_INTR_ERROR));
            vect_info = static_cast<uint32>(Vmcs::read(Vmcs::IDT_VECT_INFO));
            vect_error = static_cast<uint32>(Vmcs::read(Vmcs::IDT_VECT_ERROR));
        }
    }

    if (m & Mtd::STA) {
        intr_state = static_cast<uint32>(Vmcs::read(Vmcs::GUEST_INTR_STATE));
        actv_state = static_cast<uint32>(Vmcs::read(Vmcs::GUEST_ACTV_STATE));
    }

    if (m & Mtd::TSC) {
        tsc_val = rdtsc();
        tsc_off = Vmcs::read(Vmcs::TSC_OFFSET);

        mword guest_msr_area_phys = Vmcs::read(Vmcs::EXI_MSR_ST_ADDR);
        Msr_area* guest_msr_area = reinterpret_cast<Msr_area*>(Buddy::phys_to_ptr(guest_msr_area_phys));
        tsc_aux = static_cast<uint32>(guest_msr_area->ia32_tsc_aux.msr_data);
    }

    if (m & Mtd::TSC_TIMEOUT) {
        tsc_timeout = vmx_timer::get();
    }

    if (m & Mtd::EFER_PAT) {
        efer = Vmcs::read(Vmcs::GUEST_EFER);
        pat = Vmcs::read(Vmcs::GUEST_PAT);
    }

    if (m & Mtd::SYSCALL_SWAPGS) {
        mword guest_msr_area_phys = Vmcs::read(Vmcs::EXI_MSR_ST_ADDR);
        Msr_area* guest_msr_area = reinterpret_cast<Msr_area*>(Buddy::phys_to_ptr(guest_msr_area_phys));
        star = guest_msr_area->ia32_star.msr_data;
        lstar = guest_msr_area->ia32_lstar.msr_data;
        fmask = guest_msr_area->ia32_fmask.msr_data;
        kernel_gs_base = guest_msr_area->ia32_kernel_gs_base.msr_data;
    }

    if (m & Mtd::PDPTE) {
        pdpte[0] = Vmcs::read(Vmcs::GUEST_PDPTE0);
        pdpte[1] = Vmcs::read(Vmcs::GUEST_PDPTE1);
        pdpte[2] = Vmcs::read(Vmcs::GUEST_PDPTE2);
        pdpte[3] = Vmcs::read(Vmcs::GUEST_PDPTE3);
    }

    if (m & Mtd::TPR) {
        tpr_threshold = static_cast<uint32>(Vmcs::read(Vmcs::TPR_THRESHOLD));
    }

    if (m & Mtd::EOI) {
        eoi_bitmap[0] = Vmcs::read(Vmcs::EOI_EXIT_BITMAP_0);
        eoi_bitmap[1] = Vmcs::read(Vmcs::EOI_EXIT_BITMAP_1);
        eoi_bitmap[2] = Vmcs::read(Vmcs::EOI_EXIT_BITMAP_2);
        eoi_bitmap[3] = Vmcs::read(Vmcs::EOI_EXIT_BITMAP_3);
    }

    if (m & Mtd::VINTR) {
        vintr_status = static_cast<uint16>(Vmcs::read(Vmcs::GUEST_INTR_STS));
    }

    barrier();
    mtd = m;
    items = sizeof(Utcb_data) / sizeof(mword);
}

void Utcb::save_vmx(Cpu_regs* regs)
{
    if (mtd == 0) {
        return;
    }

    if (mtd & Mtd::GPR_ACDB) {
        regs->rax = rax;
        regs->rcx = rcx;
        regs->rdx = rdx;
        regs->rbx = rbx;
    }

    if (mtd & Mtd::GPR_BSD) {
        regs->rbp = rbp;
        regs->rsi = rsi;
        regs->rdi = rdi;
    }

    if (mtd & Mtd::GPR_R8_R15) {
        regs->r8 = r8;
        regs->r9 = r9;
        regs->r10 = r10;
        regs->r11 = r11;
        regs->r12 = r12;
        regs->r13 = r13;
        regs->r14 = r14;
        regs->r15 = r15;
    }

    regs->vmcs->make_current();

    if (mtd & Mtd::RSP)
        Vmcs::write(Vmcs::GUEST_RSP, rsp);

    if (mtd & Mtd::RIP_LEN) {
        Vmcs::write(Vmcs::GUEST_RIP, rip);
        Vmcs::write(Vmcs::ENT_INST_LEN, inst_len);
    }

    if (mtd & Mtd::RFLAGS)
        Vmcs::write(Vmcs::GUEST_RFLAGS, rflags);

    if (mtd & Mtd::DS_ES) {
        Vmcs::write(Vmcs::GUEST_SEL_DS, ds.sel);
        Vmcs::write(Vmcs::GUEST_BASE_DS, static_cast<mword>(ds.base));
        Vmcs::write(Vmcs::GUEST_LIMIT_DS, ds.limit);
        Vmcs::write(Vmcs::GUEST_AR_DS, (ds.ar << 4 & 0x1f000) | (ds.ar & 0xff));
        Vmcs::write(Vmcs::GUEST_SEL_ES, es.sel);
        Vmcs::write(Vmcs::GUEST_BASE_ES, static_cast<mword>(es.base));
        Vmcs::write(Vmcs::GUEST_LIMIT_ES, es.limit);
        Vmcs::write(Vmcs::GUEST_AR_ES, (es.ar << 4 & 0x1f000) | (es.ar & 0xff));
    }

    if (mtd & Mtd::FS_GS) {
        Vmcs::write(Vmcs::GUEST_SEL_FS, fs.sel);
        Vmcs::write(Vmcs::GUEST_BASE_FS, static_cast<mword>(fs.base));
        Vmcs::write(Vmcs::GUEST_LIMIT_FS, fs.limit);
        Vmcs::write(Vmcs::GUEST_AR_FS, (fs.ar << 4 & 0x1f000) | (fs.ar & 0xff));
        Vmcs::write(Vmcs::GUEST_SEL_GS, gs.sel);
        Vmcs::write(Vmcs::GUEST_BASE_GS, static_cast<mword>(gs.base));
        Vmcs::write(Vmcs::GUEST_LIMIT_GS, gs.limit);
        Vmcs::write(Vmcs::GUEST_AR_GS, (gs.ar << 4 & 0x1f000) | (gs.ar & 0xff));
    }

    if (mtd & Mtd::CS_SS) {
        Vmcs::write(Vmcs::GUEST_SEL_CS, cs.sel);
        Vmcs::write(Vmcs::GUEST_BASE_CS, static_cast<mword>(cs.base));
        Vmcs::write(Vmcs::GUEST_LIMIT_CS, cs.limit);
        Vmcs::write(Vmcs::GUEST_AR_CS, (cs.ar << 4 & 0x1f000) | (cs.ar & 0xff));
        Vmcs::write(Vmcs::GUEST_SEL_SS, ss.sel);
        Vmcs::write(Vmcs::GUEST_BASE_SS, static_cast<mword>(ss.base));
        Vmcs::write(Vmcs::GUEST_LIMIT_SS, ss.limit);
        Vmcs::write(Vmcs::GUEST_AR_SS, (ss.ar << 4 & 0x1f000) | (ss.ar & 0xff));
    }

    if (mtd & Mtd::TR) {
        Vmcs::write(Vmcs::GUEST_SEL_TR, tr.sel);
        Vmcs::write(Vmcs::GUEST_BASE_TR, static_cast<mword>(tr.base));
        Vmcs::write(Vmcs::GUEST_LIMIT_TR, tr.limit);
        Vmcs::write(Vmcs::GUEST_AR_TR, (tr.ar << 4 & 0x1f000) | (tr.ar & 0xff));
    }

    if (mtd & Mtd::LDTR) {
        Vmcs::write(Vmcs::GUEST_SEL_LDTR, ld.sel);
        Vmcs::write(Vmcs::GUEST_BASE_LDTR, static_cast<mword>(ld.base));
        Vmcs::write(Vmcs::GUEST_LIMIT_LDTR, ld.limit);
        Vmcs::write(Vmcs::GUEST_AR_LDTR, (ld.ar << 4 & 0x1f000) | (ld.ar & 0xff));
    }

    if (mtd & Mtd::GDTR) {
        Vmcs::write(Vmcs::GUEST_BASE_GDTR, static_cast<mword>(gd.base));
        Vmcs::write(Vmcs::GUEST_LIMIT_GDTR, gd.limit);
    }

    if (mtd & Mtd::IDTR) {
        Vmcs::write(Vmcs::GUEST_BASE_IDTR, static_cast<mword>(id.base));
        Vmcs::write(Vmcs::GUEST_LIMIT_IDTR, id.limit);
    }

    if (mtd & Mtd::CR) {
        regs->write_cr<Vmcs>(0, cr0);
        regs->write_cr<Vmcs>(2, cr2);
        regs->write_cr<Vmcs>(3, cr3);
        regs->write_cr<Vmcs>(4, cr4);
        regs->xcr0 = xcr0;
        regs->spec_ctrl = spec_ctrl;
    }

    if (mtd & Mtd::DR)
        Vmcs::write(Vmcs::GUEST_DR7, dr7);

    if (mtd & Mtd::SYSENTER) {
        Vmcs::write(Vmcs::GUEST_SYSENTER_CS, sysenter_cs);
        Vmcs::write(Vmcs::GUEST_SYSENTER_ESP, sysenter_rsp);
        Vmcs::write(Vmcs::GUEST_SYSENTER_EIP, sysenter_rip);
    }

    if (mtd & Mtd::CTRL) {
        regs->vmx_set_cpu_ctrl0(ctrl[0]);
        regs->vmx_set_cpu_ctrl1(ctrl[1]);
        regs->exc_bitmap = exc_bitmap;

        Vmcs::fix_cr0_mon() = cr0_mon;
        Vmcs::fix_cr4_mon() = cr4_mon;
        regs->set_exc<Vmcs>();
    }

    if (mtd & Mtd::INJ) {

        uint32 val = static_cast<uint32>(Vmcs::read(Vmcs::CPU_EXEC_CTRL0));

        if (intr_info & 0x1000)
            val |= Vmcs::CPU_INTR_WINDOW;
        else
            val &= ~Vmcs::CPU_INTR_WINDOW;

        if (intr_info & 0x2000)
            val |= Vmcs::CPU_NMI_WINDOW;
        else
            val &= ~Vmcs::CPU_NMI_WINDOW;

        regs->vmx_set_cpu_ctrl0(val);

        Vmcs::write(Vmcs::ENT_INTR_INFO, intr_info & ~0x3000);
        Vmcs::write(Vmcs::ENT_INTR_ERROR, intr_error);
    }

    if (mtd & Mtd::STA) {
        Vmcs::write(Vmcs::GUEST_INTR_STATE, intr_state);
        Vmcs::write(Vmcs::GUEST_ACTV_STATE, actv_state);
    }

    if (mtd & Mtd::TSC) {
        Vmcs::write(Vmcs::TSC_OFFSET, tsc_off);

        mword guest_msr_area_phys = Vmcs::read(Vmcs::EXI_MSR_ST_ADDR);
        Msr_area* guest_msr_area = reinterpret_cast<Msr_area*>(Buddy::phys_to_ptr(guest_msr_area_phys));
        guest_msr_area->ia32_tsc_aux.msr_data = tsc_aux;
    }

    if (mtd & Mtd::TSC_TIMEOUT) {
        vmx_timer::set(tsc_timeout);
    }

    if (mtd & Mtd::EFER_PAT) {
        regs->write_efer<Vmcs>(efer);
        Vmcs::write(Vmcs::GUEST_PAT, pat);
    }

    if (mtd & Mtd::SYSCALL_SWAPGS) {
        mword guest_msr_area_phys = Vmcs::read(Vmcs::EXI_MSR_ST_ADDR);
        Msr_area* guest_msr_area = reinterpret_cast<Msr_area*>(Buddy::phys_to_ptr(guest_msr_area_phys));
        guest_msr_area->ia32_star.msr_data = star;
        guest_msr_area->ia32_lstar.msr_data = lstar;
        guest_msr_area->ia32_fmask.msr_data = fmask;
        guest_msr_area->ia32_kernel_gs_base.msr_data = kernel_gs_base;
    }

    if (mtd & Mtd::PDPTE) {
        Vmcs::write(Vmcs::GUEST_PDPTE0, pdpte[0]);
        Vmcs::write(Vmcs::GUEST_PDPTE1, pdpte[1]);
        Vmcs::write(Vmcs::GUEST_PDPTE2, pdpte[2]);
        Vmcs::write(Vmcs::GUEST_PDPTE3, pdpte[3]);
    }

    if (mtd & Mtd::TLB) {
        regs->tlb_flush<Vmcs>(true);
    }

    if (mtd & Mtd::TPR) {
        Vmcs::write(Vmcs::TPR_THRESHOLD, tpr_threshold);
    }

    if (mtd & Mtd::EOI) {
        Vmcs::write(Vmcs::EOI_EXIT_BITMAP_0, eoi_bitmap[0]);
        Vmcs::write(Vmcs::EOI_EXIT_BITMAP_1, eoi_bitmap[1]);
        Vmcs::write(Vmcs::EOI_EXIT_BITMAP_2, eoi_bitmap[2]);
        Vmcs::write(Vmcs::EOI_EXIT_BITMAP_3, eoi_bitmap[3]);
    }

    if (mtd & Mtd::VINTR) {
        Vmcs::write(Vmcs::GUEST_INTR_STS, vintr_status);
    }
}
