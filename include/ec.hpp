/*
 * Execution Context
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
 * Copyright (C) 2013-2015 Alexander Boettcher, Genode Labs GmbH
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

#include "counter.hpp"
#include "fpu.hpp"
#include "mtd.hpp"
#include "pd.hpp"
#include "queue.hpp"
#include "regs.hpp"
#include "sc.hpp"
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
        Ec *        partner;
        Ec *        prev;
        Ec *        next;
        Fpu *       fpu;
        union {
            struct {
                uint16  cpu;
                uint16  glb;
            };
            uint32  xcpu;
        };
        unsigned const evt;
        Timeout_hypercall timeout;
        mword          user_utcb;

        Sm *         xcpu_sm;

        static Slab_cache cache;

        REGPARM (1)
        static void handle_exc (Exc_regs *) asm ("exc_handler");

        NORETURN
        static void handle_vmx() asm ("vmx_handler");

        NORETURN
        static void handle_svm() asm ("svm_handler");

        NORETURN
        static void handle_tss() asm ("tss_handler");

        static void handle_exc_nm();
        static bool handle_exc_ts (Exc_regs *);
        static bool handle_exc_gp (Exc_regs *);
        static bool handle_exc_pf (Exc_regs *);

        static inline uint8 ifetch (mword);

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

        NORETURN
        static inline void vmx_cr();

        static bool fixup (mword &);

        NOINLINE
        static void handle_hazard (mword, void (*)());

        static void pre_free (Rcu_elem * a)
        {
            Ec * e = static_cast<Ec *>(a);

            assert(e);

            // remove mapping in page table
            if (e->user_utcb) {
                e->pd->remove_utcb(e->user_utcb);
                e->pd->Space_mem::insert (e->user_utcb, 0, 0, 0);
                e->user_utcb = 0;
            }

            // XXX If e is on another CPU and there the fpowner - this check will fail.
            // XXX For now the destruction is delayed until somebody else grabs the FPU.
            if (fpowner == e) {
                assert (Sc::current->cpu == e->cpu);

                bool zero = fpowner->del_ref();
                assert (!zero);

                fpowner      = nullptr;
                Cpu::hazard |= HZD_FPU;
            }
        }

        static void free (Rcu_elem * a)
        {
            Ec * e = static_cast<Ec *>(a);

            if (e->regs.vtlb) {
                trace(0, "leaking memory - vCPU EC memory re-usage not supported");
                return;
            }

            if (e->del_ref()) {
                assert(e != Ec::current);
                delete e;
            }
        }

        ALWAYS_INLINE
        inline Sys_regs *sys_regs() { return &regs; }

        ALWAYS_INLINE
        inline Exc_regs *exc_regs() { return &regs; }

        ALWAYS_INLINE
        inline void set_partner (Ec *p)
        {
            partner = p;
            bool ok = partner->add_ref();
            assert (ok);
            partner->rcap = this;
            ok = partner->rcap->add_ref();
            assert (ok);
            Sc::ctr_link++;
        }

        ALWAYS_INLINE
        inline unsigned clr_partner()
        {
            assert (partner == current);
            if (partner->rcap) {
                bool last = partner->rcap->del_ref();
                assert (!last);
                partner->rcap = nullptr;
            }
            bool last = partner->del_ref();
            assert (!last);
            partner = nullptr;
            return Sc::ctr_link--;
        }

        ALWAYS_INLINE
        inline void redirect_to_iret()
        {
            regs.REG(sp) = regs.ARG_SP;
            regs.REG(ip) = regs.ARG_IP;
        }

        void load_fpu();
        void save_fpu();

        void transfer_fpu (Ec *);

    public:
        static Ec *current CPULOCAL_HOT;
        static Ec *fpowner CPULOCAL;

        Ec (Pd *, void (*)(), unsigned);
        Ec (Pd *, mword, Pd *, void (*)(), unsigned, unsigned, mword, mword);
        Ec (Pd *, Pd *, void (*f)(), unsigned, Ec *);

        ~Ec();

        ALWAYS_INLINE
        inline void add_tsc_offset (uint64 tsc)
        {
            regs.add_tsc_offset (tsc);
        }

        ALWAYS_INLINE
        inline bool blocked() const { return next || !cont; }

        ALWAYS_INLINE
        inline void set_timeout (uint64 t, Sm *s)
        {
            if (EXPECT_FALSE (t))
                timeout.enqueue (t, s);
        }

        ALWAYS_INLINE
        inline void clr_timeout()
        {
            if (EXPECT_FALSE (timeout.active()))
                timeout.dequeue();
        }

        ALWAYS_INLINE
        inline void set_si_regs(mword sig, mword cnt)
        {
            regs.ARG_2 = sig;
            regs.ARG_3 = cnt;
        }

        ALWAYS_INLINE NORETURN
        inline void make_current()
        {
            if (EXPECT_FALSE (current->del_rcu()))
                Rcu::call (current);

            current = this;

            bool ok = current->add_ref();
            assert (ok);

            Tss::run.sp0 = reinterpret_cast<mword>(exc_regs() + 1);

            pd->make_current();

            asm volatile ("mov %0," EXPAND (PREG(sp);) "jmp *%1" : : "g" (CPU_LOCAL_STCK + PAGE_SIZE), "q" (cont) : "memory"); UNREACHED;
        }

        ALWAYS_INLINE
        static inline Ec *remote (unsigned c)
        {
            return *reinterpret_cast<volatile typeof current *>(reinterpret_cast<mword>(&current) - CPU_LOCAL_DATA + HV_GLOBAL_CPUS + c * PAGE_SIZE);
        }

        NOINLINE
        void help (void (*c)())
        {
            if (EXPECT_TRUE (cont != dead)) {

                Counter::print<1,16> (++Counter::helping, Console_vga::COLOR_LIGHT_WHITE, SPN_HLP);
                current->cont = c;

                if (EXPECT_TRUE (++Sc::ctr_loop < 100))
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

                bool ok = Sc::current->add_ref();
                assert (ok);

                enqueue (Sc::current);
            }

            Sc::schedule (true);
        }

        ALWAYS_INLINE
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

        HOT NORETURN
        static void ret_user_iret() asm ("ret_user_iret");

        NORETURN
        static void ret_user_vmresume();

        NORETURN
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
        static void sys_lookup();

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
        static void die (char const *, Exc_regs * = &current->regs);

        static void idl_handler();

        ALWAYS_INLINE
        static inline void *operator new (size_t) { return cache.alloc(); }

        ALWAYS_INLINE
        static inline void operator delete (void *ptr) { cache.free (ptr); }
};
