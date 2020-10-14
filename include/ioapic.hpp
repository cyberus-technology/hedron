/*
 * I/O Advanced Programmable Interrupt Controller (IOAPIC)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
 *
 * Copyright (C) 2017-2018 Markus Partheymüller, Cyberus Technology GmbH.
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

#include "hip.hpp"
#include "list.hpp"
#include "lock_guard.hpp"
#include "static_vector.hpp"
#include "algorithm.hpp"

class Ioapic : public Forward_list<Ioapic>
{
    private:
        uint32   const      paddr;
        mword    const      reg_base;
        unsigned const      gsi_base;
        unsigned const      id;
        uint16              rid;
        Spinlock            lock;

        static Ioapic *     list;

        enum
        {
            IOAPIC_IDX  = 0x0,
            IOAPIC_WND  = 0x10,
            IOAPIC_PAR  = 0x20,
            IOAPIC_EOI  = 0x40,

            // The maximum number of redirection table entries allowed by the
            // specification.
            IOAPIC_MAX_IRT = 0xf0,
        };

        enum Register
        {
            IOAPIC_ID   = 0x0,
            IOAPIC_VER  = 0x1,
            IOAPIC_ARB  = 0x2,
            IOAPIC_BCFG = 0x3,
            IOAPIC_IRT  = 0x10,
        };

        // Stores redirection entries across suspend/resume.
        Static_vector<uint64, IOAPIC_MAX_IRT> suspend_redir_table;

        inline void index (Register reg)
        {
            *reinterpret_cast<uint8 volatile *>(reg_base + IOAPIC_IDX) = reg;
        }

        inline uint32 read (Register reg)
        {
            Lock_guard <Spinlock> guard (lock);
            index (reg);
            return *reinterpret_cast<uint32 volatile *>(reg_base + IOAPIC_WND);
        }

        inline void write (Register reg, uint32 val)
        {
            Lock_guard <Spinlock> guard (lock);
            index (reg);
            *reinterpret_cast<uint32 volatile *>(reg_base + IOAPIC_WND) = val;
        }

    public:
        Ioapic (Paddr, unsigned, unsigned);

        static void *operator new (size_t);

        static inline bool claim_dev (unsigned r, unsigned i)
        {
            auto range = Forward_list_range (list);
            auto it = find_if (range,
                               [i] (auto &ioapic) { return ioapic.rid == 0 and ioapic.id == i; });

            if (it != range.end()) {
                it->rid = static_cast<uint16> (r);
                return true;
            } else {
                return false;
            }
        }

        static inline void add_to_hip (Hip_ioapic *&entry)
        {
            for (auto &ioapic : Forward_list_range (list)) {
                entry->id = ioapic.read_id_reg();
                entry->version = ioapic.read_version_reg();
                entry->gsi_base = ioapic.get_gsi();
                entry->base = ioapic.get_paddr();
                entry++;
            }
        }

        inline uint32 read_id_reg()      { return read (IOAPIC_ID); }
        inline uint32 read_version_reg() { return read (IOAPIC_VER); }
        inline uint32 get_paddr()        { return paddr; }

        inline uint16 get_rid() const { return rid; }

        inline unsigned get_gsi() const { return gsi_base; }

        inline unsigned version() { return read (IOAPIC_VER) & 0xff; }

        inline unsigned prq() { return read (IOAPIC_VER) >> 15 & 0x1; }

        inline unsigned irt_max() { return read (IOAPIC_VER) >> 16 & 0xff; }

        inline void set_irt (unsigned gsi, unsigned val)
        {
            unsigned pin = gsi - gsi_base;
            write (Register (IOAPIC_IRT + 2 * pin), val);
        }

        inline void set_cpu (unsigned gsi, unsigned cpu, bool ire)
        {
            unsigned pin = gsi - gsi_base;
            write (Register (IOAPIC_IRT + 2 * pin + 1), ire ? (gsi << 17 | 1ul << 16) : (cpu << 24));
        }

        void set_irt_entry (size_t entry, uint64 val);
        uint64 get_irt_entry (size_t entry);

        // Prepare the IOAPIC for system suspend by saving its state to memory.
        void save();

        // Restore the state of the IOAPIC from memory after a system resume.
        void restore();

        // Call save on all IOAPICs.
        static void save_all();

        // Call restore on all IOAPICs.
        static void restore_all();
};
