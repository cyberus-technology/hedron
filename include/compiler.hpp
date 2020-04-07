/*
 * Compiler Macros
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.
 *
 * Copyright (C) 2018 Stefan Hertrampf, Cyberus Technology GmbH.
 *
 * This file is part of the NOVA microhypervisor.
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

#pragma once

#define STRING(x...) #x
#define EXPAND(x) STRING(x)

#if defined(__GNUC__)

    #if defined(__clang__)

        #define COMPILER_STRING     "clang " __clang_version__
        #define COMPILER_VERSION    (__clang_major__ * 100 + __clang_minor__ * 10 + __clang_patchlevel__)

        #if (COMPILER_VERSION < 700)
            #error "Please upgrade clang to a supported version"
        #endif

        // Certain functions cannot be marked noreturn, because of this clang issue:
        // https://bugs.llvm.org/show_bug.cgi?id=42651
        #define NORETURN_GCC
        #define FALL_THROUGH [[clang::fallthrough]]

    #else  // GCC

        #define COMPILER_STRING     "gcc " __VERSION__

        #if defined(__GNUC_PATCHLEVEL__)
            #define COMPILER_VERSION    (__GNUC__ * 100 + __GNUC_MINOR__ * 10 + __GNUC_PATCHLEVEL__)
        #else
            #define COMPILER_VERSION    (__GNUC__ * 100 + __GNUC_MINOR__ * 10)
        #endif

        #if (COMPILER_VERSION < 700)
            #error "Please upgrade GCC to a supported version"
        #endif

        #define NORETURN_GCC NORETURN
        #define FALL_THROUGH [[gnu::fallthrough]]

    #endif

    #define COLD                __attribute__((cold))
    #define HOT                 __attribute__((hot))
    #define UNREACHED           __builtin_unreachable()

    #define ALIGNED(X)          __attribute__((aligned(X)))

    // The CPU-local sections do not exist on hosted builds, so prevent people
    // from harming themselves.
    #if !__STDC_HOSTED__
        #define CPULOCAL            __attribute__((section (".cpulocal,\"w\",@nobits#")))
        #define CPULOCAL_HOT        __attribute__((section (".cpulocal.hot,\"w\",@nobits#")))
    #endif

    #define FORMAT(X,Y)         __attribute__((format (printf, (X),(Y))))

    // On hosted builds, we do not use a linker script, so linking into these
    // special section may cause problems.
    //
    // On clang 9, this causes INIT functions to be called randomly when static
    // constructors are called.
    #if __STDC_HOSTED__
        #define INIT
        #define INITDATA
    #else
        #define INIT                __attribute__((section (".init")))
        #define INITDATA            __attribute__((section (".initdata")))
    #endif

    #define INIT_PRIORITY(X)    __attribute__((init_priority((X))))
    #define NOINLINE            __attribute__((noinline))
    #define NONNULL             __attribute__((nonnull))
    #define NORETURN            __attribute__((noreturn))
    #define PACKED              __attribute__((packed))
    #define REGPARM(X)          __attribute__((regparm(X)))
    #define WARN_UNUSED_RESULT  __attribute__((warn_unused_result))
    #define USED                __attribute__((used))

    #define EXPECT_FALSE(X)     __builtin_expect(!!(X), 0)
    #define EXPECT_TRUE(X)      __builtin_expect(!!(X), 1)

    #define FIXUP_CALL(insn)    "1: " #insn "; 2:\n" \
                                ".section .fixup,\"a\"; .align 8;" EXPAND (WORD) " 1b,2b; .previous"

    #define OFFSETOF(type, m)   __builtin_offsetof (type, m)

#else
        #error "Unknown compiler"
#endif
