/*
 * Multiboot2 support
 *
 * Copyright (C) 2017 Alexander Boettcher, Genode Labs GmbH
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
#include "math.hpp"

namespace Multiboot2
{

enum
{
    MAGIC = 0x36d76289,
    TAG_END = 0,
    TAG_CMDLINE = 1,
    TAG_MODULE = 3,
    TAG_MEMORY = 6,
    TAG_EFI_ST = 12,
    TAG_ACPI_2 = 15,
};

class Memory_map
{
public:
    uint64 addr;
    uint64 len;
    uint32 type;
    uint32 reserved;
};

class Module
{
public:
    uint32 s_addr;
    uint32 e_addr;
    char string[0];
};

class Tag
{
public:
    uint32 type;
    uint32 size;

    inline char const* cmdline() const
    {
        if (type != TAG_CMDLINE)
            return nullptr;

        return reinterpret_cast<char const*>(this + 1);
    }

    inline Module const* module() const
    {
        if (type != TAG_MODULE)
            return nullptr;

        return reinterpret_cast<Module*>(reinterpret_cast<mword>(this + 1));
    }

    inline mword rsdp() const
    {
        if (type != TAG_ACPI_2)
            return 0;

        return reinterpret_cast<mword>(this + 1);
    }

    template <typename FUNC> inline void for_each_mem(FUNC const& fn) const
    {
        if (type != TAG_MEMORY)
            return;

        Memory_map const* s = reinterpret_cast<Memory_map*>(reinterpret_cast<mword>(this + 1) + 8);
        Memory_map const* e = reinterpret_cast<Memory_map*>(reinterpret_cast<mword>(this) + size);

        for (Memory_map const* i = s; i < e; i++)
            fn(i);
    }
};

class Header : public Tag
{
private:
    inline uint32 total_size() const { return type; }

public:
    template <typename FUNC> inline void for_each_tag(FUNC const& fn) const
    {
        Tag const* s = this + 1;
        Tag const* e = reinterpret_cast<Tag const*>(reinterpret_cast<mword>(this) + total_size());

        for (Tag const* i = s; i < e && !(i->type == TAG_END && i->size == sizeof(Tag));) {
            fn(i);
            i = reinterpret_cast<Tag const*>(reinterpret_cast<mword>(i) + align_up(i->size, sizeof(Tag)));
        }
    }
};

} // namespace Multiboot2
