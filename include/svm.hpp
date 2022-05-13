/*
 * Secure Virtual Machine (SVM)
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

#pragma once

#include "cpulocal.hpp"
#include "utcb.hpp"

class Vmcb
{
public:
    // SVM support is currently in poor shape and disabled by default. If you want to experiment with it, you
    // can disable this.
    //
    // If you want to find places to fix, grep for this variable in the code.
    static constexpr bool DISABLE_BROKEN{true};

    union {
        char pad[1024];
        struct {
            uint32 intercept_cr;     // 0x0
            uint32 intercept_dr;     // 0x4
            uint32 intercept_exc;    // 0x8
            uint32 intercept_cpu[2]; // 0xc
            uint32 reserved1[11];    // 0x14
            uint64 base_io;          // 0x40
            uint64 base_msr;         // 0x48
            uint64 tsc_offset;       // 0x50
            uint32 asid;             // 0x58
            uint32 tlb_control;      // 0x5c
            uint64 int_control;      // 0x60
            uint64 int_shadow;       // 0x68
            uint64 exitcode;         // 0x70
            uint64 exitinfo1;        // 0x78
            uint64 exitinfo2;        // 0x80
            uint64 exitintinfo;      // 0x88
            uint64 npt_control;      // 0x90
            uint32 reserved2[4];     // 0x98
            uint64 inj_control;      // 0xa8
            uint64 npt_cr3;          // 0xb0
            uint64 lbr;              // 0xb8
        };
    };

    Utcb_segment es, cs, ss, ds, fs, gs, gdtr, ldtr, idtr, tr;
    char reserved3[48];
    uint64 efer;
    char reserved4[112];
    uint64 cr4, cr3, cr0, dr7, dr6, rflags, rip;
    char reserved5[88];
    uint64 rsp;
    char reserved6[24];
    uint64 rax, star, lstar, cstar, sfmask, kernel_gs_base;
    uint64 sysenter_cs, sysenter_esp, sysenter_eip, cr2, nrip;
    char reserved7[24];
    uint64 g_pat;

    CPULOCAL_ACCESSOR(vmcb, root);
    CPULOCAL_ACCESSOR(vmcb, asid_ctr);
    CPULOCAL_ACCESSOR(vmcb, svm_version);
    CPULOCAL_ACCESSOR(vmcb, svm_feature);

    static mword fix_cr0_set() { return 0; }
    static mword fix_cr0_clr() { return 0; }
    static mword fix_cr0_mon() { return 0; }

    static mword fix_cr4_set() { return 0; }
    static mword fix_cr4_clr() { return 0; }
    static mword fix_cr4_mon() { return 0; }

    // "SVM Intercepts" in NOVA spec.
    enum Reason
    {
        SVM_NPT_FAULT = NUM_VMI - 4,
        SVM_INVALID_STATE = NUM_VMI - 3,
    };

    enum Ctrl0
    {
        CPU_INTR = 1ul << 0,
        CPU_NMI = 1ul << 1,
        CPU_INIT = 1ul << 3,
        CPU_VINTR = 1ul << 4,
        CPU_INVD = 1ul << 22,
        CPU_HLT = 1ul << 24,
        CPU_INVLPG = 1ul << 25,
        CPU_IO = 1ul << 27,
        CPU_MSR = 1ul << 28,
        CPU_SHUTDOWN = 1ul << 31,
    };

    enum Ctrl1
    {
        CPU_VMLOAD = 1ul << 2,
        CPU_VMSAVE = 1ul << 3,
        CPU_CLGI = 1ul << 5,
        CPU_SKINIT = 1ul << 6,
    };

    static mword const force_ctrl0 =
        CPU_INTR | CPU_NMI | CPU_INIT | CPU_INVD | CPU_HLT | CPU_IO | CPU_MSR | CPU_SHUTDOWN;

    static mword const force_ctrl1 = CPU_VMLOAD | CPU_VMSAVE | CPU_CLGI | CPU_SKINIT;

    static inline void* operator new(size_t) { return Buddy::allocator.alloc(0, Buddy::FILL_0); }

    static inline void operator delete(void* ptr) { Buddy::allocator.free(reinterpret_cast<mword>(ptr)); }

    Vmcb(mword, mword);
    ~Vmcb();

    inline Vmcb() { asm volatile("vmsave %0" : : "a"(Buddy::ptr_to_phys(this)) : "memory"); }

    static bool has_npt() { return Vmcb::svm_feature() & 1; }
    static bool has_urg() { return true; }

    static void init();
};
