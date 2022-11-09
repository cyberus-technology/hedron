/*
 * Register File
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
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

#pragma once

#include "api.hpp"
#include "arch.hpp"
#include "atomic.hpp"
#include "hazards.hpp"
#include "types.hpp"

class Vmcs;

class Sys_regs
{
protected:
    static constexpr size_t ARG1_FLAGS_SHIFT{8};
    static constexpr size_t ARG1_SEL_SHIFT{12};

    static constexpr mword ARG1_ID_MASK{0xffu};
    static constexpr mword ARG1_FLAGS_MASK{0xfu};

public:
    static constexpr mword NUM_GPR = 16;

    union {
        struct {
            mword r15;
            mword r14;
            mword r13;
            mword r12;
            mword r11;
            mword r10;
            mword r9;
            mword r8;
            mword rdi;
            mword rsi;
            mword rbp;
            mword cr2;
            mword rbx;
            mword rdx;
            mword rcx;
            mword rax;
        };
        mword gpr[NUM_GPR];
    };

    enum Status
    {
        SUCCESS,
        COM_TIM,
        COM_ABT,
        BAD_HYP,
        BAD_CAP,
        BAD_PAR,
        BAD_FTR,
        BAD_CPU,
        BAD_DEV,
        OOM,
    };

    inline hypercall_id id() const { return static_cast<hypercall_id>(ARG_1 & ARG1_ID_MASK); }

    inline unsigned flags() const { return ARG_1 >> ARG1_FLAGS_SHIFT & ARG1_FLAGS_MASK; }

    inline uint8 status() { return ARG_1 & 0xffu; }

    inline void set_status(Status s, bool c = true)
    {
        if (c)
            ARG_1 = s;
        else
            ARG_1 = (ARG_1 & ~0xfful) | s;
    }

    inline void set_pt(mword pt) { ARG_1 = pt; }

    inline void set_ip(mword ip) { ARG_IP = ip; }

    inline void set_sp(mword sp) { ARG_SP = sp; }
};
static_assert(OFFSETOF(Sys_regs, cr2) == OFS_CR2);

class Exc_regs : public Sys_regs
{
public:
    union {
        struct {
            mword err;
            mword vec;
            mword rip;
            mword cs;
            mword rfl;
            mword rsp;
            mword ss;

            // The RSP on kernel entry via interrupt or exception will point
            // right here behind SS. The processor pushes SS, RSP, RFL, CS
            // and RIP as part of switching from Ring 3 to Ring 0.
            //
            // See Ec::return_to_user for the path back.
        };
        struct {
            union {
                Vmcs* vmcs;
            };

            // This member needs to have the same offset in the data
            // structure as vec above. The code uses them interchangeably.
            mword dst_portal;

            uint64 xcr0;
            mword cr0_shadow;
            mword cr3_shadow;
            mword cr4_shadow;

            mword spec_ctrl;
            uint32 exc_bitmap;
        };
    };

    // There can be no data members after the union (and specifically the ss
    // member), because entry.S assumes a fixed layout.

private:
    template <typename T> mword get_g_cr0() const;
    template <typename T> mword get_g_cr2() const;
    template <typename T> mword get_g_cr3() const;
    template <typename T> mword get_g_cr4() const;

    template <typename T> void set_g_cr0(mword) const;
    template <typename T> void set_g_cr2(mword);
    template <typename T> void set_g_cr3(mword) const;
    template <typename T> void set_g_cr4(mword) const;

    template <typename T> void set_e_bmp(uint32) const;
    template <typename T> void set_s_cr0(mword);
    template <typename T> void set_s_cr4(mword);

    template <typename T> mword cr0_set() const;
    template <typename T> mword cr0_msk(bool include_mon = false) const;
    template <typename T> mword cr4_set() const;
    template <typename T> mword cr4_msk(bool include_mon = false) const;

    template <typename T> mword get_cr0() const;
    template <typename T> mword get_cr3() const;
    template <typename T> mword get_cr4() const;

    template <typename T> void set_cr0(mword);
    template <typename T> void set_cr3(mword);
    template <typename T> void set_cr4(mword);
    template <typename T> void set_cr_masks() const;

public:
    template <typename T> void set_exc() const;

    inline bool user() const { return cs & 3; }

    void vmx_set_cpu_ctrl0(mword);
    void vmx_set_cpu_ctrl1(mword);

    mword vmx_read_gpr(unsigned);

    void vmx_write_gpr(unsigned, mword);

    template <typename T> void nst_ctrl();

    template <typename T> void tlb_flush(bool) const;

    template <typename T> mword read_cr(unsigned) const;
    template <typename T> void write_cr(unsigned, mword);

    template <typename T> void write_efer(mword);
};
static_assert(OFFSETOF(Exc_regs, vec) == OFS_VEC);
static_assert(OFFSETOF(Exc_regs, cs) == OFS_CS);
static_assert(OFFSETOF(Exc_regs, vec) == OFFSETOF(Exc_regs, dst_portal));

class Cpu_regs : public Exc_regs
{
private:
    mword hzd;

public:
    uint64 tsc_offset;
    mword mtd;
    mword fs_base;
    mword gs_base;

    inline mword hazard() const { return hzd; }

    inline void set_hazard(mword h) { Atomic::set_mask(hzd, h); }

    inline void clr_hazard(mword h) { Atomic::clr_mask(hzd, h); }

    inline void add_tsc_offset(uint64 tsc) { tsc_offset += tsc; }
};
