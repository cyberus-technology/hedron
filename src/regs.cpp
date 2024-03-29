/*
 * Register File
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

#include "regs.hpp"
#include "cpu.hpp"
#include "hip.hpp"
#include "vmx.hpp"
#include "vpid.hpp"

template <> mword Exc_regs::get_g_cr0<Vmcs>() const { return Vmcs::read(Vmcs::GUEST_CR0); }
template <> mword Exc_regs::get_g_cr2<Vmcs>() const { return cr2; }
template <> mword Exc_regs::get_g_cr3<Vmcs>() const { return Vmcs::read(Vmcs::GUEST_CR3); }
template <> mword Exc_regs::get_g_cr4<Vmcs>() const { return Vmcs::read(Vmcs::GUEST_CR4); }

template <> void Exc_regs::set_g_cr0<Vmcs>(mword v) const { Vmcs::write(Vmcs::GUEST_CR0, v); }
template <> void Exc_regs::set_g_cr2<Vmcs>(mword v) { cr2 = v; }
template <> void Exc_regs::set_g_cr3<Vmcs>(mword v) const { Vmcs::write(Vmcs::GUEST_CR3, v); }
template <> void Exc_regs::set_g_cr4<Vmcs>(mword v) const { Vmcs::write(Vmcs::GUEST_CR4, v); }

template <> void Exc_regs::set_e_bmp<Vmcs>(uint32 v) const { Vmcs::write(Vmcs::EXC_BITMAP, v); }
template <> void Exc_regs::set_s_cr0<Vmcs>(mword v) { Vmcs::write(Vmcs::CR0_READ_SHADOW, cr0_shadow = v); }
template <> void Exc_regs::set_s_cr4<Vmcs>(mword v) { Vmcs::write(Vmcs::CR4_READ_SHADOW, cr4_shadow = v); }

template <> void Exc_regs::set_cr_masks<Vmcs>() const
{
    Vmcs::write(Vmcs::CR0_MASK, cr0_msk<Vmcs>(true));
    Vmcs::write(Vmcs::CR4_MASK, cr4_msk<Vmcs>(true));
}

template <> void Exc_regs::tlb_flush<Vmcs>(bool full) const
{
    mword vpid = Vmcs::vpid();

    if (vpid)
        Vpid::flush(full ? Vpid::CONTEXT_GLOBAL : Vpid::CONTEXT_NOGLOBAL, vpid);
}

template <typename T> mword Exc_regs::cr0_set() const { return T::fix_cr0_set(); }

template <typename T> mword Exc_regs::cr0_msk(bool include_mon) const
{
    return T::fix_cr0_clr() | cr0_set<T>() | T::fix_cr0_mon() * include_mon;
}

template <typename T> mword Exc_regs::cr4_set() const { return T::fix_cr4_set(); }

template <typename T> mword Exc_regs::cr4_msk(bool include_mon) const
{
    return T::fix_cr4_clr() | cr4_set<T>() | T::fix_cr4_mon() * include_mon;
}

template <typename T> mword Exc_regs::get_cr0() const
{
    mword msk = cr0_msk<T>();

    return (get_g_cr0<T>() & ~msk) | (cr0_shadow & msk);
}

template <typename T> mword Exc_regs::get_cr3() const { return get_g_cr3<T>(); }

template <typename T> mword Exc_regs::get_cr4() const
{
    mword msk = cr4_msk<T>();

    return (get_g_cr4<T>() & ~msk) | (cr4_shadow & msk);
}

template <typename T> void Exc_regs::set_cr0(mword v)
{
    set_g_cr0<T>((v & (~cr0_msk<T>() | Cpu::CR0_PE)) | (cr0_set<T>() & ~Cpu::CR0_PE));
    set_s_cr0<T>(v);
}

template <typename T> void Exc_regs::set_cr3(mword v) { set_g_cr3<T>(v); }

template <typename T> void Exc_regs::set_cr4(mword v)
{
    set_g_cr4<T>((v & ~cr4_msk<T>()) | cr4_set<T>());
    set_s_cr4<T>(v);
}

template <typename T> void Exc_regs::set_exc() const
{
    unsigned msk = 1UL << Cpu::EXC_AC;

    msk |= exc_bitmap;

    set_e_bmp<T>(msk);

    set_cr_masks<T>();
}

void Exc_regs::vmx_set_cpu_ctrl0(mword val, const bool passthrough_vcpu)
{
    val |= Vmcs::ctrl_cpu()[0].set;
    val |= passthrough_vcpu ? 0 : Vmcs::ctrl_cpu()[0].non_passthrough_set;
    val &= Vmcs::ctrl_cpu()[0].clr;

    bool tpr_shadow_active = val & Vmcs::Ctrl0::CPU_TPR_SHADOW;

    if (not tpr_shadow_active) {
        val |= Vmcs::Ctrl0::CPU_CR8_LOAD | Vmcs::Ctrl0::CPU_CR8_STORE;
    }

    Vmcs::write(Vmcs::CPU_EXEC_CTRL0, val);
}

void Exc_regs::vmx_set_cpu_ctrl1(mword val, const bool passthrough_vcpu)
{
    unsigned const msk = Vmcs::CPU_EPT;

    val |= msk;

    val |= Vmcs::ctrl_cpu()[1].set;
    val |= passthrough_vcpu ? 0 : Vmcs::ctrl_cpu()[1].non_passthrough_set;
    val &= Vmcs::ctrl_cpu()[1].clr;

    Vmcs::write(Vmcs::CPU_EXEC_CTRL1, val);
}

template <> void Exc_regs::nst_ctrl<Vmcs>(const bool passthrough_vcpu)
{
    assert(Vmcs::current() == vmcs);

    mword cr0 = get_cr0<Vmcs>();
    mword cr3 = get_cr3<Vmcs>();
    mword cr4 = get_cr4<Vmcs>();
    set_cr0<Vmcs>(cr0);
    set_cr3<Vmcs>(cr3);
    set_cr4<Vmcs>(cr4);

    vmx_set_cpu_ctrl0(Vmcs::read(Vmcs::CPU_EXEC_CTRL0), passthrough_vcpu);
    vmx_set_cpu_ctrl1(Vmcs::read(Vmcs::CPU_EXEC_CTRL1), passthrough_vcpu);
    set_exc<Vmcs>();
}

mword Exc_regs::vmx_read_gpr(unsigned reg)
{
    if (EXPECT_FALSE(reg == 4))
        return Vmcs::read(Vmcs::GUEST_RSP);
    else
        return gpr[sizeof(Sys_regs) / sizeof(mword) - 1 - reg];
}

void Exc_regs::vmx_write_gpr(unsigned reg, mword val)
{
    if (EXPECT_FALSE(reg == 4))
        Vmcs::write(Vmcs::GUEST_RSP, val);
    else
        gpr[sizeof(Sys_regs) / sizeof(mword) - 1 - reg] = val;
}

template <typename T> mword Exc_regs::read_cr(unsigned cr) const
{
    switch (cr) {
    case 0:
        return get_cr0<T>();
    case 2:
        return get_g_cr2<T>();
    case 3:
        return get_cr3<T>();
    case 4:
        return get_cr4<T>();
    default:
        UNREACHED;
    }
}

template <typename T> void Exc_regs::write_cr(unsigned cr, mword val)
{
    switch (cr) {

    case 0:
        set_cr0<T>(val);
        break;

    case 2:
        set_g_cr2<T>(val);
        break;

    case 3:
        set_cr3<T>(val);
        break;

    case 4:
        set_cr4<T>(val);
        break;

    default:
        UNREACHED;
    }
}

template <> void Exc_regs::write_efer<Vmcs>(mword val)
{
    Vmcs::write(Vmcs::GUEST_EFER, val);

    if (val & Cpu::EFER_LMA)
        Vmcs::write(Vmcs::ENT_CONTROLS, Vmcs::read(Vmcs::ENT_CONTROLS) | Vmcs::ENT_GUEST_64);
    else
        Vmcs::write(Vmcs::ENT_CONTROLS, Vmcs::read(Vmcs::ENT_CONTROLS) & ~Vmcs::ENT_GUEST_64);
}

template mword Exc_regs::read_cr<Vmcs>(unsigned) const;
template void Exc_regs::write_cr<Vmcs>(unsigned, mword);
template void Exc_regs::set_exc<Vmcs>() const;
