/*
 * Advanced Configuration and Power Interface (ACPI)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
 *
 * This file is part of the Hedron microhypervisor.
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

#include "acpi.hpp"
#include "acpi_dmar.hpp"
#include "acpi_facs.hpp"
#include "acpi_fadt.hpp"
#include "acpi_hpet.hpp"
#include "acpi_madt.hpp"
#include "acpi_mcfg.hpp"
#include "acpi_rsdp.hpp"
#include "acpi_rsdt.hpp"
#include "gsi.hpp"
#include "hpt.hpp"
#include "io.hpp"
#include "pic.hpp"
#include "stdio.hpp"
#include "x86.hpp"

Paddr Acpi::dmar, Acpi::facs, Acpi::fadt, Acpi::hpet, Acpi::madt, Acpi::mcfg, Acpi::rsdt, Acpi::xsdt;
Acpi_gas Acpi::pm1a_sts, Acpi::pm1b_sts, Acpi::pm1a_ena, Acpi::pm1b_ena, Acpi::pm1a_cnt, Acpi::pm1b_cnt,
    Acpi::pm2_cnt, Acpi::pm_tmr, Acpi::reset_reg;
Acpi_gas Acpi::gpe0_sts, Acpi::gpe1_sts, Acpi::gpe0_ena, Acpi::gpe1_ena;
uint32 Acpi::feature;
uint8 Acpi::reset_val;
unsigned Acpi::irq, Acpi::gsi;

void Acpi::delay(unsigned ms)
{
    unsigned cnt = timer_frequency * ms / 1000;
    unsigned val = read(PM_TMR);

    while ((read(PM_TMR) - val) % (1UL << 24) < cnt)
        pause();
}

void Acpi::reset() { write(RESET, reset_val); }

Acpi_table_facs Acpi::get_facs() { return *static_cast<Acpi_table_facs*>(Hpt::remap(facs)); }

void Acpi::set_facs(Acpi_table_facs const& saved_facs)
{
    *static_cast<Acpi_table_facs*>(Hpt::remap(facs)) = saved_facs;
}

Paddr Acpi::get_waking_vector()
{
    // It's unfortunate that we have hardcode which page table we are using for
    // remap. See #119.
    Acpi_table_facs* const facsp = static_cast<Acpi_table_facs*>(Hpt::remap(facs, false));

    return facsp->firmware_waking_vector;
}

void Acpi::set_waking_vector(Paddr vector, Wake_mode mode)
{
    Acpi_table_facs* const facsp = static_cast<Acpi_table_facs*>(Hpt::remap(facs));

    // We don't implement protected or long mode wake up, because firmware
    // doesn't correctly implement these.
    switch (mode) {
    case Wake_mode::REAL_MODE:
        // We only have this much address space in Real Mode.
        assert(vector < (1U << 20));

        facsp->firmware_waking_vector = static_cast<uint32>(vector);
        facsp->x_firmware_waking_vector = 0;
        break;
    }
}

bool Acpi::valid_sleep_type(uint8 slp_typa, uint8 slp_typb)
{
    return 0 == (((slp_typa | slp_typb) << PM1_CNT_SLP_TYP_SHIFT) & ~PM1_CNT_SLP_TYP);
}

void Acpi::enter_sleep_state(uint8 slp_typa, uint8 slp_typb)
{
    unsigned pm1_cnt_common = (Acpi::read(PM1_CNT) & ~PM1_CNT_SLP_TYP) | PM1_CNT_SLP_EN;

    // Clear WAK_STS. This is a write-one-to-clear register.
    Acpi::write(PM1_STS, PM1_STS_WAKE);

    // The PM1_CNT register is special compared to other split registers,
    // because different values have to be written in each part.
    Acpi::hw_write(&pm1a_cnt, pm1_cnt_common | (slp_typa << PM1_CNT_SLP_TYP_SHIFT));
    Acpi::hw_write(&pm1b_cnt, pm1_cnt_common | (slp_typb << PM1_CNT_SLP_TYP_SHIFT));

    // For S2 and S3, the wake status will never be set and CPU power will be
    // turned off. For S1, this bit will be set when it's time to wake up again.
    while (not(Acpi::read(PM1_STS) & PM1_STS_WAKE)) {
        pause();
    }
}

void Acpi::setup()
{
    if (!xsdt && !rsdt)
        Acpi_rsdp::parse();

    if (xsdt)
        static_cast<Acpi_table_rsdt*>(Hpt::remap(xsdt))->parse(xsdt, sizeof(uint64));
    else if (rsdt)
        static_cast<Acpi_table_rsdt*>(Hpt::remap(rsdt))->parse(rsdt, sizeof(uint32));

    if (fadt) {
        Acpi_table_fadt::init(static_cast<Acpi_table_fadt*>(Hpt::remap(fadt)));
    }
    if (hpet)
        static_cast<Acpi_table_hpet*>(Hpt::remap(hpet))->parse();
    if (madt)
        static_cast<Acpi_table_madt*>(Hpt::remap(madt))->parse();
    if (mcfg)
        static_cast<Acpi_table_mcfg*>(Hpt::remap(mcfg))->parse();
    if (dmar)
        static_cast<Acpi_table_dmar*>(Hpt::remap(dmar))->parse();

    if (facs) {
        Acpi_table_facs* const facsp = static_cast<Acpi_table_facs*>(Hpt::remap(facs));

        trace(TRACE_ACPI, "%.4s:%#010lx VER:%2d FLAGS:%#x HW:%#010x LEN:%5u",
              reinterpret_cast<char const*>(&facsp->signature), facs, facsp->version, facsp->flags,
              facsp->hardware_signature, facsp->length);
    }

    if (!Acpi_table_madt::sci_overridden) {
        Acpi_intr sci_override;
        sci_override.bus = 0;
        sci_override.irq = static_cast<uint8>(irq);
        sci_override.gsi = irq;
        sci_override.flags.pol = Acpi_inti::POL_CONFORMING;
        sci_override.flags.trg = Acpi_inti::TRG_CONFORMING;
        Acpi_table_madt::parse_intr(&sci_override);
    }

    Gsi::set(gsi = Gsi::irq_to_gsi(irq));

    Acpi::init();

    trace(TRACE_ACPI, "ACPI: GSI:%#x TMR:%lu", gsi, tmr_msb() + 1);
}

void Acpi::init()
{
    if (fadt) {
        Acpi_table_fadt::init(static_cast<Acpi_table_fadt*>(Hpt::remap(fadt)));
    }

    if (Acpi_table_madt::pic_present) {
        Pic::init();
    }

    write(PM1_ENA, 0);

    clear(GPE0_ENA, 0);
    clear(GPE1_ENA, 0);
}

unsigned Acpi::read(Register reg)
{
    switch (reg) {
    case PM1_STS:
        return hw_read(&pm1a_sts) | hw_read(&pm1b_sts);
    case PM1_ENA:
        return hw_read(&pm1a_ena) | hw_read(&pm1b_ena);
    case PM1_CNT:
        return hw_read(&pm1a_cnt) | hw_read(&pm1b_cnt);
    case PM2_CNT:
        return hw_read(&pm2_cnt);
    case PM_TMR:
        return hw_read(&pm_tmr);
    case RESET:
        break;
    default:
        Console::panic("Unimplemented register Acpi::read");
        break;
    }

    return 0;
}

void Acpi::clear(Register reg, unsigned val)
{
    switch (reg) {
    case GPE0_ENA:
        hw_write(&gpe0_ena, val, true);
        break;
    case GPE1_ENA:
        hw_write(&gpe1_ena, val, true);
        break;
    default:
        Console::panic("Unimplemented register Acpi::clear");
        break;
    }
}

void Acpi::write(Register reg, unsigned val)
{
    // XXX: Spec requires that certain bits be preserved.

    switch (reg) {
    case PM1_STS:
        hw_write(&pm1a_sts, val);
        hw_write(&pm1b_sts, val);
        break;
    case PM1_ENA:
        hw_write(&pm1a_ena, val);
        hw_write(&pm1b_ena, val);
        break;
    case PM1_CNT:
        hw_write(&pm1a_cnt, val);
        hw_write(&pm1b_cnt, val);
        break;
    case PM2_CNT:
        hw_write(&pm2_cnt, val);
        break;
    case PM_TMR: // read-only
        break;
    case RESET:
        hw_write(&reset_reg, val);
        break;
    default:
        Console::panic("Unimplemented register Acpi::write");
        break;
    }
}

unsigned Acpi::hw_read(Acpi_gas* gas)
{
    if (!gas->bits) // Register not implemented
        return 0;

    if (gas->asid == Acpi_gas::IO) {
        switch (gas->bits) {
        case 8:
            return Io::in<uint8>(static_cast<unsigned>(gas->addr));
        case 16:
            return Io::in<uint16>(static_cast<unsigned>(gas->addr));
        case 32:
            return Io::in<uint32>(static_cast<unsigned>(gas->addr));
        }
    }

    Console::panic("Unimplemented ASID %d bits=%d", gas->asid, gas->bits);
}

void Acpi::hw_write(Acpi_gas* gas, unsigned val, bool prm)
{
    if (!gas->bits) // Register not implemented
        return;

    if (gas->asid == Acpi_gas::IO) {
        switch (gas->bits) {
        case 8:
            Io::out(static_cast<unsigned>(gas->addr), static_cast<uint8>(val));
            return;
        case 16:
            Io::out(static_cast<unsigned>(gas->addr), static_cast<uint16>(val));
            return;
        case 32:
            Io::out(static_cast<unsigned>(gas->addr), static_cast<uint32>(val));
            return;
        case 64:
        case 128:
            if (!prm)
                break;

            for (unsigned i = 0; i < gas->bits / 32; i++)
                Io::out(static_cast<unsigned>(gas->addr) + i * 4, static_cast<uint32>(val));
            return;
        }
    }

    Console::panic("Unimplemented ASID %d bits=%d prm=%u", gas->asid, gas->bits, prm);
}
