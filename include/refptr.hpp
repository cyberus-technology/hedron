/*
 * Reference-Counted Pointer
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

#include "atomic.hpp"
#include "rcu.hpp"

class Refcount
{
private:
    uint32 ref{1};

public:
    /// Add a new reference to the reference count.
    ///
    /// When this function returns false, we tried to add a new reference to
    /// an object that already reached a reference count of zero.
    bool add_ref()
    {
        for (uint32 r; (r = ref);)
            if (Atomic::cmp_swap(ref, r, r + 1))
                return true;

        return false;
    }

    /// Remove a reference from the reference count.
    ///
    /// When this function returns true, we removed the last reference and
    /// the object must be freed by the caller with a call to Rcu::call().
    bool del_ref() { return Atomic::sub(ref, 1U) == 0; }

    /// Return true, if this is the only reference to the object.
    bool last_ref() { return Atomic::load(ref) == 1; }

    /// Delete a reference count similar to del_ref() but already reports
    /// the object ready for destruction when the reference count goes to
    /// one and the caller holds the last reference.
    ///
    /// **Note:** The semantics here are hard to describe and finding a
    /// better interface would be appreciated.
    bool del_rcu()
    {
        if (last_ref())
            return true;

        if (del_ref()) {
            ref = 1;
            return true;
        }

        return false;
    }
};

template <typename T> class Refptr
{
private:
    // A pointer to a reference-counted object.
    T* ptr{nullptr};

public:
    // Prevent default copy operations to avoid letting the reference count
    // go out of sync.
    Refptr& operator=(Refptr const&) = delete;
    Refptr(Refptr const&) = delete;

    operator T*() const
    {
        assert_slow(ptr != nullptr);
        return ptr;
    }

    T* operator->() const
    {
        assert_slow(ptr != nullptr);
        return ptr;
    }

    Refptr() = default;
    Refptr(T* p) : ptr(p != nullptr and p->add_ref() ? p : nullptr) {}

    ~Refptr() { release(); }

    /// Returns the stored pointer.
    T* get() const { return ptr; }

    operator bool() const { return ptr != nullptr; }

    /// Releases the stored pointer, i.e. decreases the reference count of the stored pointer and clears the
    /// stored pointer. The returned pointer may be used until the next RCU quiescent state (transition to
    /// userspace). Afterwards it is not safe to use the returned pointer.
    ///
    /// @return nullptr if the stored pointer was a nullptr or if this was the last reference to the object.
    /// The stored pointer otherwise.
    T* release()
    {
        if (ptr == nullptr) {
            return nullptr;
        }

        if (ptr->del_rcu()) {
            Rcu::call(ptr);
            return nullptr;
        }

        T* old{ptr};
        ptr = nullptr;
        return old;
    }

    /// Resets the stored pointer to the given pointer. This includes decreasing the reference count of the
    /// stored pointer and increasing the reference count of the given pointer.
    ///
    /// @param new_ptr The pointer to store. If new_ptr->add_ref() returns false, the stored pointer will be a
    /// nullptr.
    void reset(T* new_ptr = nullptr)
    {
        release();

        if (new_ptr == nullptr) {
            return;
        }

        if (new_ptr->add_ref()) {
            ptr = new_ptr;
        }
    }
};
