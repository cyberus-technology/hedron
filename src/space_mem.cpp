/*
 * Memory Space
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

#include "counter.hpp"
#include "dmar.hpp"
#include "hazards.hpp"
#include "hip.hpp"
#include "lapic.hpp"
#include "lock_guard.hpp"
#include "mtrr.hpp"
#include "pd.hpp"
#include "scope_guard.hpp"
#include "space.hpp"
#include "stdio.hpp"
#include "vectors.hpp"

unsigned Space_mem::did_ctr;

void Space_mem::init(unsigned cpu) { cpus.set(cpu); }

// Valid user mappings are below the canonical boundary and naturally aligned.
static bool is_valid_user_mapping(mword vaddr, mword ord)
{
    return vaddr < USER_ADDR and ord <= static_cast<mword>(max_order(vaddr, USER_ADDR)) and
           (vaddr & ((1UL << ord) - 1)) == 0;
}

// Find the source mapping at snd_cur in the given position.
static Hpt::Mapping lookup_and_adjust_rights(Space_mem* snd, mword snd_cur, mword snd_end, mword hw_attr)
{
    bool const is_unmap{(hw_attr & Hpt::PTE_P) == 0};
    Hpt::Mapping const empty_mapping{snd_cur, 0, 0, static_cast<Hpt::ord_t>(max_order(snd_cur, snd_end))};
    Hpt::Mapping mapping{is_unmap ? empty_mapping : snd->Space_mem::hpt.lookup(snd_cur)};

    if (mapping.present() and ((mapping.attr & Hpt::PTE_NODELEG) or not(mapping.attr & Hpt::PTE_U))) {
        trace(TRACE_ERROR, "Refusing to map region %#016lx ord %d", mapping.vaddr, mapping.order);
        mapping = empty_mapping;
    }

    mapping.attr = Hpt::merge_hw_attr(mapping.attr, hw_attr);
    return mapping;
}

// Addresses are in byte-granularity.
Delegate_result_void Space_mem::delegate(Tlb_cleanup& cleanup, Space_mem* snd, mword snd_base, mword rcv_base,
                                         mword ord, mword attr, mword sub)
{
    assert(ord >= PAGE_BITS);

    if (EXPECT_FALSE(not is_valid_user_mapping(snd_base, ord) or not is_valid_user_mapping(rcv_base, ord))) {
        trace(TRACE_ERROR, "INVALID MEM SB:%#016lx RB:%#016lx O:%#04lx A:%#lx S:%#lx", snd_base, rcv_base,
              ord, attr, sub);

        // We can only return an error here once we have propagated the error to the system call layer. In any
        // case, this would be a breaking API change, because userspace never saw these errors before and it
        // needs to handle these now.
        //
        // See #229.
        //
        // return Err(Delegate_error::invalid_mapping());
        return Ok_void({});
    }

    // Regardless of whether the operation was a success, we must take care of the TLB to not leave old
    // mappings around, even if we only managed a partial page table update.
    Scope_guard g{[this, &cleanup, sub] {
        if (cleanup.need_tlb_flush()) {
            if (sub & Space::SUBSPACE_DEVICE) {
                Dmar::flush_all_contexts();
            }
            if (sub & Space::SUBSPACE_GUEST) {
                stale_guest_tlb.merge(cpus);
            }
            if (sub & Space::SUBSPACE_HOST) {
                stale_host_tlb.merge(cpus);
            }
        }
    }};

    Hpt::pte_t const hw_attr{Hpt::hw_attr(attr)};
    mword const snd_end{snd_base + (1ULL << ord)};

    for (mword snd_cur{snd_base}; snd_cur < snd_end;) {
        // The source mapping with the correct downgraded rights.
        auto const mapping{lookup_and_adjust_rights(snd, snd_cur, snd_end, hw_attr)};

        // The source mapping chopped down to fit in the send window.
        auto const clamped{mapping.clamp(snd_base, static_cast<Hpt::ord_t>(ord))};

        // The mapping as we want to put it into the destination page tables.
        auto const target_mapping{clamped.move_by(rcv_base - snd_base)};
        assert(Hpt::attr_to_pat(target_mapping.attr) == 0);

        if (EXPECT_FALSE(target_mapping.present() and
                         (target_mapping.paddr + target_mapping.size() > (1ULL << Cpu::maxphyaddr_ord())))) {
            trace(TRACE_ERROR,
                  "Declining to map physical region %#lx+%#lx because it is beyond MAXPHYADDR (2^%u)",
                  target_mapping.paddr, target_mapping.size(), Cpu::maxphyaddr_ord());
            return Err(Delegate_error::invalid_mapping());
        }

        if (sub & Space::SUBSPACE_DEVICE) {
            // We would only want to call `cleanup.flush_tlb_later();` explicitly if the Caching Mode of the
            // IOMMU is set to 1, which implies that even non-present and invalid mappings may be cached. For
            // all other cases the generic_page_table code should already call `flush_tlb_later()` when
            // necessary. We cannot easily access this information here, which is why we always explicitly
            // kick off the flush.
            cleanup.flush_tlb_later();

            TRY_OR_RETURN(dpt.update(cleanup, Dpt::convert_mapping(target_mapping)));
        }

        if (sub & Space::SUBSPACE_GUEST) {
            TRY_OR_RETURN(ept.update(cleanup, Ept::convert_mapping(target_mapping)));
        }

        if (sub & Space::SUBSPACE_HOST) {
            TRY_OR_RETURN(hpt.update(cleanup, target_mapping));
        }

        assert(clamped.size() >= target_mapping.size());
        snd_cur = clamped.vaddr + target_mapping.size();
    }

    return Ok_void({});
}

void Space_mem::revoke(Tlb_cleanup& cleanup, mword vaddr, mword ord, mword attr)
{
    auto const all_mem_rights{Mdb::MEM_R | Mdb::MEM_W | Mdb::MEM_X};

    if ((attr & all_mem_rights) != all_mem_rights) {
        trace(TRACE_ERROR,
              "Partial memory rights revocation is not supported: Revoking everything! (%#lx %ld %#lx)",
              vaddr, ord, attr);
    }

    delegate(cleanup, this, vaddr, vaddr, ord, 0,
             Space::SUBSPACE_HOST | Space::SUBSPACE_DEVICE | Space::SUBSPACE_GUEST)
        .unwrap("Failed to revoke memory");
}

void Space_mem::shootdown()
{
    Bitmap<uint32, NUM_CPU> stale_cpus{false};

    // Collect all the TLB shootdown counters and send IPIs. These counters are increased by each CPU when it
    // receives the IPI. See Sc::rke_handler for more details.
    for (unsigned cpu = 0; cpu < NUM_CPU; cpu++) {
        if (!Hip::cpu_online(cpu)) {
            continue;
        }

        // We check whatever PD is currently running on the remote CPU. This may be another PD than what we
        // intended the shootdown for.
        //
        // If the PD is not the one we are actually interested in, the one we are interested in will
        // invalidate its TLB lazily when it is activated via Pd::make_current.
        Pd* pd = Pd::remote(cpu);

        // We still send a shootdown IPI, if this PD that we find has stale TLBs on the given CPU. This
        // happens regardless of whether this is the intended PD. This is a left-over from the past, where
        // revoke could recursively unmap memory from multiple PDs.
        if (!pd->stale_host_tlb.chk(cpu) && !pd->stale_guest_tlb.chk(cpu))
            continue;

        // There is a special case for the current CPU. We don't need to send an IPI, because before user code
        // executes again, it will check its hazards and do the TLB flush then.
        if (Cpu::id() == cpu) {
            Cpu::hazard() |= HZD_SCHED;
            continue;
        }

        // Take a snapshot of the shootdown IPI counter from the remote CPU and remember that we have to wait
        // for this CPU to receive it. See the comment at the while loop below.
        tlb_shootdown()[cpu] = Counter::remote_tlb_shootdown(cpu);
        stale_cpus[cpu] = true;

        Lapic::send_nmi(cpu);
    }

    // We have to open interrupts here, because otherwise we could deadlock with other CPUs sending us IPIs
    // and waiting for a result.
    asm volatile("sti" : : : "memory");

    // Wait for IPIs to arrive.
    for (unsigned cpu = 0; cpu < NUM_CPU; cpu++) {
        // Only CPUs to which we sent an IPI are interesting.
        if (!stale_cpus[cpu]) {
            continue;
        }

        // Once the remote CPU has received the IPI, we will break out of this loop. It doesn't matter whether
        // the remote CPU receives the IPI we sent or whether another CPU is doing a shootdown as well and its
        // IPI arrived first. We only need the other CPU to go through Sc::rke_handler.
        while (Counter::remote_tlb_shootdown(cpu) == tlb_shootdown()[cpu]) {
            relax();
        }
    }

    asm volatile("cli" : : : "memory");
}

static void map_typed_range(Hpt& hpt, Paddr start, Paddr end, Hpt::pte_t attr, unsigned t)
{
    assert((t & ~Hpt::MT_MASK) == 0);
    assert((attr & Hpt::PTE_MT_MASK) == 0);

    Hpt::pte_t const combined_attr{attr | (static_cast<Hpt::pte_t>(t) << Hpt::PTE_MT_SHIFT)};
    Paddr size;

    for (Paddr cur{start}; cur < end; cur += size) {
        auto order{min<Hpt::ord_t>(hpt.max_order(), static_cast<Hpt::ord_t>(max_order(cur, end - cur)))};
        assert(order >= PAGE_BITS);

        size = static_cast<Paddr>(1) << order;

        hpt.update({cur, cur, combined_attr, order}).ignore_tlb_flush();
    }
}

void Space_mem::insert_root(uint64 start, uint64 end, mword attr)
{
    assert(static_cast<Pd*>(this) == &Pd::kern);

    start <<= PAGE_BITS;
    end <<= PAGE_BITS;

    for (Paddr cur{start}; cur < end;) {
        uint64 next;
        unsigned t = Mtrr_state::get().memtype(cur, next);

        map_typed_range(hpt, cur, min<uint64>(next, end), Hpt::hw_attr(attr), t);
        cur = next;
    }
}

void Space_mem::claim(mword virt, unsigned o, mword attr, Paddr phys, bool exclusive)
{
    assert(static_cast<Pd*>(this) == &Pd::kern);
    assert(&Hpt::boot_hpt() != &hpt);
    assert(is_page_aligned(virt));
    assert(is_page_aligned(phys));
    assert(o >= PAGE_BITS);

    Hpt::boot_hpt().update({virt, phys, attr, static_cast<Hpt::ord_t>(o)});

    if (exclusive) {
        hpt.update({virt, phys, 0, static_cast<Hpt::ord_t>(o)});
    }
}

void Space_mem::claim_mmio_page(mword virt, Paddr phys, bool exclusive)
{
    claim(virt, PAGE_BITS, Hpt::PTE_NX | Hpt::PTE_G | Hpt::PTE_UC | Hpt::PTE_W | Hpt::PTE_P, phys, exclusive);
}
