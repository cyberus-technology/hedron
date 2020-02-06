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

#include "qpd.hpp"

class Sys_call : public Sys_regs
{
    public:
        enum
        {
            DISABLE_BLOCKING    = 1ul << 0,
            DISABLE_DONATION    = 1ul << 1,
            DISABLE_REPLYCAP    = 1ul << 2
        };

        inline unsigned long pt() const { return ARG_1 >> 8; }
};

class Sys_create_pd : public Sys_regs
{
    public:
        inline unsigned long sel() const { return ARG_1 >> 8; }

        inline unsigned long pd() const { return ARG_2; }

        inline Crd crd() const { return Crd (ARG_3); }
};

class Sys_create_ec : public Sys_regs
{
    public:
        inline unsigned long sel() const { return ARG_1 >> 8; }

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
        inline unsigned long sel() const { return ARG_1 >> 8; }

        inline unsigned long pd() const { return ARG_2; }

        inline unsigned long ec() const { return ARG_3; }

        inline Qpd qpd() const { return Qpd (ARG_4); }
};

class Sys_create_pt : public Sys_regs
{
    public:
        inline unsigned long sel() const { return ARG_1 >> 8; }

        inline unsigned long pd() const { return ARG_2; }

        inline unsigned long ec() const { return ARG_3; }

        inline Mtd mtd() const { return Mtd (ARG_4); }

        inline mword eip() const { return ARG_5; }
};

class Sys_create_sm : public Sys_regs
{
    public:
        inline unsigned long sel() const { return ARG_1 >> 8; }

        inline unsigned long pd() const { return ARG_2; }

        inline mword cnt() const { return ARG_3; }

        inline unsigned long sm() const { return ARG_4; }
};

class Sys_revoke : public Sys_regs
{
    public:
        inline Crd crd() const { return Crd (ARG_2); }

        inline bool self() const { return flags() & 0x1; }

        inline bool remote() const { return flags() & 0x2; }

        inline mword pd() const { return ARG_3; }

        inline mword sm() const { return ARG_1 >> 8; }

        inline void rem(Pd * p) { ARG_3 = reinterpret_cast<mword>(p); }
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
        inline Crd & crd() { return reinterpret_cast<Crd &>(ARG_2); }
};

class Sys_pd_ctrl_map_access_page : public Sys_regs
{
    public:
        inline Crd & crd() { return reinterpret_cast<Crd &>(ARG_2); }
};

class Sys_pd_ctrl_delegate : public Sys_regs
{
    public:
        inline mword src_pd() const { return ARG_1 >> 8; }

        inline mword dst_pd() const { return ARG_2; }

        inline Xfer xfer() const { return Xfer {Crd {ARG_3}, ARG_4}; }

        inline void set_xfer(Xfer const &xfer)
        {
            ARG_3 = xfer.crd().value();
            ARG_4 = xfer.metadata();
        }

        inline Crd dst_crd() const { return Crd {ARG_5}; }
};

class Sys_pd_ctrl_msr_access : public Sys_regs
{
    public:
        inline uint32 msr_index() const { return static_cast<uint32>(ARG_1 >> 8); }
        inline uint64 msr_value() const { return ARG_2; }
        inline bool   is_write()  const { return flags() & 4; }

        inline void set_msr_value(uint64 v) { ARG_2 = v; }
};

class Sys_reply : public Sys_regs
{
    public:
        inline unsigned long sm() const { return ARG_1 >> 8; }
};

class Sys_ec_ctrl : public Sys_regs
{
    public:
        inline unsigned long ec() const { return ARG_1 >> 8; }

        inline unsigned long cnt() const { return ARG_2; }

        inline unsigned op() const { return flags() & 0x3; }
};

class Sys_sc_ctrl : public Sys_regs
{
    public:
        inline unsigned long sc() const { return ARG_1 >> 8; }

        inline void set_time (uint64 val)
        {
            ARG_2 = static_cast<mword>(val >> 32);
            ARG_3 = static_cast<mword>(val);
        }
};

class Sys_pt_ctrl : public Sys_regs
{
    public:
        inline unsigned long pt() const { return ARG_1 >> 8; }

        inline mword id() const { return ARG_2; }
};

class Sys_sm_ctrl : public Sys_regs
{
    public:
        inline unsigned long sm() const { return ARG_1 >> 8; }

        inline unsigned op() const { return flags() & 0x1; }

        inline unsigned zc() const { return flags() & 0x2; }

        inline uint64 time() const { return static_cast<uint64>(ARG_2) << 32 | ARG_3; }
};

class Sys_assign_pci : public Sys_regs
{
    public:
        inline unsigned long pd() const { return ARG_1 >> 8; }

        inline mword dev() const { return ARG_2; }

        inline mword hnt() const { return ARG_3; }
};

class Sys_assign_gsi : public Sys_regs
{
    public:
        inline unsigned long sm() const { return ARG_1 >> 8; }

        inline mword dev() const { return ARG_2; }

        inline unsigned cpu() const { return static_cast<unsigned>(ARG_3); }

        inline mword si() const { return ARG_4; }

        inline void set_msi (uint64 val)
        {
            ARG_2 = static_cast<mword>(val >> 32);
            ARG_3 = static_cast<mword>(val);
        }
};
