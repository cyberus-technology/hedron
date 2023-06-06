/*
 * Entry Functions
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
 * Copyright (C) 2015 Alexander Boettcher, Genode Labs GmbH
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
 *
 */

#include "arch.hpp"
#include "config.hpp"
#include "idt_handlers.hpp"
#include "memory.hpp"
#include "selectors.hpp"

/*
 * handlers is an array of entry points for interrupts. We align entries
 * to have space for metainformation in the low bits. See idt.cpp for how
 * the handlers array is converted into the IDT.
 */
.data
.global                 handlers
handlers:

.text

.macro                  INTRGATE MODE
                        .align  4, 0x90
1:                      .data
                        .quad   1b + \MODE
                        .previous
.endm

.macro                  RESTORE_STACK
                        mov     %gs:0, %rsp // Per_cpu::self
.endm

.macro                  SAVE_STATE
                        // RSP must point to the _end_ of a Sys_regs structure.
                        SAVE_GPR
                        mov     %rsp, %rbx
                        cld

                        // If CS[1:0] != 0, we come from userspace.
                        testb   $RPL_MASK, OFS_CS(%rsp)
                        jz      1f
                        swapgs
                        RESTORE_STACK
1:
.endm

.macro                  EXCEPTION VEC, MODE
INTRGATE                \MODE
                        push    $\VEC
.if \MODE == IDT_MODE_DPL0_ALTSTACK
                        jmp     entry_exc_altstack
.else
                        jmp     entry_exc
.endif
.endm

.macro                  EXCEPTION_ERROR VEC, MODE
INTRGATE                \MODE
                        push    (%rsp)
                        movq    $\VEC, 8(%rsp)
.if \MODE == IDT_MODE_DPL0_ALTSTACK
                        jmp     entry_exc_error_altstack
.else
                        jmp     entry_exc_error
.endif
.endm

/*
 * Exception Entries
 */
entry_exc:              push    $0
entry_exc_error:        SAVE_STATE
                        mov     %cr2, %rax
                        mov     %rax, OFS_CR2 (%rbx)
                        mov     %rbx, %ARG_1
                        call    exc_handler
                        jmp     ret_from_interrupt

/*
 * Alternate Stack Exception Entries
 *
 * We assume here that we interrupted the kernel at an arbitrary position even
 * where interrupts are normally disabled.
 */

entry_exc_altstack:     push    $0
entry_exc_error_altstack:
                        SAVE_GPR
                        mov     %rsp, %ARG_1
                        cld

                        // ARG_1 = Exc_regs
                        call    exc_handler_altstack

                        LOAD_GPR
                        add     $(2 * PTR_SIZE), %rsp
                        iretq

EXCEPTION               0x0,    IDT_MODE_DPL0
EXCEPTION               0x1,    IDT_MODE_DPL0
EXCEPTION               0x2,    IDT_MODE_DPL0_ALTSTACK
EXCEPTION               0x3,    IDT_MODE_DPL3
EXCEPTION               0x4,    IDT_MODE_DPL3
EXCEPTION               0x5,    IDT_MODE_DPL0
EXCEPTION               0x6,    IDT_MODE_DPL0
EXCEPTION               0x7,    IDT_MODE_DPL0
EXCEPTION_ERROR         0x8,    IDT_MODE_DPL0_ALTSTACK
EXCEPTION               0x9,    IDT_MODE_DPL0 // Unused
EXCEPTION_ERROR         0xa,    IDT_MODE_DPL0
EXCEPTION_ERROR         0xb,    IDT_MODE_DPL0
EXCEPTION_ERROR         0xc,    IDT_MODE_DPL0
EXCEPTION_ERROR         0xd,    IDT_MODE_DPL0
EXCEPTION_ERROR         0xe,    IDT_MODE_DPL0
EXCEPTION               0xf,    IDT_MODE_DPL0 // Unused
EXCEPTION               0x10,   IDT_MODE_DPL0
EXCEPTION_ERROR         0x11,   IDT_MODE_DPL0
EXCEPTION               0x12,   IDT_MODE_DPL0
EXCEPTION               0x13,   IDT_MODE_DPL0

/*
 * Reserved Exception Entries
 */
.set e,0x14
.rept                   NUM_EXC - 0x14
EXCEPTION               e,      IDT_MODE_DPL0
 .set                   e,      e+1
.endr

/*
 * We add an IDT entry for each possible interrupt vector to make sure we
 * panic if Hedron receives an interrupt.
 */
.set                    VEC, NUM_EXC
.rept                   NUM_INT_VECTORS - NUM_EXC
INTRGATE                IDT_MODE_DPL0
                        push    $VEC
                        jmp     entry_interrupt
.set                    VEC, VEC + 1
.endr

entry_interrupt:        push    $0
                        SAVE_STATE
                        mov     OFS_VEC (%rbx), %ARG_1
                        call    handle_interrupt

/*
 * Interrupt Return
 */
ret_from_interrupt:     testb   $3, OFS_CS (%rbx)
                        jnz     ret_user_iret
                        LOAD_GPR
                        add     $(2 * PTR_SIZE), %rsp
                        iretq

/*
 * System-Call Entry
 */
.align                  4, 0x90
.globl                  entry_sysenter
entry_sysenter:

                        /*
                         * Syscall saves userspace RFLAGS in R11, but we do not
                         * need to save it, because we just set a default value when
                         * returning. This leaves R11 free to save the user RSP.
                         */
                        mov     %rsp, %r11

                        swapgs
                        mov     %gs:8, %rsp // Per_cpu::sys_entry_stack
                        SAVE_GPR
                        RESTORE_STACK
                        jmp     syscall_handler

/*
 * VMX Entry
 *
 * The stack pointer points just behind a Sys_regs structure, so the guest state
 * can be pushed into it without using additional registers.
 *
 * In case of VMRESUME/VMLAUNCH failure, we skip saving guest registers, because
 * they cannot have been changed and just restore the stack.
 */
.align                  4, 0x90
.globl                  entry_vmx
.globl                  entry_vmx_failure
entry_vmx:              SAVE_GPR
                        mov     %cr2, %rax
                        mov     %rax, OFS_CR2 (%rsp)
entry_vmx_failure:      RESTORE_STACK
                        jmp     vmx_handler

.section .note.GNU-stack,"",%progbits
