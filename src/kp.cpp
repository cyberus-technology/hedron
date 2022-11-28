/*
 * Kernel Page
 *
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

#include "kp.hpp"
#include "cpu.hpp"
#include "ec.hpp"
#include "hpt.hpp"
#include "lock_guard.hpp"
#include "pd.hpp"
#include "stdio.hpp"

INIT_PRIORITY(PRIO_SLAB)
Slab_cache Kp::cache(sizeof(Kp), 32);

Kp::Kp(Pd* own) : Typed_kobject(static_cast<Space_obj*>(own)), data(Buddy::allocator.alloc(0, Buddy::FILL_0))
{
    trace(TRACE_SYSCALL, "KP: %p without selector created (PD:%p, Data: %p)", this, own, data);
}

Kp::Kp(Pd* own, mword sel)
    : Typed_kobject(static_cast<Space_obj*>(own), sel, PERM_ALL, free),
      data(Buddy::allocator.alloc(0, Buddy::FILL_0))
{
    trace(TRACE_SYSCALL, "KP: %p created (PD:%p, Data:%p)", this, own, data);
}

Kp::~Kp()
{
    remove_user_mapping();

    if (data != nullptr) {
        Buddy::allocator.free(reinterpret_cast<mword>(data));
        data = nullptr;
    }
}

bool Kp::has_user_mapping() const { return pd_user_page and addr_in_user_space <= INVALID_USER_ADDR; }

bool Kp::add_user_mapping(Pd* pd, mword addr)
{
    Tlb_cleanup cleanup;

    {
        Lock_guard<Spinlock> guard(lock);

        if (has_user_mapping()) {
            return false;
        }

        if ((addr & PAGE_MASK) != 0 or addr >= INVALID_USER_ADDR) {
            return false;
        }

        pd_user_page = pd;
        if (!pd_user_page->add_ref()) {
            pd_user_page = nullptr;
            return false;
        }

        addr_in_user_space = addr;

        cleanup = pd_user_page->Space_mem::insert(
            user_address(), 0, Hpt::PTE_NODELEG | Hpt::PTE_NX | Hpt::PTE_U | Hpt::PTE_W | Hpt::PTE_P,
            Buddy::ptr_to_phys(data));
    }

    if (cleanup.need_tlb_flush()) {
        // We don't want to access pd_user_page in an unsynchronized scope, thus we use the pd variable for
        // the shootdown. At this point we already checked whether we can use the pd.
        pd->stale_host_tlb.merge(pd->Space_mem::cpus);
        pd->Space_mem::shootdown();
    }

    return true;
}

bool Kp::remove_user_mapping()
{
    // When we are doing the shootdown, pd_user_page will be a nullptr. Thus we capture the value of
    // pd_user_page inside the synchronized scope.
    Pd* pd;
    Tlb_cleanup cleanup;

    {
        Lock_guard<Spinlock> guard(lock);

        if (!has_user_mapping()) {
            return false;
        }

        if (pd_user_page->del_rcu()) {
            Rcu::call(pd_user_page);
        }

        // Check if the physical addresses of the kernel virtual address and the user virtual address are not
        // the same. If this is the case, the mapping of this kernel page has been overwritten using
        // Pd::delegate. In this case we only output a warning message.
        if (Paddr kernel_paddr, user_paddr;
            pd_user_page->Space_mem::lookup(kernel_address(), &kernel_paddr) and
            pd_user_page->Space_mem::lookup(user_address(), &user_paddr) and kernel_paddr != user_paddr) {
            trace(TRACE_ERROR,
                  "%s: User space mapping of KP has been overwritten prior to removing the mapping (CAP: %p)",
                  __func__, this);
        }

        // We remove the user space mapping unconditionally to avoid race conditions. Specifically, user space
        // can overmap the kpage mapping between the check above and a conditional `insert`.
        cleanup = pd_user_page->Space_mem::insert(user_address(), 0, 0, 0);

        pd = pd_user_page;
        pd_user_page = nullptr;
        addr_in_user_space = INVALID_USER_ADDR;
    }

    if (cleanup.need_tlb_flush()) {
        pd->stale_host_tlb.merge(pd->Space_mem::cpus);
        pd->Space_mem::shootdown();
    }

    return true;
}
