/*
 * Virtual Machine Extensions (VMX)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
 *
 * Copyright (C) 2019 Julian Stecklina, Cyberus Technology GmbH.
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

#include "types.hpp"

union vmx_basic {
    uint64 val;
    struct {
        uint32 revision;
        uint32 size : 13, : 3, width : 1, dual : 1, type : 4, insouts : 1, ctrl : 1;
    };
};

union vmx_ept_vpid {
    uint64 val;
    struct {
        uint32 : 16, super : 2, : 2, invept : 1, : 11;
        uint32 invvpid : 1;
    };
};

union vmx_ctrl_pin {
    uint64 val;
    struct {
        uint32 set, clr;
    };
};

union vmx_ctrl_cpu {
    uint64 val;
    struct {
        uint32 set, clr;
    };
};

union vmx_ctrl_exi {
    uint64 val;
    struct {
        uint32 set, clr;
    };
};

union vmx_ctrl_ent {
    uint64 val;
    struct {
        uint32 set, clr;
    };
};
