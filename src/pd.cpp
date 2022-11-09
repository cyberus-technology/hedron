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

#include "pd.hpp"
#include "hip.hpp"
#include "mtrr.hpp"
#include "scope_guard.hpp"
#include "stdio.hpp"

INIT_PRIORITY(PRIO_SLAB)
Slab_cache Pd::cache(sizeof(Pd), 32);

ALIGNED(32) No_destruct<Pd> Pd::kern;

// Constructor for the initial kernel PD.
Pd::Pd() : Typed_kobject(static_cast<Space_obj*>(this)), Space_pio(this)
{
    auto mark_avail_phys = [this](uint64 start, uint64 end, mword attr = 0x7) {
        Space_mem::insert_root(start >> PAGE_BITS, end >> PAGE_BITS, attr);
    };

    Mtrr_state::get().init();

    // We insert everything into the memory space that is usable for userspace.
    // It needs to match Hip::add_mhv and our linker script.

    // This the memory before the first ELF segment.
    mark_avail_phys(0, LOAD_ADDR + PHYS_RELOCATION);

    // The memory after the second ELF segment to "infinity".
    mark_avail_phys(reinterpret_cast<mword>(&LOAD_END) + PHYS_RELOCATION, 1ULL << hpt.max_order());

    // HIP
    Paddr frame_h = Buddy::ptr_to_phys(&PAGE_H);
    mark_avail_phys(frame_h, frame_h + PAGE_SIZE, 1);

    // I/O Ports
    Space_pio::addreg(0, 1UL << 16, 7);
}

Pd::Pd(Pd* own, mword sel, mword a, int creation_flags)
    : Typed_kobject(static_cast<Space_obj*>(own), sel, a, free, pre_free), Space_mem(Hpt::boot_hpt()),
      Space_pio(this), is_priv(creation_flags & IS_PRIVILEGED),
      is_passthrough(creation_flags & IS_PASSTHROUGH)
{
}

template <typename S>
Delegate_result_void Pd::delegate(Tlb_cleanup& cleanup, Pd* snd, mword const snd_base, mword const rcv_base,
                                  mword const ord, mword const attr, mword const sub, char const* deltype)
{
    Mdb* mdb;
    for (mword addr = snd_base; (mdb = snd->S::tree_lookup(addr, true));
         addr = mdb->node_base + (1UL << mdb->node_order)) {

        mword o, b = snd_base;
        if ((o = clamp(mdb->node_base, b, mdb->node_order, ord)) == ~0UL)
            break;

        Mdb* node = new Mdb(static_cast<S*>(this), b - mdb->node_base + mdb->node_phys,
                            b - snd_base + rcv_base, o, 0, mdb->node_type, sub);

        if (!S::tree_insert(node)) {
            delete node;

            Mdb* x = S::tree_lookup(b - snd_base + rcv_base);
            if (!x || x->prnt != mdb || x->node_attr != attr)
                trace(0, "overmap attempt %s - tree - PD:%p->%p SB:%#010lx RB:%#010lx O:%#04lx A:%#lx",
                      deltype, snd, this, snd_base, rcv_base, ord, attr);

            continue;
        }

        if (!node->insert_node(mdb, attr)) {
            S::tree_remove(node);
            delete node;
            trace(0, "overmap attempt %s - node - PD:%p->%p SB:%#010lx RB:%#010lx O:%#04lx A:%#lx", deltype,
                  snd, this, snd_base, rcv_base, ord, attr);
            continue;
        }

        cleanup.merge(S::update(node));
    }

    return Ok_void({});
}

template <>
Delegate_result_void Pd::delegate<Space_mem>(Tlb_cleanup& cleanup, Pd* snd, mword const snd_base,
                                             mword const rcv_base, mword const ord, mword const attr,
                                             mword const sub, [[maybe_unused]] char const* deltype)
{
    return Space_mem::delegate(cleanup, snd, snd_base << PAGE_BITS, rcv_base << PAGE_BITS, ord + PAGE_BITS,
                               attr, sub);
}

template <typename S> void Pd::revoke(mword const base, mword const ord, mword const attr, bool self)
{
    Mdb* mdb;
    for (mword addr = base; (mdb = S::tree_lookup(addr, true));
         addr = mdb->node_base + (1UL << mdb->node_order)) {

        mword o, p, b = base;
        if ((o = clamp(mdb->node_base, b, mdb->node_order, ord)) == ~0UL)
            break;

        Mdb* node = mdb;

        unsigned d = node->dpth;
        bool demote = false;

        for (Mdb* ptr;; node = ptr) {

            if (node->dpth == d + !self)
                demote = clamp(node->node_phys, p = b - mdb->node_base + mdb->node_phys, node->node_order,
                               o) != ~0UL;

            if (demote && node->node_attr & attr) {
                static_cast<S*>(node->space)->update(node, attr);
                node->demote_node(attr);
            }

            ptr = Atomic::load(node->next);

            if (ptr->dpth <= d)
                break;
        }

        Mdb* x = Atomic::load(node->next);

        assert((x->dpth <= d) || (self && !(x->node_attr & attr)) ||
               (!self && ((mdb == node) || (d + 1 == x->dpth) || !(x->node_attr & attr))));
        assert(x->dpth > node->dpth ? (x->dpth == node->dpth + 1) : true);

        for (Mdb* ptr;; node = ptr) {
            if (node->remove_node() && static_cast<S*>(node->space)->tree_remove(node))
                Rcu::call(node);

            ptr = Atomic::load(node->prev);

            if (node->dpth <= d)
                break;
        }

        assert(node == mdb);
    }
}

template <> void Pd::revoke<Space_mem>(mword const base, mword const ord, mword const attr, bool self)
{
    if (not self) {
        trace(TRACE_ERROR, "Non-self revocation is not supported: Revoking everything!");
    }

    Tlb_cleanup cleanup;
    Space_mem::revoke(cleanup, base << PAGE_BITS, ord + PAGE_BITS, attr);

    shootdown();
    cleanup.ignore_tlb_flush(); // because it is done.
}

mword Pd::clamp(mword snd_base, mword& rcv_base, mword snd_ord, mword rcv_ord)
{
    if ((snd_base ^ rcv_base) >> max(snd_ord, rcv_ord))
        return ~0UL;

    rcv_base |= snd_base;

    return min(snd_ord, rcv_ord);
}

mword Pd::clamp(mword& snd_base, mword& rcv_base, mword snd_ord, mword rcv_ord, mword h)
{
    assert(snd_ord < sizeof(mword) * 8);
    assert(rcv_ord < sizeof(mword) * 8);

    mword s = (1ul << snd_ord) - 1;
    mword r = (1ul << rcv_ord) - 1;

    snd_base &= ~s;
    rcv_base &= ~r;

    if (EXPECT_TRUE(s < r)) {
        rcv_base |= h & r & ~s;
        return snd_ord;
    } else {
        snd_base |= h & s & ~r;
        return rcv_ord;
    }
}

void Pd::xlt_crd(Pd* pd, Crd xlt, Crd& crd)
{
    Crd::Type t = xlt.type();

    if (EXPECT_FALSE(t == Crd::MEM)) {
        trace(TRACE_ERROR, "Memory translation is not supported anymore");
        return;
    }

    if (t && t == crd.type()) {

        Space *snd = pd->subspace(t), *rcv = subspace(t);
        mword sb = crd.base(), so = crd.order(), rb = xlt.base(), ro = xlt.order();
        Mdb *mdb, *node;

        assert(snd);
        assert(rcv);

        for (node = mdb = snd->tree_lookup(sb); node; node = node->prnt)
            if (node->space == rcv && node != mdb)
                if ((ro = clamp(node->node_base, rb, node->node_order, ro)) != ~0UL)
                    break;

        if (!node) {
            /* Special handling on Genode:
             * If a translate of an item inside the same PD (receiver/sender in same PD)
             * are of no success, then return the very same item.
             */
            Mdb* first = snd->tree_lookup(crd.base());
            if (first && first->space == rcv && first == mdb) {
                rb = xlt.base();
                ro = xlt.order();
                if ((ro = clamp(first->node_base, rb, first->node_order, ro)) != ~0UL)
                    node = first;
            }
        }

        if (node) {

            so = clamp(mdb->node_base, sb, mdb->node_order, so);
            sb = (sb - mdb->node_base) + (mdb->node_phys - node->node_phys) + node->node_base;

            if ((ro = clamp(sb, rb, so, ro)) != ~0UL) {
                trace(TRACE_DEL, "XLT OBJ PD:%p->%p SB:%#010lx RB:%#010lx O:%#04lx", pd, this, crd.base(), rb,
                      so);
                crd = Crd(crd.type(), rb, ro, mdb->node_attr);
                return;
            }
        }
    }

    crd = Crd(0);
}

Delegate_result_void Pd::del_crd(Pd* pd, Crd del, Crd& crd, mword sub, mword hot)
{
    Crd::Type st = crd.type(), rt = del.type();
    Tlb_cleanup cleanup;

    mword a = crd.attr() & del.attr(), sb = crd.base(), so = crd.order(), rb = del.base(), ro = del.order(),
          o = 0;

    if (st != rt or (not a and rt != Crd::MEM)) {
        crd = Crd(0);
        return Ok_void({});
    }

    // Regardless of whether the delegate operations below fail or succeed, they might have done operations
    // that require TLB flushing.
    auto guard{Scope_guard([this, &cleanup, rt]() {
        if (cleanup.need_tlb_flush() && rt == Crd::OBJ)
            /* if FRAME_0 got replaced by real pages we have to tell all cpus, done below by shootdown */
            this->stale_host_tlb.merge(cpus);

        if (cleanup.need_tlb_flush()) {
            shootdown();
            cleanup.ignore_tlb_flush(); // because it is done.
        }
    })};

    switch (rt) {

    case Crd::MEM:
        o = clamp(sb, rb, so, ro, hot);
        trace(TRACE_DEL, "DEL MEM PD:%p->%p SB:%#010lx RB:%#010lx O:%#04lx A:%#lx", pd, this, sb, rb, o, a);
        TRY_OR_RETURN(delegate<Space_mem>(cleanup, pd, sb, rb, o, a, sub, "MEM"));
        break;

    case Crd::PIO:
        o = clamp(sb, rb, so, ro);
        trace(TRACE_DEL, "DEL I/O PD:%p->%p SB:%#010lx RB:%#010lx O:%#04lx A:%#lx", pd, this, rb, rb, o, a);
        TRY_OR_RETURN(delegate<Space_pio>(cleanup, pd, rb, rb, o, a, sub, "PIO"));
        break;

    case Crd::OBJ:
        o = clamp(sb, rb, so, ro, hot);
        trace(TRACE_DEL, "DEL OBJ PD:%p->%p SB:%#010lx RB:%#010lx O:%#04lx A:%#lx", pd, this, sb, rb, o, a);
        TRY_OR_RETURN(delegate<Space_obj>(cleanup, pd, sb, rb, o, a, 0, "OBJ"));
        break;
    }

    crd = Crd(rt, rb, o, a);

    return Ok_void({});
}

void Pd::rev_crd(Crd crd, bool self)
{
    switch (crd.type()) {

    case Crd::MEM:
        trace(TRACE_REV, "REV MEM PD:%p B:%#010lx O:%#04x A:%#04x %s", this, crd.base(), crd.order(),
              crd.attr(), self ? "+" : "-");
        revoke<Space_mem>(crd.base(), crd.order(), crd.attr(), self);
        break;

    case Crd::PIO:
        trace(TRACE_REV, "REV I/O PD:%p B:%#010lx O:%#04x A:%#04x %s", this, crd.base(), crd.order(),
              crd.attr(), self ? "+" : "-");
        revoke<Space_pio>(crd.base(), crd.order(), crd.attr(), self);
        break;

    case Crd::OBJ:
        trace(TRACE_REV, "REV OBJ PD:%p B:%#010lx O:%#04x A:%#04x %s", this, crd.base(), crd.order(),
              crd.attr(), self ? "+" : "-");
        revoke<Space_obj>(crd.base(), crd.order(), crd.attr(), self);
        break;
    }
}

Delegate_result<Xfer> Pd::xfer_item(Pd* src_pd, Crd xlt, Crd del, Xfer s_ti)
{
    mword set_as_del = 0;
    Crd crd = s_ti.crd();

    switch (s_ti.kind()) {

    case Xfer::Kind::TRANSLATE:
        xlt_crd(src_pd, xlt, crd);
        break;

    case Xfer::Kind::TRANS_DELEGATE:
        xlt_crd(src_pd, xlt, crd);
        if (crd.type()) {
            break;
        }

        crd = s_ti.crd();
        set_as_del = 1;
        [[fallthrough]];
    case Xfer::Kind::DELEGATE:
        TRY_OR_RETURN(del_crd(src_pd->is_priv && s_ti.from_kern() ? &kern : src_pd, del, crd,
                              s_ti.subspaces(), s_ti.hotspot()));
        break;

    default:
        crd = Crd(0);
        break;
    };

    return Ok(Xfer{crd, s_ti.flags() | set_as_del});
}

Delegate_result_void Pd::xfer_items(Pd* src_pd, Crd xlt, Crd del, Xfer* s_ti, Xfer* d_ti,
                                    unsigned long num_typed)
{
    for (unsigned long cur = 0; cur < num_typed; cur++) {
        Xfer res{TRY_OR_RETURN(xfer_item(src_pd, xlt, del, *(s_ti - cur)))};

        if (d_ti) {
            *(d_ti - cur) = res;
        }
    }

    return Ok_void({});
}

void* Pd::get_access_page()
{
    if (!Atomic::load(apic_access_page)) {
        void* page = Buddy::allocator.alloc(0, Buddy::FILL_0);

        if (!Atomic::cmp_swap(apic_access_page, static_cast<void*>(nullptr), page)) {
            Buddy::allocator.free(reinterpret_cast<mword>(page));
        }
    }

    void* ret{Atomic::load(apic_access_page)};

    assert(ret);

    return ret;
}

Pd::~Pd()
{
    pre_free(this);

    if (apic_access_page) {
        Buddy::allocator.free(reinterpret_cast<mword>(apic_access_page));
        apic_access_page = nullptr;
    }
}
