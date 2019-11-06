/*
 * Initialization Code
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
 *
 * This file is part of the NOVA microhypervisor.
 *
 * Copyright (C) 2017-2018 Markus Partheymüller, Cyberus Technology GmbH.
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

#include "acpi.hpp"
#include "cmdline.hpp"
#include "compiler.hpp"
#include "console_vga.hpp"
#include "gsi.hpp"
#include "hip.hpp"
#include "hpt.hpp"
#include "idt.hpp"
#include "lapic.hpp"
#include "keyb.hpp"
#include "multiboot.hpp"
#include "tss.hpp"

extern "C" INIT REGPARM (1)
void init (mword mbi)
{
    // Setup 0-page and 1-page
    memset (PAGE_0,  0,  PAGE_SIZE);
    memset (PAGE_1, ~0u, PAGE_SIZE);

    for (void (**func)() = &CTORS_G; func != &CTORS_E; (*func++)()) ;

    Multiboot *mbi_ = static_cast<Multiboot *>(Hpt::remap (mbi));
    if (mbi_->flags & Multiboot::CMDLINE)
        Cmdline::init (mbi_->cmdline);

    for (void (**func)() = &CTORS_C; func != &CTORS_G; (*func++)()) ;

    // Now we're ready to talk to the world
    Console::print ("\fNOVA Microhypervisor (Cyberus-%07lx "
#ifdef NDEBUG
                     "RELEASE"
#else
                     "DEBUG"
#endif
                     " " ARCH "): " COMPILER_STRING "\n", reinterpret_cast<mword>(&GIT_VER));

    Idt::build();
    Gsi::setup();
    Acpi::setup();
    Tss::setup();
    Lapic::setup();
    Hip::build (mbi);

    Console_vga::con.setup();

    Keyb::init();
}
