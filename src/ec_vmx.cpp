/*
 * Execution Context
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

#include "dmar.hpp"
#include "ec.hpp"
#include "lapic.hpp"
#include "vcpu.hpp"
#include "vector_info.hpp"
#include "vectors.hpp"
#include "vmx.hpp"
#include "vmx_preemption_timer.hpp"

void Ec::handle_vmx()
{
    Cpu::hazard() |= HZD_DS_ES | HZD_TR;
    Cpu::setup_msrs();

    // A VM exit occured. We pass the control flow to the vCPU object and let it handle the exit.
    assert(current()->vcpu != nullptr);
    current()->vcpu->handle_vmx();
}
