/*
 * Algorithms
 *
 * Copyright (C) 2019 Julian Stecklina, Cyberus Technology GmbH.
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

template <typename IT, typename IT_END, typename T>
T accumulate (IT begin, IT_END end, T init)
{
    for (; begin != end; ++begin) {
        init += *begin;
    }

    return init;
}

template <typename IT, typename IT_END, typename PRED>
IT find_if (IT begin, IT_END end, PRED predicate)
{
    for (; begin != end and not predicate (*begin); ++begin) {
    }

    return begin;
}

template <typename IT, typename IT_END, typename FN>
void for_each (IT begin, IT_END end, FN fn)
{
    for (; begin != end; ++begin) {
        fn (*begin);
    }
}
