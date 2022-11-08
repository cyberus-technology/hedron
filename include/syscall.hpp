/*
 * System-Call Interface
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
 * Copyright (C) 2014-2015 Alexander Boettcher, Genode Labs GmbH.
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

#include "arch.hpp"
#include "crd.hpp"
#include "mtd.hpp"
#include "qpd.hpp"
#include "regs.hpp"
#include "types.hpp"

class Sys_call : public Sys_regs
{
public:
    enum
    {
        DISABLE_BLOCKING = 1ul << 0,
        DISABLE_DONATION = 1ul << 1,
        DISABLE_REPLYCAP = 1ul << 2
    };

    inline unsigned long pt() const { return ARG_1 >> ARG1_SEL_SHIFT; }
};

class Sys_create_pd : public Sys_regs
{
public:
    inline unsigned long sel() const { return ARG_1 >> ARG1_SEL_SHIFT; }

    inline unsigned long pd() const { return ARG_2; }

    inline Crd crd() const { return Crd(ARG_3); }

    inline bool is_passthrough() const { return flags() & 0x1; }
};

class Sys_create_ec : public Sys_regs
{
public:
    inline unsigned long sel() const { return ARG_1 >> ARG1_SEL_SHIFT; }

    inline unsigned long pd() const { return ARG_2; }

    inline unsigned cpu() const { return ARG_3 & 0xfff; }

    inline bool use_apic_access_page() const { return flags() & 0x4; }

    inline bool is_vcpu() const { return flags() & 0x2; }

    inline bool map_user_page_in_owner() const { return flags() & 0x8; }

    inline mword user_page() const { return ARG_3 & ~0xfff; }

    inline mword esp() const { return ARG_4; }

    inline unsigned evt() const { return static_cast<unsigned>(ARG_5); }
};

class Sys_create_sc : public Sys_regs
{
public:
    inline unsigned long sel() const { return ARG_1 >> ARG1_SEL_SHIFT; }

    inline unsigned long pd() const { return ARG_2; }

    inline unsigned long ec() const { return ARG_3; }

    inline Qpd qpd() const { return Qpd(ARG_4); }
};

class Sys_create_pt : public Sys_regs
{
public:
    inline unsigned long sel() const { return ARG_1 >> ARG1_SEL_SHIFT; }

    inline unsigned long pd() const { return ARG_2; }

    inline unsigned long ec() const { return ARG_3; }

    inline Mtd mtd() const { return Mtd(ARG_4); }

    inline mword eip() const { return ARG_5; }
};

class Sys_create_sm : public Sys_regs
{
public:
    inline unsigned long sel() const { return ARG_1 >> ARG1_SEL_SHIFT; }

    inline unsigned long pd() const { return ARG_2; }

    inline mword cnt() const { return ARG_3; }
};

class Sys_create_kp : public Sys_regs
{
public:
    inline unsigned long sel() const { return ARG_1 >> ARG1_SEL_SHIFT; }

    inline unsigned long pd() const { return ARG_2; }
};

class Sys_revoke : public Sys_regs
{
public:
    inline Crd crd() const { return Crd(ARG_2); }

    inline bool self() const { return flags() & 0x1; }

    inline bool remote() const { return flags() & 0x2; }

    inline mword pd() const { return ARG_3; }
};

class Sys_pd_ctrl : public Sys_regs
{
public:
    enum ctrl_op
    {
        LOOKUP,
        MAP_ACCESS_PAGE,
        DELEGATE,
        MSR_ACCESS,
    };

    ctrl_op op() const { return static_cast<ctrl_op>(flags() & 0x3); }
};

class Sys_pd_ctrl_lookup : public Sys_regs
{
public:
    inline Crd& crd() { return reinterpret_cast<Crd&>(ARG_2); }
};

class Sys_pd_ctrl_map_access_page : public Sys_regs
{
public:
    inline Crd& crd() { return reinterpret_cast<Crd&>(ARG_2); }
};

class Sys_pd_ctrl_delegate : public Sys_regs
{
public:
    inline mword src_pd() const { return ARG_1 >> ARG1_SEL_SHIFT; }

    inline mword dst_pd() const { return ARG_2; }

    inline Xfer xfer() const { return Xfer{Crd{ARG_3}, ARG_4}; }

    inline void set_xfer(Xfer const& xfer)
    {
        ARG_3 = xfer.crd().value();
        ARG_4 = xfer.metadata();
    }

    inline Crd dst_crd() const { return Crd{ARG_5}; }
};

class Sys_pd_ctrl_msr_access : public Sys_regs
{
public:
    inline uint32 msr_index() const { return static_cast<uint32>(ARG_1 >> ARG1_SEL_SHIFT); }
    inline uint64 msr_value() const { return ARG_2; }
    inline bool is_write() const { return flags() & 4; }

    inline void set_msr_value(uint64 v) { ARG_2 = v; }
};

class Sys_reply : public Sys_regs
{
public:
    inline unsigned long sm() const { return ARG_1 >> ARG1_SEL_SHIFT; }
};

class Sys_ec_ctrl : public Sys_regs
{
public:
    enum ctrl_op
    {
        RECALL,
    };

    inline unsigned long ec() const { return ARG_1 >> ARG1_SEL_SHIFT; }

    inline unsigned op() const { return flags() & 0x3; }
};

class Sys_sc_ctrl : public Sys_regs
{
public:
    inline unsigned long sc() const { return ARG_1 >> ARG1_SEL_SHIFT; }

    inline void set_time(uint64 val)
    {
        ARG_2 = static_cast<mword>(val >> 32);
        ARG_3 = static_cast<mword>(val);
    }
};

class Sys_pt_ctrl : public Sys_regs
{
public:
    inline unsigned long pt() const { return ARG_1 >> ARG1_SEL_SHIFT; }

    inline mword id() const { return ARG_2; }
};

class Sys_sm_ctrl : public Sys_regs
{
public:
    /// A "sm_ctrl" syscall is always one of these two sub operations.
    enum Sm_operation
    {
        Up = 0,
        Down = 1,
    };

    inline unsigned long sm() const { return ARG_1 >> ARG1_SEL_SHIFT; }

    inline unsigned op() const { return flags() & 0x1; }

    inline unsigned zc() const { return flags() & 0x2; }

    inline uint64 time() const { return static_cast<uint64>(ARG_2) << 32 | ARG_3; }
};

class Sys_kp_ctrl : public Sys_regs
{
public:
    enum ctrl_op
    {
        MAP,
        UNMAP,
    };

    inline mword kp() const { return ARG_1 >> ARG1_SEL_SHIFT; }

    inline ctrl_op op() const { return static_cast<ctrl_op>(flags() & 0x3); }
};

class Sys_kp_ctrl_map : public Sys_regs
{
public:
    inline mword kp() const { return ARG_1 >> ARG1_SEL_SHIFT; }

    inline mword dst_pd() const { return ARG_2; }

    inline mword dst_addr() const { return ARG_3; }
};

class Sys_kp_ctrl_unmap : public Sys_regs
{
public:
    inline mword kp() const { return ARG_1 >> ARG1_SEL_SHIFT; }
};

class Sys_assign_pci : public Sys_regs
{
public:
    inline unsigned long pd() const { return ARG_1 >> ARG1_SEL_SHIFT; }

    inline mword dev() const { return ARG_2; }

    inline mword hnt() const { return ARG_3; }
};

class Sys_machine_ctrl : public Sys_regs
{
public:
    enum ctrl_op
    {
        SUSPEND = 0,
        UPDATE_MICROCODE = 1,
    };

    inline ctrl_op op() const { return static_cast<ctrl_op>(flags() & 0x3); }
};

class Sys_machine_ctrl_suspend : public Sys_machine_ctrl
{
private:
    static constexpr size_t SLP_TYPA_SHIFT{ARG1_SEL_SHIFT};
    static constexpr size_t SLP_TYPB_SHIFT{SLP_TYPA_SHIFT + 8};

public:
    enum class mode : mword
    {
        REAL_MODE = 0,
    };

    inline uint8 slp_typa() const { return (ARG_1 >> SLP_TYPA_SHIFT) & 0xFF; }
    inline uint8 slp_typb() const { return (ARG_1 >> SLP_TYPB_SHIFT) & 0xFF; }

    inline void set_waking_vector(mword waking_vector, mode waking_mode)
    {
        ARG_2 = (static_cast<mword>(waking_mode) << 62) | waking_vector;
    }
};

class Sys_machine_ctrl_update_microcode : public Sys_machine_ctrl
{
public:
    inline unsigned size() const { return static_cast<unsigned>(ARG_1) >> ARG1_SEL_SHIFT; }
    inline mword update_address() const { return static_cast<mword>(ARG_2); }
};

class Sys_irq_ctrl : public Sys_regs
{
public:
    enum ctrl_op
    {
        CONFIGURE_VECTOR = 0,
        ASSIGN_IOAPIC_PIN = 1,
        MASK_IOAPIC_PIN = 2,
        ASSIGN_MSI = 3,
    };

    inline ctrl_op op() const { return static_cast<ctrl_op>(flags()); }
};

class Sys_irq_ctrl_configure_vector : public Sys_irq_ctrl
{
public:
    inline uint8 vector() const { return static_cast<uint8>(ARG_1 >> ARG1_SEL_SHIFT); }
    inline uint16 cpu() const { return static_cast<uint16>(ARG_1 >> (ARG1_SEL_SHIFT + 8)); }

    inline mword sm() const { return static_cast<mword>(ARG_2); }
    inline mword kp() const { return static_cast<mword>(ARG_3); }
    inline uint16 kp_bit() const { return static_cast<uint16>(ARG_4 & 0x7FFF); }
};

class Sys_irq_ctrl_assign_ioapic_pin : public Sys_irq_ctrl
{
public:
    inline bool level() const { return ARG_1 & (1UL << 36); }
    inline bool active_low() const { return ARG_1 & (1UL << 37); }

    inline uint8 vector() const { return static_cast<uint8>(ARG_1 >> ARG1_SEL_SHIFT); }
    inline uint16 cpu() const { return static_cast<uint16>(ARG_1 >> (ARG1_SEL_SHIFT + 8)); }

    inline uint8 ioapic_id() const { return static_cast<uint8>(ARG_2 & 0xF); }
    inline uint8 ioapic_pin() const { return static_cast<uint8>(ARG_2 >> 4); }
};

class Sys_irq_ctrl_mask_ioapic_pin : public Sys_irq_ctrl
{
public:
    inline bool mask() const { return ARG_1 & (1UL << ARG1_SEL_SHIFT); }

    inline uint8 ioapic_id() const { return static_cast<uint8>(ARG_2 & 0xF); }
    inline uint8 ioapic_pin() const { return static_cast<uint8>(ARG_2 >> 4); }
};

class Sys_irq_ctrl_assign_msi : public Sys_irq_ctrl
{
public:
    inline uint8 vector() const { return static_cast<uint8>(ARG_1 >> ARG1_SEL_SHIFT); }
    inline uint16 cpu() const { return static_cast<uint16>(ARG_1 >> (ARG1_SEL_SHIFT + 8)); }

    inline mword dev() const { return ARG_2 & ~0xfff; }

    inline void set_msi(uint32 msi_addr, uint32 msi_data)
    {
        ARG_2 = msi_addr;
        ARG_3 = msi_data;
    }
};
