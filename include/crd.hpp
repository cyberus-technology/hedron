/*
 * Capability Range Descriptor (CRD)
 *
 * Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * Copyright (C) 2012 Udo Steinberg, Intel Corporation.
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

#include "compiler.hpp"
#include "types.hpp"

class Crd
{
    private:
        mword val;

    public:
        enum Type
        {
            MEM = 1,
            PIO = 2,
            OBJ = 3,
        };

        ALWAYS_INLINE
        inline explicit Crd() : val (0) {}

        ALWAYS_INLINE
        inline explicit Crd (mword v) : val (v) {}

        ALWAYS_INLINE
        inline explicit Crd (Type t, mword b = 0, mword o = 0x1f, mword a = 0x1f) : val (b << 12 | o << 7 | a << 2 | t) {}

        ALWAYS_INLINE
        inline Type type() const { return static_cast<Type>(val & 0x3); }

        ALWAYS_INLINE
        inline unsigned attr() const { return val >> 2 & 0x1f; }

        ALWAYS_INLINE
        inline unsigned order() const { return val >> 7 & 0x1f; }

        ALWAYS_INLINE
        inline mword base() const { return val >> 12; }

        ALWAYS_INLINE
        inline mword value() const { return val; }
};

class Xfer
{
    private:
        Crd   xfer_crd;
        mword xfer_meta;

    public:
        ALWAYS_INLINE
        inline explicit Xfer (Crd c, mword v) : xfer_crd (c), xfer_meta (v) {}

        ALWAYS_INLINE
        inline mword flags() const { return xfer_meta & 0xfff; }

        ALWAYS_INLINE
        inline mword hotspot() const { return xfer_meta >> 12; }

        ALWAYS_INLINE
        inline mword metadata() const { return xfer_meta; }

        ALWAYS_INLINE
        inline Crd crd() const { return xfer_crd; }

        enum class Kind
        {
            TRANSLATE      = 0,
            DELEGATE       = 1,
            TRANS_DELEGATE = 2,
            INVALID        = 3,
        };

        ALWAYS_INLINE
        inline Kind kind() const { return Kind (xfer_meta & 0x3); }

        ALWAYS_INLINE
        inline mword subspaces() const { return (xfer_meta >> 9) & 0x3; }

        ALWAYS_INLINE
        inline bool from_kern() const { return flags() & 0x800; }
};
