/*
 * Virtual CPU
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

#include "fpu.hpp"
#include "kobject.hpp"
#include "kp.hpp"
#include "mtd.hpp"
#include "pd.hpp"
#include "refptr.hpp"
#include "slab.hpp"
#include "unique_ptr.hpp"
#include "utcb.hpp"
#include "vlapic.hpp"
#include "vmx.hpp"
#include "vmx_msr_bitmap.hpp"

// A struct that is passed to the vCPU's constructor.
struct Vcpu_init_config {
    Pd* owner_pd;
    mword cap_selector;
    Kp* kp_vcpu_state;
    Kp* kp_vlapic_page;
    Kp* kp_fpu_state;
};

// A virtual CPU. Objects of this class are passive objects, i.e. they have no associated SC and can only run
// when user space executes a `vcpu_ctrl_run` system call.
class Vcpu : public Typed_kobject<Kobject::Type::VCPU>, public Refcount
{
private:
    static Slab_cache cache;

    // RCU callback that is used to delete this object.
    static void free(Rcu_elem* a)
    {
        Vcpu* vcpu{static_cast<Vcpu*>(a)};
        delete vcpu;
    }

    const Refptr<Pd> pd; // The protection domain the vCPU will run in.

    const Refptr<Kp> kp_vcpu_state;
    const Refptr<Kp> kp_vlapic_page;
    const Refptr<Kp> kp_fpu_state;

    const unsigned cpu_id; // The ID of the CPU this vCPU is running on.
    Unique_ptr<Vmcs> vmcs;
    Unique_ptr<Msr_area> guest_msr_area;
    Unique_ptr<Vmx_msr_bitmap> msr_bitmap;

    Fpu fpu;

public:
    // Capability permission bitmask.
    enum
    {
        PERM_VCPU_CTRL = 1U << 0,

        PERM_ALL = PERM_VCPU_CTRL,
    };

    explicit Vcpu(const Vcpu_init_config& init_cfg);
    ~Vcpu() = default;

    static inline void* operator new(size_t) { return cache.alloc(); }
    static inline void operator delete(void* ptr) { cache.free(ptr); }
};
