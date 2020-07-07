/*
 * MSR Bitmaps for VMX
 *
 * Copyright (C) 2020 Markus Partheymüller, Cyberus Technology GmbH.
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

#include "assert.hpp"
#include "memory.hpp"
#include "msr.hpp"
#include "page_alloc_policy.hpp"
#include "string.hpp"

/**
 * VMX MSR Bitmap abstraction
 *
 * Convience wrapper around a raw bitmap for MSRs
 * 0 - 0x1FFF and 0xC0000000 - 0xC00001FFF.
 *
 * MSRs can be set to cause VM exits or are accessible directly by the guest.
 * Those settings can be configured separately for read and write accesses.
 */
template <typename PAGE_ALLOC>
class Generic_vmx_msr_bitmap
{
    public:
        Generic_vmx_msr_bitmap()
        {
            // We want the default to be to exit on all MSR accesses, so we
            // initialize the bitmap to all ones.
            memset(bitmap, 0xFF, PAGE_SIZE);
        }

        /// Sets the given MSR to be directly readable and writable by the guest
        void set_passthrough(Msr::Register msr)
        {
            set(msr, false, false);
        }

        /// Sets the given MSR to cause a VM exit on all accesses
        void set_exit(Msr::Register msr)
        {
            set(msr, true, true);
        }

        /// Sets the given MSR to be directly readable by the guest
        void set_read_passthrough(Msr::Register msr)
        {
            set(msr, false, exits_on_write(msr));
        }

        /// Sets the given MSR to cause a VM exit upon read
        void set_read_exit(Msr::Register msr)
        {
            set(msr, true, exits_on_write(msr));
        }

        /// Sets the given MSR to be directly writable by the guest
        void set_write_passthrough(Msr::Register msr)
        {
            set(msr, exits_on_read(msr), false);
        }

        /// Sets the given MSR to cause a VM exit upon write
        void set_write_exit(Msr::Register msr)
        {
            set(msr, exits_on_read(msr), true);
        }

        /// Returns the resulting physical address to write to the VMCS
        mword phys_addr()
        {
            return PAGE_ALLOC::pointer_to_phys(reinterpret_cast<mword*>(this));
        }

        static void *operator new (size_t)
        {
            return static_cast<void*>(PAGE_ALLOC::alloc_zeroed_page());
        }

        static void operator delete (void *ptr)
        {
            PAGE_ALLOC::free_page(reinterpret_cast<mword*>(ptr));
        }

    private:
        void set(Msr::Register msr, bool exit_read, bool exit_write)
        {
            const size_t bit_pos {bit_position(msr)};
            bitmap[read_index(msr)] &= ~(1u << bit_pos);
            bitmap[read_index(msr)] |= exit_read * (1u << bit_pos);

            bitmap[write_index(msr)] &= ~(1u << bit_pos);
            bitmap[write_index(msr)] |= exit_write * (1u << bit_pos);
        }

        // low  MSRs (<= 0x1FFF)     READ:  bits     0 -  8191
        // high MSRs (>= 0xC0000000) READ:  bits  8192 - 16383
        // low  MSRs (<= 0x1FFF)     WRITE: bits 16384 - 24575
        // high MSRs (>= 0xC0000000) WRITE: bits 24576 - 32767

        size_t bit_position(Msr::Register msr) const
        {
            return msr % (sizeof(bitmap[0]) * 8);
        }

        size_t read_index(Msr::Register msr) const
        {
            assert(msr <= 0x1FFF or (msr >= 0xC0000000 and msr <= 0xC0001FFF));

            const size_t idx {(msr & 0x1FFF) / sizeof(bitmap[0]) / 8 +
                !!(msr & 0xC0000000) * (8192 / sizeof(bitmap[0]) / 8)};
            assert(idx < PAGE_SIZE / sizeof(bitmap[0]));
            return idx;
        }

        size_t write_index(Msr::Register msr) const
        {
            assert(msr <= 0x1FFF or (msr >= 0xC0000000 and msr <= 0xC0001FFF));

            const size_t idx {read_index(msr) + (16384 / sizeof(bitmap[0]) / 8)};
            assert(idx < PAGE_SIZE / sizeof(bitmap[0]));
            return idx;
        }

        bool exits_on_read(Msr::Register msr) const
        {
            return bitmap[read_index(msr)] & (1u << bit_position(msr));
        }

        bool exits_on_write(Msr::Register msr) const
        {
            return bitmap[write_index(msr)] & (1u << bit_position(msr));
        }

        alignas(PAGE_SIZE) unsigned bitmap[PAGE_SIZE];
};

using Vmx_msr_bitmap = Generic_vmx_msr_bitmap<Page_alloc_policy<>>;
