/*
 * Execution Context
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
 * Copyright (C) 2013-2018 Alexander Boettcher, Genode Labs GmbH
 *
 * Copyright (C) 2018 Stefan Hertrampf, Cyberus Technology GmbH.
 * Copyright (C) 2019 Julian Stecklina, Cyberus Technology GmbH.
 * Copyright (C) 2022 Sebastian Eydam, Cyberus Technology GmbH.
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

#include "cpulocal.hpp"
#include "fpu.hpp"
#include "lock_guard.hpp"
#include "math.hpp"
#include "mtd.hpp"
#include "pd.hpp"
#include "queue.hpp"
#include "regs.hpp"
#include "sc.hpp"
#include "si.hpp"
#include "syscall.hpp"
#include "timeout_hypercall.hpp"
#include "tss.hpp"
#include "unique_ptr.hpp"
#include "vlapic.hpp"
#include "vmx_msr_bitmap.hpp"

// Startup exception index for global ECs. Global ECs (except roottask) receive this
// exception the first time a scheduling context is bound to them.
#define EXC_STARTUP (NUM_EXC - 2)
// Recall exception index for global ECs.
#define EXC_RECALL (NUM_EXC - 1)

// Startup exception index for vCPUs. vCPUs receive this exception the first
// time a scheduling context is bound to them.
#define VMI_STARTUP (NUM_VMI - 2)
// Recall exception index for vCPUs.
#define VMI_RECALL (NUM_VMI - 1)

class Utcb;

class Ec : public Typed_kobject<Kobject::Type::EC>, public Refcount, public Queue<Sc>
{
    friend class Queue<Ec>;

private:
    void (*cont)() ALIGNED(16);
    Cpu_regs regs;
    Ec* rcap{nullptr};

    Unique_ptr<Utcb> utcb;
    Unique_ptr<Vlapic> vlapic;

    Unique_ptr<Vmx_msr_bitmap> msr_bitmap;

    // The protection domain the EC will run in.
    Refptr<Pd> pd;

    // The protection domain that holds the UTCB or vLAPIC page.
    Refptr<Pd> pd_user_page;

    Ec* partner{nullptr};
    Ec* prev{nullptr};
    Ec* next{nullptr};
    union {
        struct {
            uint16 cpu;
            uint16 glb;
        };
        uint32 xcpu;
    };
    unsigned const evt{0};
    Timeout_hypercall timeout{this};

    // Virtual Address of the UTCB in userspace.
    mword user_utcb{0};

    // Virtual Address of the vLAPIC page in userspace.
    mword user_vlapic{0};

    Fpu fpu;

    static Slab_cache cache;

    REGPARM(1)
    static void handle_exc(Exc_regs*) asm("exc_handler");

    NORETURN
    static void handle_vmx() asm("vmx_handler");

    NORETURN
    static void USED handle_svm() asm("svm_handler");

    static bool handle_exc_gp(Exc_regs*);
    static bool handle_exc_pf(Exc_regs*);

    NORETURN
    static inline void svm_exception(mword);

    NORETURN
    static inline void svm_cr();

    NORETURN
    static inline void svm_invlpg();

    NORETURN
    static inline void vmx_exception();

    NORETURN
    static inline void vmx_extint();

    NORETURN
    static inline void vmx_invlpg();

    // Try to fixup a #GP in the kernel. See FIXUP_CALL for when this may be
    // appropriate.
    //
    // Returns true, if the #GP was handled and normal operation can resume.
    static bool fixup(Exc_regs* r);

    NOINLINE
    static void handle_hazard(mword, void (*)());

    static void pre_free(Rcu_elem* a)
    {
        Ec* e = static_cast<Ec*>(a);

        assert(e);

        // Cleanup user space mappings of kernel memory.
        if (e->user_utcb) {
            e->pd_user_page->Space_mem::insert(e->user_utcb, 0, 0, 0);
            e->user_utcb = 0;
        }

        if (e->user_vlapic) {
            e->pd_user_page->Space_mem::insert(e->user_vlapic, 0, 0, 0);
            e->user_vlapic = 0;
        }
    }

    inline bool is_idle_ec() const { return cont == idle; }

    inline bool is_vcpu() const { return not utcb; }

    static void free(Rcu_elem* a)
    {
        Ec* e = static_cast<Ec*>(a);

        if (e->del_ref()) {
            assert(e != Ec::current());
            delete e;
        }
    }

    inline Sys_regs* sys_regs() { return &regs; }

    inline Exc_regs* exc_regs() { return &regs; }

    inline void set_partner(Ec* p)
    {
        partner = p;
        bool ok = partner->add_ref();
        assert(ok);
        partner->rcap = this;
        ok = partner->rcap->add_ref();
        assert(ok);
        Sc::ctr_link()++;
    }

    inline unsigned clr_partner()
    {
        assert(partner == current());
        if (partner->rcap) {
            bool last = partner->rcap->del_ref();
            assert(!last);
            partner->rcap = nullptr;
        }
        bool last = partner->del_ref();
        assert(!last);
        partner = nullptr;
        return Sc::ctr_link()--;
    }

    inline void redirect_to_iret()
    {
        regs.rsp = regs.ARG_SP;
        regs.rip = regs.ARG_IP;
    }

    void load_fpu();
    void save_fpu();

    void transfer_fpu(Ec*);

    NORETURN
    static void idle();

public:
    // Capability permission bitmask.
    enum
    {
        PERM_EC_CTRL = 1U << 0,
        PERM_CREATE_SC = 1U << 2,
        PERM_CREATE_PT = 1U << 3,

        PERM_ALL = PERM_EC_CTRL | PERM_CREATE_SC | PERM_CREATE_PT,
    };

    CPULOCAL_REMOTE_ACCESSOR(ec, current);
    CPULOCAL_ACCESSOR(ec, idle_ec);

    // Special constructor for the idle thread.
    Ec(Pd* own, unsigned c);

    enum ec_creation_flags
    {
        CREATE_VCPU = 1 << 0,
        USE_APIC_ACCESS_PAGE = 1 << 1,
        MAP_USER_PAGE_IN_OWNER = 1 << 2,
    };

    // Construct a normal execution context.
    //
    // creation_flags is a bit field of ec_creation_flags.
    Ec(Pd* own, mword sel, Pd* p, void (*f)(), unsigned c, unsigned e, mword u, mword s, int creation_flags);

    ~Ec();

    inline void add_tsc_offset(uint64 tsc) { regs.add_tsc_offset(tsc); }

    inline bool blocked() const { return next || !cont; }

    inline void set_timeout(uint64 t, Sm* s)
    {
        if (EXPECT_FALSE(t))
            timeout.enqueue(t, s);
    }

    inline void clr_timeout()
    {
        if (EXPECT_FALSE(timeout.active()))
            timeout.dequeue();
    }

    inline void set_si_regs(mword sig, mword cnt)
    {
        regs.ARG_2 = sig;
        regs.ARG_3 = cnt;
    }

    inline void save_fsgs_base()
    {
        // The kernel switched GS_BASE and KERNEL_GS_BASE on kernel entry.
        // Thus, the user applications GS_BASE value currently resides in
        // KERNEL_GS_BASE and the values will be switched again on kernel
        // exit. Therefore, we must wrap rdgsbase with swapgs in order to
        // get the correct value. This is still faster than using rdmsr
        // with KERNEL_GS_BASE directly.
        swapgs();
        regs.gs_base = rdgsbase();
        swapgs();

        regs.fs_base = rdfsbase();
    }

    inline void load_fsgs_base()
    {
        // See documentation of save_fsgs_base().
        swapgs();
        wrgsbase(regs.gs_base);
        swapgs();

        wrfsbase(regs.fs_base);
    }

    inline void make_current()
    {
        if (current() != this) {
            current()->save_fsgs_base();
            load_fsgs_base();
        }

        transfer_fpu(current());

        if (EXPECT_FALSE(current()->del_rcu()))
            Rcu::call(current());

        current() = this;

        bool ok = current()->add_ref();
        assert(ok);

        pd->make_current();
    }

    // Return to user via the current continuation.
    //
    // This function also resets the kernel stack.
    NORETURN void return_to_user();

    // Access the current EC on a remote core.
    //
    // The returned pointer stays valid until the next transition to
    // userspace.
    static Ec* remote(unsigned cpu) { return remote_load_current(cpu); }

    NOINLINE
    void help(void (*c)())
    {
        if (EXPECT_TRUE(cont != dead)) {

            current()->cont = c;

            if (EXPECT_TRUE(++Sc::ctr_loop() < 100))
                activate();

            die("Livelock");
        }
    }

    NOINLINE
    void block_sc()
    {
        {
            Lock_guard<Spinlock> guard(lock);

            if (!blocked())
                return;

            bool ok = Sc::current()->add_ref();
            assert(ok);

            enqueue(Sc::current());
        }

        Sc::schedule(true);
    }

    inline void release(void (*c)())
    {
        if (c)
            cont = c;

        Lock_guard<Spinlock> guard(lock);

        for (Sc* s; dequeue(s = head());) {
            if (EXPECT_TRUE(!s->last_ref()) || s->ec->partner) {
                s->remote_enqueue(false);
                continue;
            }

            Rcu::call(s);
        }
    }

    HOT NORETURN static void ret_user_sysexit();

    HOT NORETURN_GCC static void ret_user_iret() asm("ret_user_iret");

    NORETURN_GCC
    static void ret_user_vmresume();

    NORETURN_GCC
    static void ret_user_vmrun();

    template <Sys_regs::Status S, bool T = false> NOINLINE NORETURN static void sys_finish();

    NORETURN
    void activate();

    template <void (*)()> NORETURN static void send_msg();

    HOT NORETURN static void recv_kern();

    HOT NORETURN static void recv_user();

    HOT NORETURN static void reply(void (*)() = nullptr, Sm* = nullptr);

    HOT NORETURN static void sys_call();

    HOT NORETURN static void sys_reply();

    NORETURN
    static void sys_create_pd();

    NORETURN
    static void sys_create_ec();

    NORETURN
    static void sys_create_sc();

    NORETURN
    static void sys_create_pt();

    NORETURN
    static void sys_create_sm();

    NORETURN
    static void sys_create_kp();

    NORETURN
    static void sys_revoke();

    NORETURN
    static void sys_pd_ctrl();

    NORETURN
    static void sys_pd_ctrl_lookup();

    NORETURN
    static void sys_pd_ctrl_map_access_page();

    NORETURN
    static void sys_pd_ctrl_delegate();

    NORETURN
    static void sys_pd_ctrl_msr_access();

    NORETURN
    static void sys_ec_ctrl();

    NORETURN
    static void sys_sc_ctrl();

    NORETURN
    static void sys_pt_ctrl();

    NORETURN
    static void sys_sm_ctrl();

    NORETURN
    static void sys_assign_pci();

    NORETURN
    static void sys_assign_gsi();

    NORETURN
    static void sys_machine_ctrl();

    NORETURN
    static void sys_machine_ctrl_suspend();

    NORETURN
    static void sys_machine_ctrl_update_microcode();

    NORETURN
    static void root_invoke();

    template <bool> static void delegate();

    NORETURN
    static void dead() { die("IPC Abort"); }

    NORETURN
    static void die(char const*, Exc_regs* = &current()->regs);

    static void idl_handler();

    static inline void* operator new(size_t) { return cache.alloc(); }

    static inline void operator delete(void* ptr) { cache.free(ptr); }

    NORETURN
    static void syscall_handler() asm("syscall_handler");
};
