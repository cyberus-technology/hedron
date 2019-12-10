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

#include "math.hpp"
#include "counter.hpp"
#include "cpulocal.hpp"
#include "fpu.hpp"
#include "lock_guard.hpp"
#include "mtd.hpp"
#include "pd.hpp"
#include "queue.hpp"
#include "regs.hpp"
#include "sc.hpp"
#include "syscall.hpp"
#include "timeout_hypercall.hpp"
#include "tss.hpp"
#include "si.hpp"

#include "stdio.hpp"

class Utcb;
class Sm;

class Ec : public Kobject, public Refcount, public Queue<Sc>
{
    friend class Queue<Ec>;

    private:
        void        (*cont)() ALIGNED (16);
        Cpu_regs    regs;
        Ec *        rcap;
        Utcb *      utcb;
        Refptr<Pd>  pd;
        Ec *        partner {nullptr};
        Ec *        prev {nullptr};
        Ec *        next {nullptr};
        union {
            struct {
                uint16  cpu;
                uint16  glb;
            };
            uint32  xcpu;
        };
        unsigned const evt;
        Timeout_hypercall timeout {this};
        mword          user_utcb;

        Sm *         xcpu_sm;

        void * vlapic_page {nullptr};

        Fpu fpu;

        static Slab_cache cache;

        REGPARM (1)
        static void handle_exc (Exc_regs *) asm ("exc_handler");

        NORETURN
        static void handle_vmx() asm ("vmx_handler");

        NORETURN
        static void USED handle_svm() asm ("svm_handler");

        static bool handle_exc_gp (Exc_regs *);
        static bool handle_exc_pf (Exc_regs *);

        NORETURN
        static inline void svm_exception (mword);

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

        static bool fixup (mword &);

        NOINLINE
        static void handle_hazard (mword, void (*)());

        static void pre_free (Rcu_elem * a)
        {
            Ec * e = static_cast<Ec *>(a);

            assert(e);

            // remove mapping in page table
            if (e->user_utcb) {
                e->pd->Space_mem::insert (e->user_utcb, 0, 0, 0);
                e->user_utcb = 0;
            }
        }

        inline bool is_idle_ec() { return cont == idle; }

        static void free (Rcu_elem * a)
        {
            Ec * e = static_cast<Ec *>(a);

            if (e->del_ref()) {
                assert(e != Ec::current());
                delete e;
            }
        }

        inline Sys_regs *sys_regs() { return &regs; }

        inline Exc_regs *exc_regs() { return &regs; }

        inline void set_partner (Ec *p)
        {
            partner = p;
            bool ok = partner->add_ref();
            assert (ok);
            partner->rcap = this;
            ok = partner->rcap->add_ref();
            assert (ok);
            Sc::ctr_link()++;
        }

        inline unsigned clr_partner()
        {
            assert (partner == current());
            if (partner->rcap) {
                bool last = partner->rcap->del_ref();
                assert (!last);
                partner->rcap = nullptr;
            }
            bool last = partner->del_ref();
            assert (!last);
            partner = nullptr;
            return Sc::ctr_link()--;
        }

        inline void redirect_to_iret()
        {
            regs.REG(sp) = regs.ARG_SP;
            regs.REG(ip) = regs.ARG_IP;
        }

        void load_fpu();
        void save_fpu();

        void transfer_fpu (Ec *);

        static bool sanitize_cap(Capability &cap, Kobject::Type expected_type, mword perm_mask = 0);

        static Pd *sanitize_syscall_params(Sys_create_ec *);

    public:
        CPULOCAL_ACCESSOR(ec, current);

        Ec (Pd *, void (*)(), unsigned);
        Ec (Pd *, mword, Pd *, void (*)(), unsigned, unsigned, mword, mword, bool, bool);
        Ec (Pd *, Pd *, void (*f)(), unsigned, Ec *);

        ~Ec();

        inline void add_tsc_offset (uint64 tsc)
        {
            regs.add_tsc_offset (tsc);
        }

        inline bool blocked() const { return next || !cont; }

        inline void set_timeout (uint64 t, Sm *s)
        {
            if (EXPECT_FALSE (t))
                timeout.enqueue (t, s);
        }

        inline void clr_timeout()
        {
            if (EXPECT_FALSE (timeout.active()))
                timeout.dequeue();
        }

        inline void set_si_regs(mword sig, mword cnt)
        {
            regs.ARG_2 = sig;
            regs.ARG_3 = cnt;
        }

        NORETURN
        inline void make_current()
        {
            if (EXPECT_FALSE (current()->del_rcu()))
                Rcu::call (current());

            transfer_fpu(current());
            current() = this;

            bool ok = current()->add_ref();
            assert (ok);

            // Set the stack to just behind the register block to be able to use
            // push instructions to fill it. System call entry points need to
            // preserve less state.
            Tss::local().sp0 = reinterpret_cast<mword>(exc_regs() + 1);
            Cpulocal::set_sys_entry_stack (sys_regs() + 1);

            pd->make_current();

            asm volatile ("mov %%gs:0," EXPAND (PREG(sp);) "jmp *%0" : : "q" (cont) : "memory"); UNREACHED;
        }

        static inline Ec *remote (unsigned cpu)
        {
            return Atomic::load (Cpulocal::get_remote (cpu).ec_current);
        }

        NOINLINE
        void help (void (*c)())
        {
            if (EXPECT_TRUE (cont != dead)) {

                Counter::print<1,16> (++Counter::helping(), Console_vga::COLOR_LIGHT_WHITE, SPN_HLP);
                current()->cont = c;

                if (EXPECT_TRUE (++Sc::ctr_loop() < 100))
                    activate();

                die ("Livelock");
            }
        }

        NOINLINE
        void block_sc()
        {
            {   Lock_guard <Spinlock> guard (lock);

                if (!blocked())
                    return;

                bool ok = Sc::current()->add_ref();
                assert (ok);

                enqueue (Sc::current());
            }

            Sc::schedule (true);
        }

        inline void release (void (*c)())
        {
            if (c)
                cont = c;

            Lock_guard <Spinlock> guard (lock);

            for (Sc *s; dequeue (s = head()); ) {
                if (EXPECT_TRUE(!s->last_ref()) || s->ec->partner) {
                    s->remote_enqueue(false);
                    continue;
                }

                Rcu::call(s);
            }
        }

        HOT NORETURN
        static void ret_user_sysexit();

        HOT NORETURN_GCC
        static void ret_user_iret() asm ("ret_user_iret");

        HOT
        static void chk_kern_preempt() asm ("chk_kern_preempt");

        NORETURN_GCC
        static void ret_user_vmresume();

        NORETURN_GCC
        static void ret_user_vmrun();

        NORETURN
        static void ret_xcpu_reply();

        template <Sys_regs::Status S, bool T = false>

        NOINLINE NORETURN
        static void sys_finish();

        NORETURN
        void activate();

        template <void (*)()>
        NORETURN
        static void send_msg();

        HOT NORETURN
        static void recv_kern();

        HOT NORETURN
        static void recv_user();

        HOT NORETURN
        static void reply (void (*)() = nullptr, Sm * = nullptr);

        HOT NORETURN
        static void sys_call();

        HOT NORETURN
        static void sys_reply();

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
        static void sys_xcpu_call();

        NORETURN
        static void idle();

        NORETURN
        static void xcpu_return();

        NORETURN
        static void root_invoke();

        template <bool>
        static void delegate();

        NORETURN
        static void dead() { die ("IPC Abort"); }

        NORETURN
        static void die (char const *, Exc_regs * = &current()->regs);

        static void idl_handler();

        static inline void *operator new (size_t) { return cache.alloc(); }

        static inline void operator delete (void *ptr) { cache.free (ptr); }
};
