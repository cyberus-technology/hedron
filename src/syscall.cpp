/*
 * System-Call Interface
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
 * Copyright (C) 2013-2015 Alexander Boettcher, Genode Labs GmbH
 *
 * Copyright (C) 2018 Stefan Hertrampf, Cyberus Technology GmbH.
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

#include "dmar.hpp"
#include "gsi.hpp"
#include "hip.hpp"
#include "hpet.hpp"
#include "lapic.hpp"
#include "pci.hpp"
#include "pt.hpp"
#include "sm.hpp"
#include "stdio.hpp"
#include "syscall.hpp"
#include "utcb.hpp"
#include "vectors.hpp"

// The permissions we need on a PD capability to create an object of a given type.
static unsigned perm_for_obj_creation(Kobject::Type type)
{
    return 1U << static_cast<int>(type);
}

template <Sys_regs::Status S, bool T>
void Ec::sys_finish()
{
    if (T)
        current()->clr_timeout();

    current()->regs.set_status (S);

    ret_user_sysexit();
}

void Ec::activate()
{
    Ec *ec = this;

    // XXX: Make the loop preemptible
    for (Sc::ctr_link() = 0; ec->partner; ec = ec->partner)
        Sc::ctr_link()++;

    if (EXPECT_FALSE (ec->blocked()))
        ec->block_sc();

    ec->make_current();
}

template <bool C>
void Ec::delegate()
{
    Ec *ec = current()->rcap;
    assert (ec);

    Ec *src = C ? ec : current();
    Ec *dst = C ? current() : ec;

    bool user = C || dst->cont == ret_user_sysexit;

    dst->pd->xfer_items (src->pd,
                         user ? dst->utcb->xlt : Crd (0),
                         user ? dst->utcb->del : Crd (Crd::MEM, (dst->cont == ret_user_iret ? dst->regs.cr2 : dst->regs.nst_fault) >> PAGE_BITS),
                         src->utcb->xfer(),
                         user ? dst->utcb->xfer() : nullptr,
                         src->utcb->ti());
}

template <void (*C)()>
void Ec::send_msg()
{
    Exc_regs *r = &current()->regs;

    Pt *pt = capability_cast<Pt>(Space_obj::lookup (current()->evt + r->dst_portal));

    if (EXPECT_FALSE (not pt)) {
        die ("PT not found");
    }

    Ec *ec = pt->ec;

    if (EXPECT_FALSE (current()->cpu != ec->xcpu))
        die ("PT wrong CPU");

    if (EXPECT_TRUE (!ec->cont)) {
        current()->cont = C;
        current()->set_partner (ec);
        current()->regs.mtd = pt->mtd.val;
        ec->cont = recv_kern;
        ec->regs.set_pt (pt->id);
        ec->regs.set_ip (pt->ip);
        ec->make_current();
    }

    ec->help (send_msg<C>);

    die ("IPC Timeout");
}

void Ec::sys_call()
{
    Sys_call *s = static_cast<Sys_call *>(current()->sys_regs());
    Pt *pt = capability_cast<Pt>(Space_obj::lookup (s->pt()));

    if (EXPECT_FALSE (not pt)) {
        sys_finish<Sys_regs::BAD_CAP>();
    }

    Ec *ec = pt->ec;

    if (EXPECT_FALSE (current()->cpu != ec->xcpu))
        sys_finish<Sys_regs::BAD_CPU>();

    if (EXPECT_TRUE (!ec->cont)) {
        current()->cont = ret_user_sysexit;
        current()->set_partner (ec);
        ec->cont = recv_user;
        ec->regs.set_pt (pt->id);
        ec->regs.set_ip (pt->ip);
        ec->make_current();
    }

    if (EXPECT_TRUE (!(s->flags() & Sys_call::DISABLE_BLOCKING)))
        ec->help (sys_call);

    sys_finish<Sys_regs::COM_TIM>();
}

void Ec::recv_kern()
{
    Ec *ec = current()->rcap;

    bool fpu = false;

    if (ec->cont == ret_user_iret)
        fpu = current()->utcb->load_exc (&ec->regs);
    else if (ec->cont == ret_user_vmresume)
        fpu = current()->utcb->load_vmx (&ec->regs);
    else if (ec->cont == ret_user_vmrun)
        fpu = current()->utcb->load_svm (&ec->regs);

    if (EXPECT_FALSE (fpu))
        ec->transfer_fpu (current());

    ret_user_sysexit();
}

void Ec::recv_user()
{
    Ec *ec = current()->rcap;

    ec->utcb->save (current()->utcb.get());

    if (EXPECT_FALSE (ec->utcb->tcnt()))
        delegate<true>();

    ret_user_sysexit();
}

void Ec::reply (void (*c)(), Sm * sm)
{
    current()->cont = c;

    if (EXPECT_FALSE (current()->glb))
        Sc::schedule (true);

    Ec *ec = current()->rcap;

    if (EXPECT_FALSE (!ec))
        Sc::current()->ec->activate();

    bool clr = ec->clr_partner();

    if (Sc::current()->ec == ec && Sc::current()->last_ref())
        Sc::schedule (true);

    if (sm)
        sm->dn (false, 0, ec, clr);

    if (!clr)
        Sc::current()->ec->activate();

    ec->make_current();
}

void Ec::sys_reply()
{
    Sm *sm = nullptr;

    if (Ec *ec = current()->rcap; EXPECT_TRUE (ec)) {

        Sys_reply *r = static_cast<Sys_reply *>(current()->sys_regs());
        if (EXPECT_FALSE (r->sm())) {
            sm = capability_cast<Sm>(Space_obj::lookup (r->sm()));

            if (sm and ec->cont == ret_user_sysexit) {
                ec->cont = sys_call;
            }
        }

        Utcb *src = current()->utcb.get();

        if (EXPECT_FALSE (src->tcnt()))
            delegate<false>();

        bool fpu = false;

        if (EXPECT_TRUE (ec->cont == ret_user_sysexit))
            src->save (ec->utcb.get());
        else if (ec->cont == ret_user_iret)
            fpu = src->save_exc (&ec->regs);
        else if (ec->cont == ret_user_vmresume)
            fpu = src->save_vmx (&ec->regs);
        else if (ec->cont == ret_user_vmrun)
            fpu = src->save_svm (&ec->regs);

        if (EXPECT_FALSE (fpu))
            current()->transfer_fpu (ec);
    }

    reply(nullptr, sm);
}

void Ec::sys_create_pd()
{
    Sys_create_pd *r = static_cast<Sys_create_pd *>(current()->sys_regs());

    trace (TRACE_SYSCALL, "EC:%p SYS_CREATE PD:%#lx", current(), r->sel());

    Capability parent_pd_cap = Space_obj::lookup (r->pd());
    Pd *parent_pd = capability_cast<Pd>(parent_pd_cap, perm_for_obj_creation (Type::PD));

    if (EXPECT_FALSE (not parent_pd)) {
        trace (TRACE_ERROR, "%s: Non-PD CAP (%#lx)", __func__, r->pd());
        sys_finish<Sys_regs::BAD_CAP>();
    }

    Pd *pd = new Pd (Pd::current(), r->sel(), parent_pd_cap.prm());
    if (!Space_obj::insert_root (pd)) {
        trace (TRACE_ERROR, "%s: Non-NULL CAP (%#lx)", __func__, r->sel());
        delete pd;
        sys_finish<Sys_regs::BAD_CAP>();
    }

    Crd crd = r->crd();
    pd->del_crd (Pd::current(), Crd (Crd::OBJ), crd);

    sys_finish<Sys_regs::SUCCESS>();
}

void Ec::sys_create_ec()
{
    Sys_create_ec *r = static_cast<Sys_create_ec *>(current()->sys_regs());

    trace (TRACE_SYSCALL, "EC:%p SYS_CREATE EC:%#lx CPU:%#x UPAGE:%#lx ESP:%#lx EVT:%#x", current(), r->sel(), r->cpu(), r->user_page(), r->esp(), r->evt());

    if (EXPECT_FALSE (!Hip::cpu_online (r->cpu()))) {
        trace (TRACE_ERROR, "%s: Invalid CPU (%#x)", __func__, r->cpu());
        sys_finish<Sys_regs::BAD_CPU>();
    }

    if (EXPECT_FALSE (r->is_vcpu() && !(Hip::feature() & (Hip::FEAT_VMX | Hip::FEAT_SVM)))) {
        trace (TRACE_ERROR, "%s: VCPUs not supported", __func__);
        sys_finish<Sys_regs::BAD_FTR>();
    }

    Pd *pd = capability_cast<Pd>(Space_obj::lookup (r->pd()), perm_for_obj_creation (Type::EC));

    if (EXPECT_FALSE (not pd)) {
        trace (TRACE_ERROR, "%s: Non-PD CAP (%#lx)", __func__, r->pd());
        sys_finish<Sys_regs::BAD_CAP>();
    }

    if (EXPECT_FALSE (r->user_page() >= USER_ADDR || r->user_page() & PAGE_MASK)) {
        trace (TRACE_ERROR, "%s: Invalid UPAGE address (%#lx)", __func__, r->user_page());
        sys_finish<Sys_regs::BAD_PAR>();
    }

    Ec *ec = new Ec (Pd::current(),
                     r->sel(),
                     pd,
                     r->flags() & 1 ? static_cast<void (*)()>(send_msg<ret_user_iret>) : nullptr,
                     r->cpu(),
                     r->evt(),
                     r->user_page(),
                     r->esp(),
                     (r->is_vcpu() ? Ec::CREATE_VCPU : 0)
                     | (r->use_apic_access_page() ? Ec::USE_APIC_ACCESS_PAGE : 0)
                     | (r->map_user_page_in_owner() ? Ec::MAP_USER_PAGE_IN_OWNER : 0)
                     );

    if (!Space_obj::insert_root (ec)) {
        trace (TRACE_ERROR, "%s: Non-NULL CAP (%#lx)", __func__, r->sel());
        delete ec;
        sys_finish<Sys_regs::BAD_CAP>();
    }

    sys_finish<Sys_regs::SUCCESS>();
}

void Ec::sys_create_sc()
{
    Sys_create_sc *r = static_cast<Sys_create_sc *>(current()->sys_regs());

    trace (TRACE_SYSCALL, "EC:%p SYS_CREATE SC:%#lx EC:%#lx P:%#x Q:%#x", current(), r->sel(), r->ec(), r->qpd().prio(), r->qpd().quantum());
    if (Pd *pd_parent = capability_cast<Pd>(Space_obj::lookup (r->pd()), perm_for_obj_creation (Type::SC));
        EXPECT_FALSE (not pd_parent)) {
        trace (TRACE_ERROR, "%s: Non-PD CAP (%#lx)", __func__, r->pd());
        sys_finish<Sys_regs::BAD_CAP>();
    }

    Ec *ec = capability_cast<Ec>(Space_obj::lookup (r->ec()), Ec::PERM_CREATE_SC);

    if (EXPECT_FALSE (not ec)) {
        trace (TRACE_ERROR, "%s: Non-EC CAP (%#lx)", __func__, r->ec());
        sys_finish<Sys_regs::BAD_CAP>();
    }

    if (EXPECT_FALSE (!ec->glb)) {
        trace (TRACE_ERROR, "%s: Cannot bind SC", __func__);
        sys_finish<Sys_regs::BAD_CAP>();
    }

    if (EXPECT_FALSE (!r->qpd().prio() || !r->qpd().quantum() | (r->qpd().prio() >= NUM_PRIORITIES))) {
        trace (TRACE_ERROR, "%s: Invalid QPD", __func__);
        sys_finish<Sys_regs::BAD_PAR>();
    }

    Sc *sc = new Sc (Pd::current(), r->sel(), ec, ec->cpu, r->qpd().prio(), r->qpd().quantum());
    if (!Space_obj::insert_root (sc)) {
        trace (TRACE_ERROR, "%s: Non-NULL CAP (%#lx)", __func__, r->sel());
        delete sc;
        sys_finish<Sys_regs::BAD_CAP>();
    }

    sc->remote_enqueue();

    sys_finish<Sys_regs::SUCCESS>();
}

void Ec::sys_create_pt()
{
    Sys_create_pt *r = static_cast<Sys_create_pt *>(current()->sys_regs());

    trace (TRACE_SYSCALL, "EC:%p SYS_CREATE PT:%#lx EC:%#lx EIP:%#lx", current(), r->sel(), r->ec(), r->eip());

    if (Pd *pd_parent = capability_cast<Pd>(Space_obj::lookup (r->pd()), perm_for_obj_creation (Type::PT));
        EXPECT_FALSE (not pd_parent)) {
        trace (TRACE_ERROR, "%s: Non-PD CAP (%#lx)", __func__, r->pd());
        sys_finish<Sys_regs::BAD_CAP>();
    }

    Ec *ec = capability_cast<Ec>(Space_obj::lookup (r->ec()), Ec::PERM_CREATE_PT);

    if (EXPECT_FALSE (not ec)) {
        trace (TRACE_ERROR, "%s: Non-EC CAP (%#lx)", __func__, r->ec());
        sys_finish<Sys_regs::BAD_CAP>();
    }

    if (EXPECT_FALSE (ec->glb)) {
        trace (TRACE_ERROR, "%s: Cannot bind PT", __func__);
        sys_finish<Sys_regs::BAD_CAP>();
    }

    Pt *pt = new Pt (Pd::current(), r->sel(), ec, r->mtd(), r->eip());
    if (!Space_obj::insert_root (pt)) {
        trace (TRACE_ERROR, "%s: Non-NULL CAP (%#lx)", __func__, r->sel());
        delete pt;
        sys_finish<Sys_regs::BAD_CAP>();
    }

    sys_finish<Sys_regs::SUCCESS>();
}

void Ec::sys_create_sm()
{
    Sys_create_sm *r = static_cast<Sys_create_sm *>(current()->sys_regs());

    trace (TRACE_SYSCALL, "EC:%p SYS_CREATE SM:%#lx CNT:%lu", current(), r->sel(), r->cnt());

    if (Pd *pd_parent = capability_cast<Pd>(Space_obj::lookup (r->pd()), perm_for_obj_creation (Type::SM));
        EXPECT_FALSE (not pd_parent)) {
        trace (TRACE_ERROR, "%s: Non-PD CAP (%#lx)", __func__, r->pd());
        sys_finish<Sys_regs::BAD_CAP>();
    }

    Sm *sm;

    if (r->sm()) {
        Sm *si = capability_cast<Sm>(Space_obj::lookup (r->sm()), Sm::PERM_UP);

        if (EXPECT_FALSE (not si)) {
            trace (TRACE_ERROR, "%s: Non-SM CAP (%#lx)", __func__, r->sm());
            sys_finish<Sys_regs::BAD_CAP>();
        }

        if (si->is_signal()) {
            /* limit chaining to solely one level */
            trace (TRACE_ERROR, "%s: SM CAP (%#lx) is signal", __func__, r->sm());
            sys_finish<Sys_regs::BAD_CAP>();
        }

        sm = new Sm (Pd::current(), r->sel(), 0, si, r->cnt());
    } else
        sm = new Sm (Pd::current(), r->sel(), r->cnt());

    if (!Space_obj::insert_root (sm)) {
        trace (TRACE_ERROR, "%s: Non-NULL CAP (%#lx)", __func__, r->sel());
        delete sm;
        sys_finish<Sys_regs::BAD_CAP>();
    }

    sys_finish<Sys_regs::SUCCESS>();
}

void Ec::sys_revoke()
{
    Sys_revoke *r = static_cast<Sys_revoke *>(current()->sys_regs());

    trace (TRACE_SYSCALL, "EC:%p SYS_REVOKE", current());

    Pd * pd = Pd::current();

    if (current()->cont != sys_revoke) {
        if (r->remote()) {
            Pd * remote_pd = capability_cast<Pd>(Space_obj::lookup (r->pd()));

            if (EXPECT_FALSE (not remote_pd)) {
                trace (TRACE_ERROR, "%s: Bad PD CAP (%#lx)", __func__, r->pd());
                sys_finish<Sys_regs::BAD_CAP>();
            }

            pd = remote_pd;
            if (!pd->add_ref())
                sys_finish<Sys_regs::BAD_CAP>();
        }
        current()->cont = sys_revoke;

        r->rem(pd);
    } else
        pd = reinterpret_cast<Pd *>(r->pd());

    pd->rev_crd (r->crd(), r->self());

    current()->cont = sys_finish<Sys_regs::SUCCESS>;
    r->rem(nullptr);

    if (r->remote() && pd->del_rcu())
        Rcu::call(pd);

    if (EXPECT_FALSE (r->sm())) {
        if (Sm *sm = capability_cast<Sm>(Space_obj::lookup (r->sm()), Sm::PERM_UP); sm) {
            sm->add_to_rcu();
        }
    }

    sys_finish<Sys_regs::SUCCESS>();
}

void Ec::sys_pd_ctrl_lookup()
{
    Sys_pd_ctrl_lookup *s = static_cast<Sys_pd_ctrl_lookup *>(current()->sys_regs());

    trace (TRACE_SYSCALL, "EC:%p SYS_LOOKUP T:%d B:%#lx", current(), s->crd().type(), s->crd().base());

    Space *space; Mdb *mdb;
    if ((space = Pd::current()->subspace (s->crd().type())) && (mdb = space->tree_lookup (s->crd().base())))
        s->crd() = Crd (s->crd().type(), mdb->node_base, mdb->node_order, mdb->node_attr);
    else
        s->crd() = Crd (0);

    sys_finish<Sys_regs::SUCCESS>();
}

void Ec::sys_pd_ctrl_map_access_page()
{
    Sys_pd_ctrl_map_access_page *s = static_cast<Sys_pd_ctrl_map_access_page *>(current()->sys_regs());

    trace (TRACE_SYSCALL, "EC:%p SYS_MAP_ACCESS_PAGE B:%#lx", current(), s->crd().base());

    Pd *pd  = Pd::current();
    Crd crd = s->crd();

    void *access_addr = pd->get_access_page();

    assert(pd and access_addr);

    static constexpr mword ord {0};
    static constexpr mword rights {0x3}; // R+W

    if (crd.type() != Crd::MEM or crd.attr() != rights or crd.order() != ord) {
        sys_finish<Sys_regs::BAD_PAR>();
    }

    mword access_addr_phys = Buddy::ptr_to_phys(access_addr);

    auto cleanup {pd->ept.update({crd.base() << PAGE_BITS, access_addr_phys,
                                  Ept::PTE_R | Ept::PTE_W | Ept::PTE_I | (6 /* WB */ << Ept::PTE_MT_SHIFT), PAGE_BITS})};

    // XXX Check whether TLB needs to be invalidated.
    cleanup.ignore_tlb_flush();

    sys_finish<Sys_regs::SUCCESS>();
}

void Ec::sys_pd_ctrl_delegate()
{
    Sys_pd_ctrl_delegate *s = static_cast<Sys_pd_ctrl_delegate *>(current()->sys_regs());
    Xfer xfer = s->xfer();

    trace (TRACE_SYSCALL, "EC:%p SYS_DELEGATE SRC:%#lx DST:%#lx FLAGS:%#lx",
           current, s->src_pd(), s->dst_pd(), xfer.flags());

    Pd *src_pd = capability_cast<Pd>(Space_obj::lookup (s->src_pd()));
    Pd *dst_pd = capability_cast<Pd>(Space_obj::lookup (s->dst_pd()));

    if (EXPECT_FALSE (not (src_pd and dst_pd))) {
        trace (TRACE_ERROR, "%s: Bad PD CAP SRC:%#lx DST:%#lx", __func__, s->src_pd(), s->dst_pd());
        sys_finish<Sys_regs::BAD_CAP>();
    }

    s->set_xfer (dst_pd->xfer_item (src_pd, s->dst_crd(), s->dst_crd(), xfer));

    sys_finish<Sys_regs::SUCCESS>();
}

void Ec::sys_pd_ctrl()
{
    Sys_pd_ctrl *s = static_cast<Sys_pd_ctrl *>(current()->sys_regs());
    switch (s->op()) {
    case Sys_pd_ctrl::LOOKUP:          { sys_pd_ctrl_lookup();          }
    case Sys_pd_ctrl::MAP_ACCESS_PAGE: { sys_pd_ctrl_map_access_page(); }
    case Sys_pd_ctrl::DELEGATE:        { sys_pd_ctrl_delegate();        }
    };

    sys_finish<Sys_regs::BAD_PAR>();
}

void Ec::sys_ec_ctrl()
{
    Sys_ec_ctrl *r = static_cast<Sys_ec_ctrl *>(current()->sys_regs());

    switch (r->op()) {
        case 0:
        {
            Ec *ec = capability_cast<Ec>(Space_obj::lookup (r->ec()), Ec::PERM_EC_CTRL);

            if (EXPECT_FALSE (not ec)) {
                trace (TRACE_ERROR, "%s: Bad EC CAP (%#lx)", __func__, r->ec());
                sys_finish<Sys_regs::BAD_CAP>();
            }

            if (!(ec->regs.hazard() & HZD_RECALL)) {

                ec->regs.set_hazard (HZD_RECALL);

                if (Cpu::id() != ec->cpu && Ec::remote (ec->cpu) == ec)
                    Lapic::send_ipi (ec->cpu, VEC_IPI_RKE);

            }
            break;
        }

        case 1: /* yield */
            current()->cont = sys_finish<Sys_regs::SUCCESS>;
            Sc::schedule (false, false);
            break;

        case 2: /* helping */
        {
            Ec *ec = capability_cast<Ec>(Space_obj::lookup (r->ec()));

            if (EXPECT_FALSE (not ec)) {
                sys_finish<Sys_regs::BAD_CAP>();
            }

            if (EXPECT_FALSE(ec->cpu != current()->cpu))
                sys_finish<Sys_regs::BAD_CPU>();

            if (EXPECT_FALSE(!ec->utcb || ec->blocked() || ec->partner || ec->pd != Ec::current()->pd || (r->cnt() != ec->utcb->tls)))
                sys_finish<Sys_regs::BAD_PAR>();

            current()->cont = sys_finish<Sys_regs::SUCCESS>;
            ec->make_current();

            break;
        }

        case 3: /* re-schedule */
            current()->cont = sys_finish<Sys_regs::SUCCESS>;
            Sc::schedule (false, true);
            break;

        default:
            sys_finish<Sys_regs::BAD_PAR>();
    }

    sys_finish<Sys_regs::SUCCESS>();
}

void Ec::sys_sc_ctrl()
{
    Sys_sc_ctrl *r = static_cast<Sys_sc_ctrl *>(current()->sys_regs());
    Sc *sc = capability_cast<Sc>(Space_obj::lookup (r->sc()), Sc::PERM_SC_CTRL);

    if (EXPECT_FALSE (not sc)) {
        trace (TRACE_ERROR, "%s: Bad SC CAP (%#lx)", __func__, r->sc());
        sys_finish<Sys_regs::BAD_CAP>();
    }

    r->set_time ((sc->time * 1000) / Lapic::freq_tsc);

    sys_finish<Sys_regs::SUCCESS>();
}

void Ec::sys_pt_ctrl()
{
    Sys_pt_ctrl *r = static_cast<Sys_pt_ctrl *>(current()->sys_regs());
    Pt *pt = capability_cast<Pt>(Space_obj::lookup (r->pt()), Pt::PERM_CTRL);

    if (EXPECT_FALSE (not pt)) {
        trace (TRACE_ERROR, "%s: Bad PT CAP (%#lx)", __func__, r->pt());
        sys_finish<Sys_regs::BAD_CAP>();
    }

    pt->set_id (r->id());

    sys_finish<Sys_regs::SUCCESS>();
}

void Ec::sys_sm_ctrl()
{
    Sys_sm_ctrl *r = static_cast<Sys_sm_ctrl *>(current()->sys_regs());
    Sm *sm = capability_cast<Sm>(Space_obj::lookup (r->sm()), 1U << r->op());

    if (EXPECT_FALSE (not sm)) {
        trace (TRACE_ERROR, "%s: Bad SM CAP (%#lx)", __func__, r->sm());
        sys_finish<Sys_regs::BAD_CAP>();
    }

    switch (r->op()) {

        case 0:
            sm->submit();
            break;

        case 1:
            if (sm->space == static_cast<Space_obj *>(&Pd::kern)) {
                Gsi::unmask (static_cast<unsigned>(sm->node_base - NUM_CPU));
                if (sm->is_signal())
                    break;
            }

            if (sm->is_signal())
                sys_finish<Sys_regs::BAD_CAP>();

            current()->cont = Ec::sys_finish<Sys_regs::SUCCESS, true>;
            sm->dn (r->zc(), r->time());
            break;
    }

    sys_finish<Sys_regs::SUCCESS>();
}

void Ec::sys_assign_pci()
{
    Sys_assign_pci *r = static_cast<Sys_assign_pci *>(current()->sys_regs());
    Pd *pd = capability_cast<Pd>(Space_obj::lookup (r->pd()));

    if (EXPECT_FALSE (not pd)) {
        trace (TRACE_ERROR, "%s: Non-PD CAP (%#lx)", __func__, r->pd());
        sys_finish<Sys_regs::BAD_CAP>();
    }

    Paddr phys; unsigned rid;
    if (EXPECT_FALSE (!pd->Space_mem::lookup (r->dev(), &phys) || (rid = Pci::phys_to_rid (phys)) == ~0U)) {
        trace (TRACE_ERROR, "%s: Non-DEV CAP (%#lx)", __func__, r->dev());
        sys_finish<Sys_regs::BAD_DEV>();
    }

    Dmar *dmar = Pci::find_dmar (r->hnt());
    if (EXPECT_FALSE (!dmar)) {
        trace (TRACE_ERROR, "%s: Invalid Hint (%#lx)", __func__, r->hnt());
        sys_finish<Sys_regs::BAD_DEV>();
    }

    dmar->assign (rid, pd);

    sys_finish<Sys_regs::SUCCESS>();
}

void Ec::sys_assign_gsi()
{
    Sys_assign_gsi *r = static_cast<Sys_assign_gsi *>(current()->sys_regs());

    if (EXPECT_FALSE (!Hip::cpu_online (r->cpu()))) {
        trace (TRACE_ERROR, "%s: Invalid CPU (%#x)", __func__, r->cpu());
        sys_finish<Sys_regs::BAD_CPU>();
    }

    Sm *sm = capability_cast<Sm>(Space_obj::lookup (r->sm()));

    if (EXPECT_FALSE (not sm)) {
        trace (TRACE_ERROR, "%s: Non-SM CAP (%#lx)", __func__, r->sm());
        sys_finish<Sys_regs::BAD_CAP>();
    }

    if (EXPECT_FALSE (sm->space != static_cast<Space_obj *>(&Pd::kern))) {
        trace (TRACE_ERROR, "%s: Non-GSI SM (%#lx)", __func__, r->sm());
        sys_finish<Sys_regs::BAD_CAP>();
    }

    if (r->si() != ~0UL) {
        Sm *si = capability_cast<Sm>(Space_obj::lookup (r->si()));

        if (EXPECT_FALSE (not si)) {
            trace (TRACE_ERROR, "%s: Non-SI CAP (%#lx)", __func__, r->si());
            sys_finish<Sys_regs::BAD_CAP>();
        }

        if (si == sm) {
            sm->chain(nullptr);
            sys_finish<Sys_regs::SUCCESS>();
        }

        if (EXPECT_FALSE (si->space == static_cast<Space_obj *>(&Pd::kern))) {
            trace (TRACE_ERROR, "%s: Invalid-SM CAP (%#lx)", __func__, r->si());
            sys_finish<Sys_regs::BAD_CAP>();
        }

        sm->chain(si);
    }

    Paddr phys; unsigned rid = 0, gsi = static_cast<unsigned>(sm->node_base - NUM_CPU);
    if (EXPECT_FALSE (!Gsi::gsi_table[gsi].ioapic && (!Pd::current()->Space_mem::lookup (r->dev(), &phys) || ((rid = Pci::phys_to_rid (phys)) == ~0U && (rid = Hpet::phys_to_rid (phys)) == ~0U)))) {
        trace (TRACE_ERROR, "%s: Non-DEV CAP (%#lx)", __func__, r->dev());
        sys_finish<Sys_regs::BAD_DEV>();
    }

    r->set_msi (Gsi::set (gsi, r->cpu(), rid));

    sys_finish<Sys_regs::SUCCESS>();
}

extern "C"
void (*const syscall[])() =
{
    &Ec::sys_call,
    &Ec::sys_reply,
    &Ec::sys_create_pd,
    &Ec::sys_create_ec,
    &Ec::sys_create_sc,
    &Ec::sys_create_pt,
    &Ec::sys_create_sm,
    &Ec::sys_revoke,
    &Ec::sys_pd_ctrl,
    &Ec::sys_ec_ctrl,
    &Ec::sys_sc_ctrl,
    &Ec::sys_pt_ctrl,
    &Ec::sys_sm_ctrl,
    &Ec::sys_assign_pci,
    &Ec::sys_assign_gsi,
    &Ec::sys_finish<Sys_regs::BAD_HYP>,
};

template void Ec::sys_finish<Sys_regs::COM_ABT>();
template void Ec::send_msg<Ec::ret_user_vmresume>();
template void Ec::send_msg<Ec::ret_user_vmrun>();
template void Ec::send_msg<Ec::ret_user_iret>();
