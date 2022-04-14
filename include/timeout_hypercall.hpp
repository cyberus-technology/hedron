/*
 * Hypercall Timeout
 *
 * Copyright (C) 2014 Udo Steinberg, FireEye, Inc.
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

#include "timeout.hpp"

class Ec;
class Sm;

class Timeout_hypercall final : public Timeout
{
private:
    Ec* const ec;
    Sm* sm{nullptr};

    virtual void trigger() override;

public:
    inline Timeout_hypercall(Ec* e) : ec(e) {}

    ~Timeout_hypercall();

    void enqueue(uint64 t, Sm* s);
};
