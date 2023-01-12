/*
 * User Thread Control Block (UTCB)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
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

#pragma once

#include "buddy.hpp"
#include "crd.hpp"
#include "math.hpp"

class Cpu_regs;

class Utcb_segment
{
public:
    uint16 sel, ar;
    uint32 limit;
    uint64 base;

    inline void set_vmx(mword s, mword b, mword l, mword a)
    {
        sel = static_cast<uint16>(s);
        ar = static_cast<uint16>((a >> 4 & 0x1f00) | (a & 0xff));
        limit = static_cast<uint32>(l);
        base = b;
    }
};

class Utcb_head
{
protected:
    mword items;

public:
    Crd xlt, del;
    mword tls;
};

class Utcb_data
{
protected:
    union {
        struct {
            mword mtd, inst_len, rip, rflags;
            uint32 intr_state, actv_state;
            union {
                struct {
                    uint32 intr_info, intr_error;
                };
                uint64 inj;
            };
            uint32 vect_info, vect_error;

            mword rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi;
            mword r8, r9, r10, r11, r12, r13, r14, r15;
            uint64 qual[2];
            uint32 ctrl[2];
            uint64 xcr0;
            mword cr0, cr2, cr3, cr4;
            mword pdpte[4];
            mword cr8, efer, pat;
            uint64 star, lstar, fmask, kernel_gs_base;
            mword dr7, sysenter_cs, sysenter_rsp, sysenter_rip;
            Utcb_segment es, cs, ss, ds, fs, gs, ld, tr, gd, id;
            uint64 tsc_val, tsc_off;

            uint32 tsc_aux, exc_bitmap;

            uint32 tpr_threshold;
            uint32 reserved2;

            uint64 eoi_bitmap[4];

            uint16 vintr_status;
            uint16 reserved_array[3];

            uint64 cr0_mon, cr4_mon;
            uint64 spec_ctrl;
            uint64 tsc_timeout;

            // If this UTCB belongs to a vCPU, this field holds the reason for the last VM exit. This field is
            // 32 bits in size, because a VMX_ENTRY_FAILURE needs 32 bits.
            uint32 exit_reason;
            uint32 reserved3;
        };

        mword data_begin;
    };
};

class Vcpu;

class Utcb : public Utcb_head, private Utcb_data
{
    // TODO: the Vcpu class needs direct access to some members of this class. Making the Vcpu a friend of
    // the Utcb is just a workaround! We should decouple the vCPU-State and the UTCB in the near future. See
    // hedron#252.
    friend class Vcpu;

private:
    static mword const words = (PAGE_SIZE - sizeof(Utcb_head)) / sizeof(mword);

public:
    WARN_UNUSED_RESULT bool load_exc(Cpu_regs*);
    WARN_UNUSED_RESULT bool load_vmx(Cpu_regs*);
    WARN_UNUSED_RESULT bool save_exc(Cpu_regs*);
    WARN_UNUSED_RESULT bool save_vmx(Cpu_regs*);

    inline mword ucnt() const { return static_cast<uint16>(items); }
    inline mword tcnt() const { return static_cast<uint16>(items >> 16); }

    inline mword ui() const { return min(words / 1, ucnt()); }
    inline mword ti() const { return min(words / 2, tcnt()); }

    inline mword& mr(mword i) { return (&data_begin)[i]; }

    NONNULL
    inline void save(Utcb* dst)
    {
        mword n = ui();

        dst->items = items;
        for (mword i = 0; i < n; i++) {
            dst->mr(i) = mr(i);
        }
    }

    inline Xfer* xfer() { return reinterpret_cast<Xfer*>(this) + PAGE_SIZE / sizeof(Xfer) - 1; }

    static inline void* operator new(size_t) { return Buddy::allocator.alloc(0, Buddy::FILL_0); }

    static inline void operator delete(void* ptr) { Buddy::allocator.free(reinterpret_cast<mword>(ptr)); }
};
