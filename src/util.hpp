// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef UTIL_HPP
#define UTIL_HPP

#include <mutex>
#include <string>
#include <optional>
#include <vector>

#ifdef _WIN32
char const PATH_SEP = '\\';
#else
char const PATH_SEP = '/';
#endif

typedef std::lock_guard<std::mutex> scoped_lock;

std::string_view basename(std::string_view const path);

std::string absolute_cover_path(std::string_view const base_dir, std::string_view const rel_song_dir_path);

std::string string_from_ptr(char const * const ptr);

std::optional<std::string> find_cover_file(std::string_view const rel_song_dir_path, std::string_view const base_dir, std::vector<std::string> names, std::vector<std::string> extensions);

#endif
