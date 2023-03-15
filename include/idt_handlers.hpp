/*
 * IDT Handler Modes
 *
 * Copyright (C) 2023 Julian Stecklina, Cyberus Technology GmbH.
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

// These definitions are used in entry.S to define the handlers[] array.

#define IDT_MODE_MASK 0x3

// An IDT entry that userspace cannot invoke directly.
#define IDT_MODE_DPL0 0

// An IDT entry that is available to userspace via `int`, `int3`, or `into` instructions.
#define IDT_MODE_DPL3 1

// Same as IDT_MODE_DPL0, but the handler will execute on an alternate stack.
#define IDT_MODE_DPL0_ALTSTACK 2
