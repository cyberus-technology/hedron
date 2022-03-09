/*
 * Virtual Machine Extensions (VMX)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
 *
 * Copyright (C) 2017-2018 Markus Partheymüller, Cyberus Technology GmbH.
 * Copyright (C) 2017-2018 Thomas Prescher, Cyberus Technology GmbH.
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

#pragma once

#include "assert.hpp"
#include "buddy.hpp"
#include "cpulocal.hpp"
#include "msr.hpp"
#include "vmx_types.hpp"

class Ept;

class Vmcs
{
public:
    uint32 rev;
    uint32 abort;

    CPULOCAL_ACCESSOR(vmcs, current);

    // The VPID counter is used to generate VPIDs for new vCPUs.
    //
    // Because we allocate VPIDs from the CPU where it is created and not
    // where it will run, we have to be able to access this value remotely.
    CPULOCAL_REMOTE_ACCESSOR(vmcs, vpid_ctr);

    CPULOCAL_ACCESSOR(vmcs, basic);
    CPULOCAL_ACCESSOR(vmcs, ept_vpid);
    CPULOCAL_ACCESSOR(vmcs, ctrl_pin);
    CPULOCAL_ACCESSOR(vmcs, ctrl_cpu);
    CPULOCAL_ACCESSOR(vmcs, ctrl_exi);
    CPULOCAL_ACCESSOR(vmcs, ctrl_ent);

    CPULOCAL_ACCESSOR(vmcs, fix_cr0_set);
    CPULOCAL_ACCESSOR(vmcs, fix_cr0_clr);
    CPULOCAL_ACCESSOR(vmcs, fix_cr0_mon);

    CPULOCAL_ACCESSOR(vmcs, fix_cr4_set);
    CPULOCAL_ACCESSOR(vmcs, fix_cr4_clr);
    CPULOCAL_ACCESSOR(vmcs, fix_cr4_mon);

    enum Encoding
    {
        // 16-Bit Control Fields
        VPID = 0x0000ul,

        // 16-Bit Guest State Fields
        GUEST_SEL_ES = 0x0800ul,
        GUEST_SEL_CS = 0x0802ul,
        GUEST_SEL_SS = 0x0804ul,
        GUEST_SEL_DS = 0x0806ul,
        GUEST_SEL_FS = 0x0808ul,
        GUEST_SEL_GS = 0x080aul,
        GUEST_SEL_LDTR = 0x080cul,
        GUEST_SEL_TR = 0x080eul,
        GUEST_INTR_STS = 0x0810ul,

        // 16-Bit Host State Fields
        HOST_SEL_ES = 0x0c00ul,
        HOST_SEL_CS = 0x0c02ul,
        HOST_SEL_SS = 0x0c04ul,
        HOST_SEL_DS = 0x0c06ul,
        HOST_SEL_FS = 0x0c08ul,
        HOST_SEL_GS = 0x0c0aul,
        HOST_SEL_TR = 0x0c0cul,

        // 64-Bit Control Fields
        IO_BITMAP_A = 0x2000ul,
        IO_BITMAP_B = 0x2002ul,
        MSR_BITMAP = 0x2004ul,
        EXI_MSR_ST_ADDR = 0x2006ul,
        EXI_MSR_LD_ADDR = 0x2008ul,
        ENT_MSR_LD_ADDR = 0x200aul,
        VMCS_EXEC_PTR = 0x200cul,
        TSC_OFFSET = 0x2010ul,
        TSC_OFFSET_HI = 0x2011ul,
        APIC_VIRT_ADDR = 0x2012ul,
        APIC_ACCS_ADDR = 0x2014ul,
        EPTP = 0x201aul,
        EPTP_HI = 0x201bul,

        EOI_EXIT_BITMAP_0 = 0x201cul,
        EOI_EXIT_BITMAP_1 = 0x201eul,
        EOI_EXIT_BITMAP_2 = 0x2020ul,
        EOI_EXIT_BITMAP_3 = 0x2022ul,

        INFO_PHYS_ADDR = 0x2400ul,

        // 64-Bit Guest State
        VMCS_LINK_PTR = 0x2800ul,
        VMCS_LINK_PTR_HI = 0x2801ul,
        GUEST_DEBUGCTL = 0x2802ul,
        GUEST_DEBUGCTL_HI = 0x2803ul,
        GUEST_PAT = 0x2804ul,
        GUEST_EFER = 0x2806ul,
        GUEST_PERF_GLOBAL_CTRL = 0x2808ul,
        GUEST_PDPTE0 = 0x280aul,
        GUEST_PDPTE1 = 0x280cul,
        GUEST_PDPTE2 = 0x280eul,
        GUEST_PDPTE3 = 0x2810ul,

        // 64-Bit Host State
        HOST_PAT = 0x2c00ul,
        HOST_EFER = 0x2c02ul,
        HOST_PERF_GLOBAL_CTRL = 0x2c04ul,

        // 32-Bit Control Fields
        PIN_CONTROLS = 0x4000ul,
        CPU_EXEC_CTRL0 = 0x4002ul,
        EXC_BITMAP = 0x4004ul,
        PF_ERROR_MASK = 0x4006ul,
        PF_ERROR_MATCH = 0x4008ul,
        CR3_TARGET_COUNT = 0x400aul,
        EXI_CONTROLS = 0x400cul,
        EXI_MSR_ST_CNT = 0x400eul,
        EXI_MSR_LD_CNT = 0x4010ul,
        ENT_CONTROLS = 0x4012ul,
        ENT_MSR_LD_CNT = 0x4014ul,
        ENT_INTR_INFO = 0x4016ul,
        ENT_INTR_ERROR = 0x4018ul,
        ENT_INST_LEN = 0x401aul,
        TPR_THRESHOLD = 0x401cul,
        CPU_EXEC_CTRL1 = 0x401eul,

        // 32-Bit R/O Data Fields
        VMX_INST_ERROR = 0x4400ul,
        EXI_REASON = 0x4402ul,
        EXI_INTR_INFO = 0x4404ul,
        EXI_INTR_ERROR = 0x4406ul,
        IDT_VECT_INFO = 0x4408ul,
        IDT_VECT_ERROR = 0x440aul,
        EXI_INST_LEN = 0x440cul,
        EXI_INST_INFO = 0x440eul,

        // 32-Bit Guest State Fields
        GUEST_LIMIT_ES = 0x4800ul,
        GUEST_LIMIT_CS = 0x4802ul,
        GUEST_LIMIT_SS = 0x4804ul,
        GUEST_LIMIT_DS = 0x4806ul,
        GUEST_LIMIT_FS = 0x4808ul,
        GUEST_LIMIT_GS = 0x480aul,
        GUEST_LIMIT_LDTR = 0x480cul,
        GUEST_LIMIT_TR = 0x480eul,
        GUEST_LIMIT_GDTR = 0x4810ul,
        GUEST_LIMIT_IDTR = 0x4812ul,
        GUEST_AR_ES = 0x4814ul,
        GUEST_AR_CS = 0x4816ul,
        GUEST_AR_SS = 0x4818ul,
        GUEST_AR_DS = 0x481aul,
        GUEST_AR_FS = 0x481cul,
        GUEST_AR_GS = 0x481eul,
        GUEST_AR_LDTR = 0x4820ul,
        GUEST_AR_TR = 0x4822ul,
        GUEST_INTR_STATE = 0x4824ul,
        GUEST_ACTV_STATE = 0x4826ul,
        GUEST_SMBASE = 0x4828ul,
        GUEST_SYSENTER_CS = 0x482aul,
        VMX_PREEMPT_TIMER = 0x482eul,

        // 32-Bit Host State Fields
        HOST_SYSENTER_CS = 0x4c00ul,

        // Natural-Width Control Fields
        CR0_MASK = 0x6000ul,
        CR4_MASK = 0x6002ul,
        CR0_READ_SHADOW = 0x6004ul,
        CR4_READ_SHADOW = 0x6006ul,
        CR3_TARGET_0 = 0x6008ul,
        CR3_TARGET_1 = 0x600aul,
        CR3_TARGET_2 = 0x600cul,
        CR3_TARGET_3 = 0x600eul,

        // Natural-Width R/O Data Fields
        EXI_QUALIFICATION = 0x6400ul,
        IO_RCX = 0x6402ul,
        IO_RSI = 0x6404ul,
        IO_RDI = 0x6406ul,
        IO_RIP = 0x6408ul,
        GUEST_LINEAR_ADDRESS = 0x640aul,

        // Natural-Width Guest State Fields
        GUEST_CR0 = 0x6800ul,
        GUEST_CR3 = 0x6802ul,
        GUEST_CR4 = 0x6804ul,
        GUEST_BASE_ES = 0x6806ul,
        GUEST_BASE_CS = 0x6808ul,
        GUEST_BASE_SS = 0x680aul,
        GUEST_BASE_DS = 0x680cul,
        GUEST_BASE_FS = 0x680eul,
        GUEST_BASE_GS = 0x6810ul,
        GUEST_BASE_LDTR = 0x6812ul,
        GUEST_BASE_TR = 0x6814ul,
        GUEST_BASE_GDTR = 0x6816ul,
        GUEST_BASE_IDTR = 0x6818ul,
        GUEST_DR7 = 0x681aul,
        GUEST_RSP = 0x681cul,
        GUEST_RIP = 0x681eul,
        GUEST_RFLAGS = 0x6820ul,
        GUEST_PENDING_DEBUG = 0x6822ul,
        GUEST_SYSENTER_ESP = 0x6824ul,
        GUEST_SYSENTER_EIP = 0x6826ul,

        // Natural-Width Host State Fields
        HOST_CR0 = 0x6c00ul,
        HOST_CR3 = 0x6c02ul,
        HOST_CR4 = 0x6c04ul,
        HOST_BASE_FS = 0x6c06ul,
        HOST_BASE_GS = 0x6c08ul,
        HOST_BASE_TR = 0x6c0aul,
        HOST_BASE_GDTR = 0x6c0cul,
        HOST_BASE_IDTR = 0x6c0eul,
        HOST_SYSENTER_ESP = 0x6c10ul,
        HOST_SYSENTER_EIP = 0x6c12ul,
        HOST_RSP = 0x6c14ul,
        HOST_RIP = 0x6c16ul
    };

    enum Ctrl_exi
    {
        EXI_HOST_64 = 1UL << 9,
        EXI_INTA = 1UL << 15,
        EXI_SAVE_PAT = 1UL << 18,
        EXI_LOAD_PAT = 1UL << 19,
        EXI_SAVE_EFER = 1UL << 20,
        EXI_LOAD_EFER = 1UL << 21,
        EXI_SAVE_PREEMPT_TIMER = 1UL << 22,
    };

    enum Ctrl_ent
    {
        ENT_GUEST_64 = 1UL << 9,
        ENT_LOAD_PAT = 1UL << 14,
        ENT_LOAD_EFER = 1UL << 15,
    };

    enum Ctrl_pin
    {
        PIN_EXTINT = 1ul << 0,
        PIN_NMI = 1ul << 3,
        PIN_VIRT_NMI = 1ul << 5,
        PIN_PREEMPT_TIMER = 1ul << 6,
    };

    enum Ctrl0
    {
        CPU_INTR_WINDOW = 1ul << 2,
        CPU_HLT = 1ul << 7,
        CPU_INVLPG = 1ul << 9,
        CPU_CR3_LOAD = 1ul << 15,
        CPU_CR3_STORE = 1ul << 16,
        CPU_CR8_LOAD = 1ul << 19,
        CPU_CR8_STORE = 1ul << 20,
        CPU_TPR_SHADOW = 1ul << 21,
        CPU_NMI_WINDOW = 1ul << 22,
        CPU_IO = 1ul << 24,
        CPU_IO_BITMAP = 1ul << 25,
        CPU_MSR_BITMAP = 1ul << 28,
        CPU_SECONDARY = 1ul << 31,
    };

    enum Ctrl1
    {
        CPU_EPT = 1ul << 1,
        CPU_VPID = 1ul << 5,
        CPU_URG = 1ul << 7,
    };

    enum Reason
    {
        VMX_EXC_NMI = 0,
        VMX_EXTINT = 1,
        VMX_TRIPLE_FAULT = 2,
        VMX_INIT = 3,
        VMX_SIPI = 4,
        VMX_SMI_IO = 5,
        VMX_SMI_OTHER = 6,
        VMX_INTR_WINDOW = 7,
        VMX_NMI_WINDOW = 8,
        VMX_TASK_SWITCH = 9,
        VMX_CPUID = 10,
        VMX_GETSEC = 11,
        VMX_HLT = 12,
        VMX_INVD = 13,
        VMX_INVLPG = 14,
        VMX_RDPMC = 15,
        VMX_RDTSC = 16,
        VMX_RSM = 17,
        VMX_VMCALL = 18,
        VMX_VMCLEAR = 19,
        VMX_VMLAUNCH = 20,
        VMX_VMPTRLD = 21,
        VMX_VMPTRST = 22,
        VMX_VMREAD = 23,
        VMX_VMRESUME = 24,
        VMX_VMWRITE = 25,
        VMX_VMXOFF = 26,
        VMX_VMXON = 27,
        VMX_CR = 28,
        VMX_DR = 29,
        VMX_IO = 30,
        VMX_RDMSR = 31,
        VMX_WRMSR = 32,
        VMX_FAIL_STATE = 33,
        VMX_FAIL_MSR = 34,
        VMX_MWAIT = 36,
        VMX_MTF = 37,
        VMX_MONITOR = 39,
        VMX_PAUSE = 40,
        VMX_FAIL_MCHECK = 41,
        VMX_TPR_THRESHOLD = 43,
        VMX_APIC_ACCESS = 44,
        VMX_GDTR_IDTR = 46,
        VMX_LDTR_TR = 47,
        VMX_EPT_VIOLATION = 48,
        VMX_EPT_MISCONFIG = 49,
        VMX_INVEPT = 50,
        VMX_PREEMPT = 52,
        VMX_INVVPID = 53,
        VMX_WBINVD = 54,
        VMX_XSETBV = 55,

        // This is not a real VM exit, but we use it to signal VM entry
        // failures.
        VMX_FAIL_VMENTRY = NUM_VMI - 3,
    };

    static inline void* operator new(size_t) { return Buddy::allocator.alloc(0, Buddy::FILL_0); }

    static inline void operator delete(void* ptr) { Buddy::allocator.free(reinterpret_cast<mword>(ptr)); }

    Vmcs(mword, mword, mword, Ept const&, unsigned);

    /// Construct a root VMCS.
    Vmcs() : rev(basic().revision) {}

    void vmxon()
    {
        uint64 phys = Buddy::ptr_to_phys(this);

        bool ret;
        asm volatile("vmxon %1" : "=@cca"(ret) : "m"(phys) : "cc");
        assert(ret);
    }

    static void vmxoff()
    {
        bool ret;
        asm volatile("vmxoff" : "=@cca"(ret)::"cc");
        assert(ret);
    }

    inline void clear()
    {
        if (EXPECT_TRUE(current() == this))
            current() = nullptr;

        uint64 phys = Buddy::ptr_to_phys(this);

        bool ret;
        asm volatile("vmclear %1" : "=@cca"(ret) : "m"(phys) : "cc");
        assert(ret);
    }

    inline void make_current()
    {
        if (EXPECT_TRUE(current() == this))
            return;

        uint64 phys = Buddy::ptr_to_phys(current() = this);

        bool ret;
        asm volatile("vmptrld %1" : "=@cca"(ret) : "m"(phys) : "cc");
        assert(ret);
    }

    static inline mword read(Encoding enc)
    {
        mword val;
        asm volatile("vmread %1, %0" : "=rm"(val) : "r"(static_cast<mword>(enc)) : "cc");
        return val;
    }

    static inline void write(Encoding enc, mword val)
    {
        asm volatile("vmwrite %0, %1" : : "rm"(val), "r"(static_cast<mword>(enc)) : "cc");
    }

    static inline unsigned long vpid() { return has_vpid() ? read(VPID) : 0; }

    static bool has_secondary() { return ctrl_cpu()[0].clr & CPU_SECONDARY; }
    static bool has_guest_pat() { return ctrl_exi().clr & (EXI_SAVE_PAT | EXI_LOAD_PAT); }
    static bool has_ept() { return ctrl_cpu()[1].clr & CPU_EPT; }
    static bool has_vpid() { return ctrl_cpu()[1].clr & CPU_VPID; }
    static bool has_urg() { return ctrl_cpu()[1].clr & CPU_URG; }
    static bool has_vnmi() { return ctrl_pin().clr & PIN_VIRT_NMI; }
    static bool has_msr_bmp() { return ctrl_cpu()[0].clr & CPU_MSR_BITMAP; }
    static bool has_vmx_preemption_timer() { return ctrl_pin().clr & PIN_PREEMPT_TIMER; }

    /// Try to enable VMX, if it was not enabled.
    ///
    /// Returns true, if successful.
    static bool try_enable_vmx();

    static void init();
};

// A single-entry in the MSR save/load area. See struct Msr_area below.
struct Msr_entry {
    uint32 msr_index;
    uint32 reserved;
    uint64 msr_data;

    Msr_entry(uint32 index) : msr_index(index), reserved(0), msr_data(0) {}
};
static_assert(sizeof(Msr_entry) == 16, "MSR area entry does not conform to specification.");

// The MSR save/load area that is referenced from a VMCS.
//
// This struct must have a layout as it is described in the Intel SDM Vol. 3
// Section 24.8.2 "VM-Entry Controls for MSRs".
struct Msr_area {
    enum
    {
        MSR_COUNT = 5
    };

    Msr_entry ia32_star{Msr::IA32_STAR};
    Msr_entry ia32_lstar{Msr::IA32_LSTAR};
    Msr_entry ia32_fmask{Msr::IA32_FMASK};
    Msr_entry ia32_kernel_gs_base{Msr::IA32_KERNEL_GS_BASE};
    Msr_entry ia32_tsc_aux{Msr::IA32_TSC_AUX};

    static inline void* operator new(size_t)
    {
        /* allocate one page */
        return Buddy::allocator.alloc(0, Buddy::FILL_0);
    }

    static inline void destroy(Msr_area* obj) { Buddy::allocator.free(reinterpret_cast<mword>(obj)); }
};
static_assert(sizeof(Msr_area) == Msr_area::MSR_COUNT * sizeof(Msr_entry),
              "MSR area size does not match the MSR count.");
