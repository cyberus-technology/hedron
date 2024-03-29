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
#include "fpu.hpp"
#include "lock_guard.hpp"
#include "math.hpp"
#include "mtd.hpp"
#include "pd.hpp"
#include "queue.hpp"
#include "regs.hpp"
#include "sc.hpp"
#include "syscall.hpp"
#include "tss.hpp"
#include "unique_ptr.hpp"
#include "vcpu.hpp"
#include "vlapic.hpp"
#include "vmx_msr_bitmap.hpp"

// Startup exception index for global ECs. Global ECs (except roottask) receive this
// exception the first time a scheduling context is bound to them.
#define EXC_STARTUP (NUM_EXC - 2)

class Sm;
class Utcb;

class Ec : public Typed_kobject<Kobject::Type::EC>, public Refcount, public Queue<Sc>
{
    friend class Queue<Ec>;

    // Needs access to NMI handling functions.
    friend class Vcpu;

private:
    void (*cont)() ALIGNED(16);
    Cpu_regs regs;
    Ec* rcap{nullptr};

    Unique_ptr<Utcb> utcb;

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

    // Virtual Address of the UTCB in userspace.
    mword user_utcb{0};

    Fpu fpu;

    // Ec::run_vcpu needs a way to find the right vcpu when the continuation points to it. Having a cpu-local
    // "current" vCPU doesn't make much sense, because we would have to ask the EC for the right vCPU anyway.
    Refptr<Vcpu> vcpu;

    static Slab_cache cache;

    // If we received an NMI while we are in kernel space, the NMI handler will create a state in which we
    // receive an exception if we try to return to user space. This function restores our state so we can
    // return to user space.
    static void fixup_nmi_user_trap();

    // Do the NMI work that can be done inside the NMI handler.
    static void do_early_nmi_work();

    // Check if an exception is due to an earlier NMI and if yes, restore a good state and handle the deferred
    // work.
    static void maybe_handle_deferred_nmi_work(Exc_regs*);

    // Do the work that led to sending an NMI, e.g. invalidating the TLB by reloading the CR3. This function
    // should only be called in the `from user space`-part of the NMI handler. As soon as HZD_SCHED goes away,
    // this function can be replaced with handle_hazards.
    static void do_deferred_nmi_work();

    static void handle_exc(Exc_regs*) asm("exc_handler");
    static void handle_exc_altstack(Exc_regs*) asm("exc_handler_altstack");

    [[noreturn]] static void handle_vmx() asm("vmx_handler");

    static bool handle_exc_gp(Exc_regs*);
    static bool handle_exc_pf(Exc_regs*);

    // Try to fixup a #GP in the kernel. See FIXUP_CALL for when this may be
    // appropriate.
    //
    // Returns true, if the #GP was handled and normal operation can resume.
    static bool fixup(Exc_regs* r);

    // Checks Cpu::hazard for any set hazards and handles them. If this leads to a reschedule, the current EC
    // will continue to execute at the given continuation.
    static void handle_hazards(void (*continuation)());

    static void pre_free(Rcu_elem* a)
    {
        Ec* e = static_cast<Ec*>(a);

        assert(e);

        // Cleanup user space mappings of kernel memory.
        if (e->user_utcb) {
            e->pd_user_page->Space_mem::insert(e->user_utcb, 0, 0, 0);
            e->user_utcb = 0;
        }
    }

    inline bool is_idle_ec() const { return cont == idle; }

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

    // Modify the register state of the EC for exiting via iret.
    //
    // This function assumes that the EC entered via syscall.
    inline void redirect_to_iret()
    {
        regs.rip = regs.ARG_IP;
        regs.cs = SEL_USER_CODE;
        regs.rfl = Cpu::EFL_MBS;
        regs.rsp = regs.ARG_SP;
        regs.ss = SEL_USER_DATA;
    }

    void transfer_fpu(Ec*);

    [[noreturn]] static void idle();

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
        MAP_USER_PAGE_IN_OWNER = 1 << 2,
    };

    // Construct a normal execution context.
    //
    // creation_flags is a bit field of ec_creation_flags.
    Ec(Pd* own, mword sel, Pd* p, void (*f)(), unsigned c, unsigned e, mword u, mword s, int creation_flags);

    ~Ec();

    inline bool blocked() const { return next || !cont; }

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

    // We have to make load_fpu and save_fpu public becaue the vCPU has to save and restore the FPU content of
    // the EC that is executing the vCPU.
    void load_fpu();
    void save_fpu();

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
    [[noreturn]] void return_to_user();

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

    // Tries to acquire the given vCPU as Ec::current. If the EC was able to acquire the vCPU, Ec::vcpu is set
    // to point to the given vCPU. Otherwise Ec::vcpu will continue to be a nullptr. An EC is only allowed to
    // modify a vCPUs state or to run it after successfully acquiring it.
    static Vcpu_acquire_result try_acquire_vcpu(Vcpu* vcpu_);

    // Releases the ownership of the vCPU associated with this EC and also clears Ec::vcpu. An EC is only
    // allowed to release a vCPU if it successfully acquired it before. After releasing the vCPU, the EC has
    // to acquire it again before it may run it again.
    static void release_vcpu();

    // Sets the given MTD bits in Ec::vcpu and then calls Ec::resume_vcpu to execute the associated vCPU. An
    // EC is only allowed to do this if it successfully acquired a vCPU before.
    [[noreturn]] static void run_vcpu(Mtd mtd);

    // Enter the vCPU that is associated with this EC (Ec::vcpu). This function can only be called if
    // Ec::try_acquire_vcpu has been called before to acquire and set Ec::vcpu. Returning to user space using
    // Ec::sys_finish clears Ec::vcpu again.
    [[noreturn]] static void resume_vcpu();

    [[noreturn]] HOT static void ret_user_sysexit();

    [[noreturn]] HOT static void ret_user_iret() asm("ret_user_iret");

    [[noreturn]] static void sys_finish(Sys_regs::Status status);
    [[noreturn]] static void sys_finish(Result_void<Sys_regs::Status> result);

    // We need a parameter-less version of sys_finish that can be used as EC continuation.
    template <Sys_regs::Status S> [[noreturn]] static void sys_finish() { sys_finish(S); }

    [[noreturn]] void activate();

    template <void (*)()> [[noreturn]] static void send_msg();

    [[noreturn]] HOT static void recv_kern();

    [[noreturn]] HOT static void recv_user();

    [[noreturn]] HOT static void reply(void (*)() = nullptr, Sm* = nullptr);

    [[noreturn]] HOT static void sys_call();

    [[noreturn]] HOT static void sys_reply();

    [[noreturn]] static void sys_create_pd();

    [[noreturn]] static void sys_create_ec();

    [[noreturn]] static void sys_create_sc();

    [[noreturn]] static void sys_create_pt();

    [[noreturn]] static void sys_create_sm();

    [[noreturn]] static void sys_create_kp();

    [[noreturn]] static void sys_create_vcpu();

    [[noreturn]] static void sys_revoke();

    [[noreturn]] static void sys_pd_ctrl();

    [[noreturn]] static void sys_pd_ctrl_lookup();

    [[noreturn]] static void sys_pd_ctrl_map_access_page();

    [[noreturn]] static void sys_pd_ctrl_delegate();

    [[noreturn]] static void sys_pd_ctrl_msr_access();

    [[noreturn]] static void sys_ec_ctrl();

    [[noreturn]] static void sys_sc_ctrl();

    [[noreturn]] static void sys_pt_ctrl();

    [[noreturn]] static void sys_sm_ctrl();

    [[noreturn]] static void sys_kp_ctrl();

    [[noreturn]] static void sys_kp_ctrl_map();

    [[noreturn]] static void sys_kp_ctrl_unmap();

    [[noreturn]] static void sys_vcpu_ctrl();

    [[noreturn]] static void sys_vcpu_ctrl_run();

    [[noreturn]] static void sys_vcpu_ctrl_poke();

    [[noreturn]] static void sys_machine_ctrl();

    [[noreturn]] static void sys_machine_ctrl_suspend();

    [[noreturn]] static void sys_machine_ctrl_update_microcode();

    [[noreturn]] static void root_invoke();

    template <bool> static Delegate_result_void delegate();

    [[noreturn]] static void dead() { die("IPC Abort"); }

    [[noreturn]] static void die(char const*, Exc_regs* = &current()->regs);

    static inline void* operator new(size_t) { return cache.alloc(); }

    static inline void operator delete(void* ptr) { cache.free(ptr); }

    [[noreturn]] static void syscall_handler() asm("syscall_handler");
};
