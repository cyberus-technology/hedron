/*
 * DMA Remapping Unit (DMAR)
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

#include "algorithm.hpp"
#include "assert.hpp"
#include "config.hpp"
#include "list.hpp"
#include "slab.hpp"
#include "util.hpp"
#include "x86.hpp"

class Pd;

class Dmar_qi
{
private:
    uint64 lo, hi;

public:
    Dmar_qi(uint64 l = 0, uint64 h = 0) : lo(l), hi(h) {}
};

class Dmar_qi_ctx : public Dmar_qi
{
public:
    Dmar_qi_ctx() : Dmar_qi(0x1 | 1UL << 4) {}
};

class Dmar_qi_tlb : public Dmar_qi
{
public:
    Dmar_qi_tlb() : Dmar_qi(0x2 | 1UL << 4) {}
};

class Dmar_qi_iec : public Dmar_qi
{
public:
    Dmar_qi_iec() : Dmar_qi(0x4 | 1UL << 4) {}
};

class Dmar_ctx
{
private:
    uint64 lo, hi;

public:
    inline bool present() const { return lo & 1; }

    inline Paddr addr() const { return static_cast<Paddr>(lo) & ~PAGE_MASK; }

    inline void set(uint64 h, uint64 l)
    {
        hi = h;
        lo = l;
        clflush(this);
    }

    static inline void* operator new(size_t)
    {
        return clflush(Buddy::allocator.alloc(0, Buddy::FILL_0), PAGE_SIZE);
    }
};

class Dmar_irt
{
private:
    uint64 lo, hi;

public:
    enum
    {
        // The size of an IRT entry as a power of two.
        ENTRY_SIZE_ORDER = 4,

        // The IRT has 2^NUM_ENTRIES_ORDER entries.
        NUM_ENTRIES_ORDER = 15,
    };

    inline void set(uint64 h, uint64 l)
    {
        hi = h;
        lo = l;
        clflush(this);
    }

    static inline void* operator new(size_t)
    {
        constexpr unsigned IRT_SIZE_ORDER{NUM_ENTRIES_ORDER + ENTRY_SIZE_ORDER};
        static_assert(IRT_SIZE_ORDER >= PAGE_BITS);

        return clflush(
            // The allocator takes the allocation size as order of pages.
            Buddy::allocator.alloc(IRT_SIZE_ORDER - PAGE_BITS, Buddy::FILL_0), 1 << IRT_SIZE_ORDER);
    }
};
static_assert(sizeof(Dmar_irt) == 1 << Dmar_irt::ENTRY_SIZE_ORDER);

class Dmar : public Forward_list<Dmar>
{
private:
    mword const reg_base;
    uint64 cap;
    uint64 ecap;
    Dmar_qi* invq;
    unsigned invq_idx;

    static Dmar_ctx* ctx;
    static Dmar_irt* irt;
    static uint32 gcmd;

    static Dmar* list;
    static Slab_cache cache;

    static unsigned const ord = 0;
    static unsigned const cnt = (PAGE_SIZE << ord) / sizeof(Dmar_qi);

    enum Reg
    {
        REG_VER = 0x0,
        REG_CAP = 0x8,
        REG_ECAP = 0x10,
        REG_GCMD = 0x18,
        REG_GSTS = 0x1c,
        REG_RTADDR = 0x20,
        REG_CCMD = 0x28,
        REG_FSTS = 0x34,
        REG_FECTL = 0x38,
        REG_FEDATA = 0x3c,
        REG_FEADDR = 0x40,
        REG_IQH = 0x80,
        REG_IQT = 0x88,
        REG_IQA = 0x90,
        REG_IRTA = 0xb8,
    };

    enum Tlb
    {
        REG_IVA = 0x0,
        REG_IOTLB = 0x8,
    };

    enum Cmd
    {
        GCMD_SIRTP = 1UL << 24,
        GCMD_IRE = 1UL << 25,
        GCMD_QIE = 1UL << 26,
        GCMD_SRTP = 1UL << 30,
        GCMD_TE = 1UL << 31,
    };

    inline unsigned nfr() const { return static_cast<unsigned>(cap >> 40 & 0xff) + 1; }

    inline mword fro() const { return static_cast<mword>(cap >> 20 & 0x3ff0) + reg_base; }

    inline mword iro() const { return static_cast<mword>(ecap >> 4 & 0x3ff0) + reg_base; }

    inline unsigned ir() const { return static_cast<unsigned>(ecap) & 0x8; }

    inline unsigned qi() const { return static_cast<unsigned>(ecap) & 0x2; }

    // Return the number of supported page table levels.
    int page_table_levels() const;

    template <typename T> inline T read(Reg reg) { return *reinterpret_cast<T volatile*>(reg_base + reg); }

    template <typename T> inline void write(Reg reg, T val)
    {
        *reinterpret_cast<T volatile*>(reg_base + reg) = val;
    }

    template <typename T> inline T read(Tlb tlb) { return *reinterpret_cast<T volatile*>(iro() + tlb); }

    template <typename T> inline void write(Tlb tlb, T val)
    {
        *reinterpret_cast<T volatile*>(iro() + tlb) = val;
    }

    inline void read(unsigned frr, uint64& hi, uint64& lo)
    {
        lo = *reinterpret_cast<uint64 volatile*>(fro() + frr * 16);
        hi = *reinterpret_cast<uint64 volatile*>(fro() + frr * 16 + 8);
        *reinterpret_cast<uint64 volatile*>(fro() + frr * 16 + 8) = 1ULL << 63;
    }

    inline void command(uint32 val)
    {
        write<uint32>(REG_GCMD, val);
        while ((read<uint32>(REG_GSTS) & val) != val)
            relax();
    }

    inline void qi_submit(Dmar_qi const& q)
    {
        invq[invq_idx] = q;
        invq_idx = (invq_idx + 1) % cnt;
        write<uint64>(REG_IQT, invq_idx << 4);
    };

    inline void qi_wait()
    {
        for (uint64 v = read<uint64>(REG_IQT); v != read<uint64>(REG_IQH); relax())
            ;
    }

    inline void flush_ctx()
    {
        if (qi()) {
            qi_submit(Dmar_qi_ctx());
            qi_submit(Dmar_qi_tlb());
            qi_wait();
        } else {
            write<uint64>(REG_CCMD, 1ULL << 63 | 1ULL << 61);
            while (read<uint64>(REG_CCMD) & (1ULL << 63))
                relax();
            write<uint64>(REG_IOTLB, 1ULL << 63 | 1ULL << 60);
            while (read<uint64>(REG_IOTLB) & (1ULL << 63))
                relax();
        }
    }

    void fault_handler();

    /// Configure the basic DMAR unit registers.
    void init();

public:
    Dmar(Paddr);

    static inline void* operator new(size_t) { return cache.alloc(); }

    /// Enable the DMAR unit, including re-initialization of registers (e.g. after suspend).
    static inline void enable()
    {
        for_each(Forward_list_range{list}, mem_fn_closure(&Dmar::init)());
        for_each(Forward_list_range{list}, mem_fn_closure(&Dmar::command)(gcmd));
    }

    /// Enable the DMAR unit with given feature flags from ACPI tables.
    static inline void enable(unsigned flags)
    {
        if (!(flags & 1))
            gcmd &= ~GCMD_IRE;

        for_each(Forward_list_range{list}, mem_fn_closure(&Dmar::command)(gcmd));
    }

    static inline void set_irt(uint16 i, unsigned rid, unsigned cpu, unsigned vec, unsigned trg)
    {
        assert(i < (1 << Dmar_irt::NUM_ENTRIES_ORDER));

        // This line triggers a clang-tidy false positive.
        //
        // https://github.com/llvm/llvm-project/issues/56253
        irt[i].set(1ULL << 18 | rid, static_cast<uint64>(cpu) << 40 | static_cast<uint64>(vec) << 16 |
                                         static_cast<uint64>(trg) << 4 | 1); // NOLINT
    }

    static inline void clear_irt(uint16 i)
    {
        assert(i < (1 << Dmar_irt::NUM_ENTRIES_ORDER));
        irt[i].set(0, 0);
    }

    /// Returns the IRT index to use for the given vector and CPU.
    static inline uint16 irt_index(uint16 cpu, uint8 vector)
    {
        static_assert(1 << Dmar_irt::NUM_ENTRIES_ORDER >= NUM_USER_VECTORS * NUM_CPU,
                      "IRT too small for number of CPUs");
        static_assert(Dmar_irt::NUM_ENTRIES_ORDER <= 16, "Index type to small for IRT size");

        assert(cpu < NUM_CPU and vector < NUM_USER_VECTORS);

        return static_cast<uint16>(vector * NUM_CPU + cpu);
    }

    static bool ire() { return gcmd & GCMD_IRE; }

    static bool qie() { return gcmd & GCMD_QIE; }

    static void flush_all_contexts()
    {
        for_each(Forward_list_range{list}, mem_fn_closure(&Dmar::flush_ctx)());
    }

    void assign(unsigned long, Pd*);

    REGPARM(1)
    static void vector(unsigned) asm("msi_vector");
};
