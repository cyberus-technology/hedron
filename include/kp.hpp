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

#pragma once

#include "kobject.hpp"
#include "memory.hpp"
#include "slab.hpp"
#include "spinlock.hpp"

class Pd;

// Kernel Page
//
// A kernel page represents a page of hypervisor memory shared with user space.
class Kp : public Typed_kobject<Kobject::Type::KP>, public Refcount
{
private:
    static Slab_cache cache;

    static void free(Rcu_elem* a)
    {
        Kp* kp{static_cast<Kp*>(a)};
        delete kp;
    }

    // The value of addr_in_user_space must be smaller than this value,
    // otherwise the address is non-canonical or a kernel address. This value
    // is used to check whether the kernel page is mapped.
    static constexpr mword INVALID_USER_ADDR{USER_ADDR + 1};

    // Spinlock to prevent multiple threads from concurrently adding/removing
    // user mappings
    Spinlock lock;

    // The kernel memory of this kernel page.
    void* data;
    // The pd that has a user space mapping for this kernel page.
    Pd* pd_user_page{nullptr};
    // The address of this kernel page in user space. If this value is greater
    // or equals INVALID_USER_ADDR, the kernel page is not mapped. Otherwise it
    // is mapped.
    mword addr_in_user_space{INVALID_USER_ADDR};

    // Checks if a valid user mapping for this kernel page exists.
    bool has_user_mapping() const;

    // The kernel virtual address of the page represented by this object.
    mword kernel_address() const { return reinterpret_cast<mword>(data); }

    // The user virtual address of the page represented by this object.
    mword user_address() const { return addr_in_user_space; }

public:
    // Capability permission bitmask.
    enum
    {
        PERM_KP_CTRL = 1U << 0,

        PERM_ALL = PERM_KP_CTRL,
    };

    // This constructor is just a workaround because the FPU needs a kpage and we currently can't provide a
    // kpage in the create_ec syscall (because all the possible parameters are used). As soon as this issue is
    // solved this constructor has to be removed, because a kpage that the user space has no access to makes
    // little to no sense.
    Kp(Pd* own);

    Kp(Pd* own, mword sel);
    ~Kp();

    // The data page that is shared with userspace.
    //
    // Code using the returned pointer must take care to co-exist with userspace reading and writing to the
    // same memory concurrently.
    void* data_page() const { return data; }

    // Adds a user space mapping for this kernel page. This includes adding a
    // RCU reference to the destination PD and mapping the memory at the given
    // address.
    // Returns true if the mapping was successful, i.e. if no mapping
    // existed and if the given address is valid. Otherwise returns false.
    bool add_user_mapping(Pd* pd, mword addr);

    // Removes the current user space mapping. If no user space mapping exists,
    // this function does nothing and returns false. Returns true otherwise.
    bool remove_user_mapping();

    static inline void* operator new(size_t) { return cache.alloc(); }
    static inline void operator delete(void* ptr) { cache.free(ptr); }
};
