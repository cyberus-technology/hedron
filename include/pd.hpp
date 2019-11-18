/*
 * Protection Domain
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
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
#include "crd.hpp"
#include "nodestruct.hpp"
#include "space_mem.hpp"
#include "space_obj.hpp"
#include "space_pio.hpp"

class Pd : public Kobject, public Refcount, public Space_mem, public Space_pio, public Space_obj
{
    private:
        static Slab_cache cache;

        WARN_UNUSED_RESULT
        mword clamp (mword,   mword &, mword, mword);

        WARN_UNUSED_RESULT
        mword clamp (mword &, mword &, mword, mword, mword);

        void *apic_access_page {nullptr};

        static void pre_free (Rcu_elem * a)
        {
            Pd * pd = static_cast <Pd *>(a);

            Crd crd(Crd::MEM);
            pd->revoke<Space_mem>(crd.base(), crd.order(), crd.attr(), true);

            crd = Crd(Crd::PIO);
            pd->revoke<Space_pio>(crd.base(), crd.order(), crd.attr(), true);

            crd = Crd(Crd::OBJ);
            pd->revoke<Space_obj>(crd.base(), crd.order(), crd.attr(), true);
        }

        static void free (Rcu_elem * a) {
            Pd * pd = static_cast <Pd *>(a);

            if (pd->del_ref()) {
                assert (pd != Pd::current());
                delete pd;
            }
        }

    public:
        CPULOCAL_ACCESSOR(pd, current);
        static No_destruct<Pd> kern;

        // The roottask is privileged and can map arbitrary physical memory that
        // is not claimed by the hypervisor.
        bool const is_priv = false;

        void *get_access_page();

        INIT Pd();
        ~Pd();

        Pd (Pd *own, mword sel, mword a, bool priv = false);

        HOT
        inline void make_current()
        {
            mword pcid = did;

            if (EXPECT_FALSE (htlb.chk (Cpu::id())))
                htlb.clr (Cpu::id());

            else {

                if (EXPECT_TRUE (current() == this))
                    return;

                pcid |= static_cast<mword>(1ULL << 63);
            }

            if (current()->del_rcu())
                Rcu::call (current());

            current() = this;

            bool ok = current()->add_ref();
            assert (ok);

            hpt.make_current (Cpu::feature (Cpu::FEAT_PCID) ? pcid : 0);
        }

        static inline Pd *remote (unsigned c)
        {
            return Cpulocal::get_remote(c).pd_current;
        }

        inline Space *subspace (Crd::Type t)
        {
            switch (t) {
                case Crd::MEM:  return static_cast<Space_mem *>(this);
                case Crd::PIO:  return static_cast<Space_pio *>(this);
                case Crd::OBJ:  return static_cast<Space_obj *>(this);
            }

            return nullptr;
        }

        template <typename>
        Tlb_cleanup delegate (Pd *, mword, mword, mword, mword, mword = 0, char const * = nullptr);

        template <typename>
        void revoke (mword, mword, mword, bool);

        Xfer xfer_item  (Pd *, Crd, Crd, Xfer);
        void xfer_items (Pd *, Crd, Crd, Xfer *, Xfer *, unsigned long);

        void xlt_crd (Pd *, Crd, Crd &);
        void del_crd (Pd *, Crd, Crd &, mword = 0, mword = 0);
        void rev_crd (Crd, bool, bool = true);

        static inline void *operator new (size_t) { return cache.alloc(); }

        static inline void *operator new (size_t, void *p) { return p; }

        static inline void operator delete (void *ptr) { cache.free (ptr); }
};
