/*
 * MSR Bitmaps for VMX
 *
 * Copyright (C) 2020 Markus Partheymüller, Cyberus Technology GmbH.
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

#include "assert.hpp"
#include "bitmap.hpp"
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
template <typename PAGE_ALLOC> class Generic_vmx_msr_bitmap
{
public:
    enum exit_setting
    {
        EXIT_NEVER = 1u << 0,
        EXIT_READ = 1u << 1,
        EXIT_WRITE = 1u << 2,
        EXIT_ALWAYS = EXIT_READ | EXIT_WRITE,
    };

    /// Sets the respective MSR to exit according to the exit setting
    void set_exit(Msr::Register msr, exit_setting exit)
    {
        set(msr, exit & exit_setting::EXIT_READ, exit & exit_setting::EXIT_WRITE);
    }

    /// Returns the resulting physical address to write to the VMCS
    mword phys_addr() { return PAGE_ALLOC::pointer_to_phys(reinterpret_cast<mword*>(this)); }

    static void* operator new(size_t) { return static_cast<void*>(PAGE_ALLOC::alloc_zeroed_page()); }

    static void operator delete(void* ptr) { PAGE_ALLOC::free_page(reinterpret_cast<mword*>(ptr)); }

private:
    // low  MSRs (<= 0x1FFF)     READ:  bits     0 -  8191
    // high MSRs (>= 0xC0000000) READ:  bits  8192 - 16383
    // low  MSRs (<= 0x1FFF)     WRITE: bits 16384 - 24575
    // high MSRs (>= 0xC0000000) WRITE: bits 24576 - 32767
    void set(Msr::Register msr, bool exit_read, bool exit_write)
    {
        assert(msr <= 0x1FFF or (msr >= 0xC0000000 and msr <= 0xC0001FFF));

        const bool high{msr >= 0xC0000000};

        auto& bitmap_read{high ? bitmap_read_high : bitmap_read_low};
        auto& bitmap_write{high ? bitmap_write_high : bitmap_write_low};

        bitmap_read[msr & 0x1FFF] = exit_read;
        bitmap_write[msr & 0x1FFF] = exit_write;
    }

    // We want the default to be to exit on all MSR accesses, so we
    // initialize the bitmaps to all ones.
    Bitmap<unsigned, 8192> bitmap_read_low{true};
    Bitmap<unsigned, 8192> bitmap_read_high{true};
    Bitmap<unsigned, 8192> bitmap_write_low{true};
    Bitmap<unsigned, 8192> bitmap_write_high{true};
};

using Vmx_msr_bitmap = Generic_vmx_msr_bitmap<Page_alloc_policy<>>;
static_assert(sizeof(Vmx_msr_bitmap) == PAGE_SIZE, "MSR bitmap has to fit in a single page!");
