// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "util.hpp"

#include <algorithm>

#include <boost/filesystem.hpp>

std::string_view basename(std::string_view const path)
{
    return path.substr(0, path.find_last_of(PATH_SEP) + 1);
}

std::string string_from_ptr(char const * const ptr)
{
    return ptr == 0 ? "" : ptr;
}

