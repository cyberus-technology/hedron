/*
 * Capability Range Descriptor (CRD)
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

    inline explicit Crd() : val(0) {}

    inline explicit Crd(mword v) : val(v) {}

    inline explicit Crd(Type t, mword b = 0, mword o = 0x1f, mword a = 0x1f)
        : val(b << 12 | o << 7 | a << 2 | t)
    {
    }

    inline Type type() const { return static_cast<Type>(val & 0x3); }

    inline unsigned attr() const { return val >> 2 & 0x1f; }

    inline unsigned order() const { return val >> 7 & 0x1f; }

    inline mword base() const { return val >> 12; }

    inline mword value() const { return val; }
};

// A typed IPC item used to transfer capabilities.
//
// See `enum Kind` for possible variants of capability transfers.
class Xfer
{
private:
    Crd xfer_crd;
    mword xfer_meta;

public:
    inline explicit Xfer(Crd c, mword v) : xfer_crd(c), xfer_meta(v) {}

    inline mword flags() const { return xfer_meta & 0xfff; }

    inline mword hotspot() const { return xfer_meta >> 12; }

    inline mword metadata() const { return xfer_meta; }

    inline Crd crd() const { return xfer_crd; }

    enum class Kind
    {
        TRANSLATE = 0,
        DELEGATE = 1,
        TRANS_DELEGATE = 2,
        INVALID = 3,
    };

    inline Kind kind() const { return Kind(xfer_meta & 0x3); }

    // Which subspaces are the target of this mapping.
    //
    // The lowest bit is the HOST subspace and it is currently inverted for
    // backward compatibility.
    inline mword subspaces() const { return ((xfer_meta >> 8) & 0x7) ^ 1; }

    // If true, the source should be the kernel PD.
    //
    // See "hypervisor" flag in Delegate Flags in the specification.
    inline bool from_kern() const { return flags() & 0x800; }
};
