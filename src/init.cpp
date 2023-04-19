/*
 * Initialization Code
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
 *
 * This file is part of the Hedron hypervisor.
 *
 * Copyright (C) 2017-2018 Markus Partheymüller, Cyberus Technology GmbH.
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
#include "acpi_rsdp.hpp"
#include "cmdline.hpp"
#include "compiler.hpp"
#include "console_serial.hpp"
#include "console_vga.hpp"
#include "hip.hpp"
#include "hpt.hpp"
#include "idt.hpp"
#include "lapic.hpp"
#include "multiboot.hpp"
#include "multiboot2.hpp"
#include "suspend.hpp"
#include "tss.hpp"

static char const* get_boot_type(mword magic)
{
    switch (magic) {
    case Multiboot::MAGIC:
        return "MB1";
    case Multiboot2::MAGIC:
        return "MB2";
    default:
        return "UNKNOWN";
    }
}

extern "C" void init(mword magic, mword mbi)
{
    // When we execute code in this function, CPU-local memory is not yet setup. Access to it might still work
    // and return bogus values, if we don't actively prevent it.
    Cpulocal::prevent_accidental_access();

    // Setup 0-page and 1-page
    memset(PAGE_0, 0, PAGE_SIZE);
    memset(PAGE_1, ~0u, PAGE_SIZE);

    for (void (**func)() = &CTORS_G; func != &CTORS_E; (*func++)())
        ;

    if (magic == Multiboot::MAGIC) {
        Multiboot* mbi_ = static_cast<Multiboot*>(Hpt::remap(mbi));
        if (mbi_->flags & Multiboot::CMDLINE)
            Cmdline::init(static_cast<char const*>(Hpt::remap(mbi_->cmdline)));
    }

    if (magic == Multiboot2::MAGIC) {
        Multiboot2::Header const* mbi_ = static_cast<Multiboot2::Header const*>(Hpt::remap(mbi));
        mbi_->for_each_tag([&](Multiboot2::Tag const* tag) {
            if (tag->type == Multiboot2::TAG_CMDLINE)
                Cmdline::init(tag->cmdline());
        });
    }

    // These constructors need access to the configuration from the command line.
    static Console_serial con_serial;
    static Console_vga con_vga;

    // Now we're ready to talk to the world
    Console::print("\fHedron Hypervisor (Cyberus-%07lx "
#ifdef NDEBUG
                   "RELEASE"
#else
                   "DEBUG"
#endif
                   " " ARCH "): " COMPILER_STRING " [%s] \n",
                   reinterpret_cast<mword>(&GIT_VER), get_boot_type(magic));

    if (magic == Multiboot2::MAGIC) {
        Multiboot2::Header const* mbi_ = static_cast<Multiboot2::Header const*>(Hpt::remap(mbi));
        mbi_->for_each_tag([&](Multiboot2::Tag const* tag) {
            if (tag->type == Multiboot2::TAG_ACPI_2) {
                Acpi_rsdp::parse(tag->rsdp());
            }
        });
    }

    Gdt::build();
    Idt::build();
    Acpi::setup();
    Tss::setup();
    Lapic::setup();
    Hip::build(magic, mbi);
}
