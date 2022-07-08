/*
 * Standard I/O
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

#include "console.hpp"
#include "string.hpp"

// Returns the current CPU ID or a placeholder value if the CPU ID is not yet known.
//
// This function is only intended to be called from the trace macro below.
int trace_id();

#define trace(T, format, ...)                                                                                \
    do {                                                                                                     \
        if (EXPECT_FALSE((trace_mask & (T)) == (T))) {                                                       \
            Console::print("[%3d][%s:%d] " format, trace_id(), FILENAME, __LINE__, ##__VA_ARGS__);           \
        }                                                                                                    \
    } while (0)

/*
 * Definition of trace events
 */
enum
{
    TRACE_CPU = 1UL << 0,
    TRACE_IOMMU = 1UL << 1,
    TRACE_APIC = 1UL << 2,
    TRACE_VMX = 1UL << 4,
    TRACE_SVM = 1UL << 5,
    TRACE_ACPI = 1UL << 8,
    TRACE_MEMORY = 1UL << 13,
    TRACE_PCI = 1UL << 14,
    TRACE_SCHEDULE = 1UL << 16,
    TRACE_DEL = 1UL << 18,
    TRACE_REV = 1UL << 19,
    TRACE_RCU = 1UL << 20,
    TRACE_SYSCALL = 1UL << 30,
    TRACE_ERROR = 1UL << 31,
};

/*
 * Enabled trace events
 */
unsigned const trace_mask = TRACE_CPU | TRACE_IOMMU |
#ifdef DEBUG
                            //                            TRACE_APIC      |
                            TRACE_VMX | TRACE_SVM |
//                            TRACE_ACPI      |
//                            TRACE_MEMORY    |
//                            TRACE_PCI       |
//                            TRACE_SCHEDULE  |
//                            TRACE_DEL       |
//                            TRACE_REV       |
//                            TRACE_RCU       |
//                            TRACE_SYSCALL   |
#endif
                            TRACE_ERROR | 0;
