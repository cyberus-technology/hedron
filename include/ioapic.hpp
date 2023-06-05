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
#include "hip.hpp"
#include "nodestruct.hpp"
#include "optional.hpp"
#include "static_vector.hpp"

// A single IOAPIC
//
// All public functions of this class are safe to be called without synchronization, unless stated otherwise
// in its documentation.
class Ioapic
{
private:
    uint32 const paddr;
    mword const reg_base;
    unsigned const gsi_base;
    unsigned const id;
    uint16 rid;

    // An array of all IOAPICs in the system indexed by their ID.
    static No_destruct<Optional<Ioapic>> ioapics_by_id[NUM_IOAPIC];

    enum
    {
        IOAPIC_IDX = 0x0,
        IOAPIC_WND = 0x10,
        IOAPIC_PAR = 0x20,
        IOAPIC_EOI = 0x40,

        // The maximum number of redirection table entries allowed by the
        // specification.
        IOAPIC_MAX_IRT = 0xf0,
    };

    enum Register
    {
        IOAPIC_ID = 0x0,
        IOAPIC_VER = 0x1,
        IOAPIC_ARB = 0x2,
        IOAPIC_BCFG = 0x3,
        IOAPIC_IRT = 0x10,
    };

    enum Id_reg
    {
        ID_SHIFT = 24,
    };

    enum Irt_entry : uint64
    {
        // Constants for remappable (IOMMU) IRT entries.
        IRT_REMAPPABLE_HANDLE_15_SHIFT = 11,
        IRT_REMAPPABLE_HANDLE_0_14_SHIFT = 49,
        IRT_FORMAT_REMAPPABLE = 1UL << 48,

        // Constants for legacy (non-IOMMU) IRT entries.
        IRT_DESTINATION_SHIFT = 56,

        IRT_MASKED = 1UL << 16,
        IRT_TRIGGER_MODE_LEVEL = 1UL << 15,
        IRT_POLARITY_ACTIVE_LOW = 1UL << 13,
    };

    inline void index(Register reg) { *reinterpret_cast<uint8 volatile*>(reg_base + IOAPIC_IDX) = reg; }

    inline uint32 read(Register reg)
    {
        index(reg);
        return *reinterpret_cast<uint32 volatile*>(reg_base + IOAPIC_WND);
    }

    inline void write(Register reg, uint32 val)
    {
        index(reg);
        *reinterpret_cast<uint32 volatile*>(reg_base + IOAPIC_WND) = val;
    }

    inline uint32 read_id_reg() { return read(IOAPIC_ID); }
    inline uint32 read_version_reg() { return read(IOAPIC_VER); }
    inline uint32 get_paddr() { return paddr; }

    inline unsigned get_gsi() const { return gsi_base; }

    inline unsigned version() { return read(IOAPIC_VER) & 0xff; }

    // Return the highest entry index (pin number).
    //
    // The returned value is one less then the count of pins.
    inline unsigned irt_max() { return read(IOAPIC_VER) >> 16 & 0xff; }

public:
    enum
    {
        ID_MASK = 0xf,
    };

    Ioapic(Paddr paddr_, unsigned id_, unsigned gsi_base_);
    Ioapic(Ioapic const&) = default;
    Ioapic(Ioapic&&) = default;

    static Optional<Ioapic>& by_id(uint8 ioapic_id)
    {
        assert(ioapic_id < NUM_IOAPIC);
        return *ioapics_by_id[ioapic_id];
    }

    static inline void add_to_hip(Hip_ioapic*& entry)
    {
        for (auto& opt_ioapic : ioapics_by_id) {
            if (not opt_ioapic->has_value()) {
                continue;
            }

            Ioapic& ioapic{opt_ioapic->value()};

            entry->id = ioapic.read_id_reg();
            entry->version = ioapic.read_version_reg();
            entry->gsi_base = ioapic.get_gsi();
            entry->base = ioapic.get_paddr();
            entry++;
        }
    }

    uint16 get_rid() const { return rid; }
};
