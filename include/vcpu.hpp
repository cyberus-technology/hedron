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
#include "regs.hpp"
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

// Acquiring a vCPU can fail for multiple reasons. See the different constructors below.
struct Vcpu_acquire_error {
    enum class type
    {
        BUSY,
        BAD_CPU,
    };

    type error_type;

    Vcpu_acquire_error() = delete;
    Vcpu_acquire_error(type error_type_) : error_type(error_type_) {}

    // Acquiring a vCPU failed because the vCPU already has an owner.
    static Vcpu_acquire_error busy() { return type::BUSY; }

    // Acquiring the vCPU failed because the vCPU cannot run on the CPU the acquiring EC is running on.
    static Vcpu_acquire_error bad_cpu() { return type::BAD_CPU; }
};

using Vcpu_acquire_result = Result_void<Vcpu_acquire_error>;

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

    Utcb* utcb() { return reinterpret_cast<Utcb*>(kp_vcpu_state.get()->data_page()); }

    const unsigned cpu_id; // The ID of the CPU this vCPU is running on.
    Unique_ptr<Vmcs> vmcs;
    Unique_ptr<Msr_area> guest_msr_area;
    Unique_ptr<Vmx_msr_bitmap> msr_bitmap;

    // The VMCS does not contain general-purpose register content, so we have to save them separately.
    //
    // TODO: When we decouple the vCPU-State and the UTCB in the future, the VM exit path can store the
    // registers directly in the vCPU state page. See hedron#252.
    Cpu_regs regs;

    // The first thing we do after a VM exit is to save the general-purpose register content by pushing it
    // onto the stack. By making the host rsp point to the regs-structure we ensure that the register content
    // is stored in this structure.
    mword host_rsp() { return reinterpret_cast<mword>(static_cast<Sys_regs*>(&regs) + 1); }

    Fpu fpu;

    // The EC this vCPU is currently executing on. This variable has to be set prior to any modifications to
    // the vCPUs state and cleared before returning to the VMM. If a EC tries to modify the vCPUs state
    // without being the owner, this is a bug!
    //
    // This pointer is also a 'busy'-flag. Using this pointer instead of a boolean has the advantage that no
    // other EC can modify this vCPU while a EC that is already executing this vCPU is currently not
    // scheduled.
    //
    // This pointer must be accessed using atomic ops!
    Ec* owner{nullptr};

    // True if we entered this vCPU at least once during Vcpu::run.
    //
    // Vcpu::run sets this flag and Vcpu::return_to_vmm clears it. We use this value to avoid unnecessary VM
    // entries when the vCPU is poked.
    //
    // There is no need to access this flag using atomic ops, because Vcpu::run and Vcpu::return_to_vmm are
    // guarded by Vcpu::owner.
    bool has_entered{false};

    // True if the vCPU has been poked and must return to user space as soon as possible.
    //
    // This bool must be accessed using atomic ops!
    bool poked{false};

    // This is the value of the exit_reason_shadow-field if it is cleared. In this case the values inside the
    // VMCS has to be used.
    static constexpr uint32 EXIT_REASON_CLEAR{~0u};

    // The exit reason, if this value does not eqaul EXIT_REASON_CLEAR. Otherwise the exit reason from the
    // VMCS is the actual exit reason.
    //
    // Some CPUs cannot write to the exit_reason-field of the VMCS. Thus if we want to set a exit reason, we
    // write it to this field. The VM exit path then has to use this value instead of the one inside the VMCS.
    uint32 exit_reason_shadow{Vcpu::EXIT_REASON_CLEAR};

    // Clears the exit_reason_shadow-field. This has to be done early in the VM entry path.
    void clear_exit_reason_shadow() { exit_reason_shadow = Vcpu::EXIT_REASON_CLEAR; }

    // Sets the exit_reason_shadow-field to the given exit_reason.
    void set_exit_reason_shadow(uint32 exit_reason) { exit_reason_shadow = exit_reason; }

    // Returns the current exit reason. This function always uses the exit_reason_shadow-field if it is not
    // cleared, otherwise it reads from the VMCS:
    uint32 get_exit_reason()
    {
        if (exit_reason_shadow == Vcpu::EXIT_REASON_CLEAR) {
            return static_cast<uint32>(Vmcs::read(Vmcs::EXI_REASON));
        }
        return exit_reason_shadow;
    }

    // Transfers the VMCS contents into the vCPU state page, sets the given exit reason in the vCPU state
    // page and returns to the VMM with the given status.
    [[noreturn]] void return_to_vmm(uint32 exit_reason, Sys_regs::Status status);

    // Called during handling of a VM exit. This function prepares the vCPU to do another VM entry and then
    // passes control flow to Ec::run_vcpu.
    [[noreturn]] void continue_running();

    // Handles a VM exit due to an exception.
    [[noreturn]] inline void handle_exception();

    // Handles a VM exit due to an extint.
    [[noreturn]] inline void handle_extint();

public:
    // Capability permission bitmask.
    enum
    {
        PERM_VCPU_CTRL = 1U << 0,

        PERM_ALL = PERM_VCPU_CTRL,
    };

    explicit Vcpu(const Vcpu_init_config& init_cfg);
    ~Vcpu() = default;

    // Trys to set the current EC as the new owner. ECs are only allowed to modify the vCPUs state or to run
    // it after a successful call to this function. The owner of a vCPU has the duty to release it, the vCPU
    // will never clear its owner by itself.
    Vcpu_acquire_result try_acquire();

    // Clears the owner of this vCPU. Only the owner of a vCPU is allowed to release the vCPU.
    void release();

    // Unions the current MTD-bits of this vCPU with the given MTD-bits. An EC has to acquire this vCPU before
    // modifying its MTD bits!
    void mtd(Mtd mtd);

    // Prepares this vCPU to be executed (e.g. transfers the modified vCPU state fields) and then enters this
    // vCPU. An EC has to acquire this vCPU before it is allowed to execute it.
    [[noreturn]] void run();

    // Handles a VM exit. This function should only be called by a EC in its VM exit path!
    [[noreturn]] void handle_vmx();

    static inline void* operator new(size_t) { return cache.alloc(); }
    static inline void operator delete(void* ptr) { cache.free(ptr); }
};
