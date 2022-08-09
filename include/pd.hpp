/*
 * Protection Domain
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
#include "crd.hpp"
#include "delegate_result.hpp"
#include "nodestruct.hpp"
#include "space_mem.hpp"
#include "space_obj.hpp"
#include "space_pio.hpp"

class Pd : public Typed_kobject<Kobject::Type::PD>,
           public Refcount,
           public Space_mem,
           public Space_pio,
           public Space_obj
{
private:
    static Slab_cache cache;

    WARN_UNUSED_RESULT
    mword clamp(mword, mword&, mword, mword);

    WARN_UNUSED_RESULT
    mword clamp(mword&, mword&, mword, mword, mword);

    void* apic_access_page{nullptr};

    static void pre_free(Rcu_elem* a)
    {
        Pd* pd = static_cast<Pd*>(a);

        Crd crd(Crd::PIO);
        pd->revoke<Space_pio>(crd.base(), crd.order(), crd.attr(), true);

        crd = Crd(Crd::OBJ);
        pd->revoke<Space_obj>(crd.base(), crd.order(), crd.attr(), true);
    }

    static void free(Rcu_elem* a)
    {
        Pd* pd = static_cast<Pd*>(a);

        if (pd->del_ref()) {
            assert(pd != Pd::current());
            delete pd;
        }
    }

public:
    CPULOCAL_REMOTE_ACCESSOR(pd, current);
    static No_destruct<Pd> kern;

    // The roottask is privileged and can map arbitrary physical memory that
    // is not claimed by the hypervisor.
    bool const is_priv = false;

    // When PDs have this property, they are allowed to access certain
    // hardware properties in a potentially unsafe way. In particular, this
    // grants partial MSR access.
    bool const is_passthrough = false;

    void* get_access_page();

    Pd();
    ~Pd();

    // Capability permission bitmask
    enum
    {
        PERM_OBJ_CREATION = 1U << 0,
    };

    enum pd_creation_flags
    {
        IS_PRIVILEGED = 1 << 0,
        IS_PASSTHROUGH = 1 << 1,
    };

    // Construct a protection domain.
    //
    // creation_flags is a bit field of pd_creation_flags.
    Pd(Pd* own, mword sel, mword a, int creation_flags);

    HOT inline void make_current()
    {
        mword pcid = did;

        if (EXPECT_FALSE(stale_host_tlb.chk(Cpu::id())))
            stale_host_tlb.clr(Cpu::id());

        else {

            if (EXPECT_TRUE(current() == this))
                return;

            pcid |= static_cast<mword>(1ULL << 63);
        }

        if (current()->del_rcu())
            Rcu::call(current());

        current() = this;

        bool ok = current()->add_ref();
        assert(ok);

        // When we schedule the idle EC, we switch to Pd::kern. Pd::kern's
        // host page table is actually all the physical memory that
        // userspace can use, so we cannot use it as a page table here.
        Hpt& target_hpt{EXPECT_FALSE(this == &Pd::kern) ? Hpt::boot_hpt() : hpt};
        target_hpt.make_current(Cpu::feature(Cpu::FEAT_PCID) ? pcid : 0);
    }

    // Access the current PD on a remote core.
    //
    // The returned pointer stays valid until the next transition to
    // userspace.
    static Pd* remote(unsigned cpu) { return remote_load_current(cpu); }

    inline Space* subspace(Crd::Type t)
    {
        switch (t) {
        // Memory is not handled as a Mdb-managed space anymore.
        case Crd::MEM:
            return nullptr;

        case Crd::PIO:
            return static_cast<Space_pio*>(this);
        case Crd::OBJ:
            return static_cast<Space_obj*>(this);
        }

        return nullptr;
    }

    template <typename>
    Delegate_result_void delegate(Tlb_cleanup& cleanup, Pd* snd, mword snd_base, mword rcv_base, mword ord,
                                  mword attr, mword sub = 0, char const* deltype = nullptr);

    template <typename> void revoke(mword, mword, mword, bool);

    Xfer xfer_item(Pd*, Crd, Crd, Xfer);
    void xfer_items(Pd*, Crd, Crd, Xfer*, Xfer*, unsigned long);

    void xlt_crd(Pd*, Crd, Crd&);
    Delegate_result_void del_crd(Pd* pd, Crd del, Crd& crd, mword sub = 0, mword hot = 0);
    void rev_crd(Crd, bool);

    static inline void* operator new(size_t) { return cache.alloc(); }

    static inline void* operator new(size_t, void* p) { return p; }

    static inline void operator delete(void* ptr) { cache.free(ptr); }
};
