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

std::string absolute_cover_path(std::string_view const base_dir, std::string_view const rel_song_dir_path)
{
    // try to detect, whether we need to look for the album cover in the super directory
    std::size_t const super_dir_sep_pos = rel_song_dir_path.find_last_of(PATH_SEP, rel_song_dir_path.size() - 2);

    bool has_discnumber = false;
    if (super_dir_sep_pos != std::string::npos)
    {
        std::string_view const super_dir =
            rel_song_dir_path.substr(super_dir_sep_pos + 1, rel_song_dir_path.size() - 2 - super_dir_sep_pos);

        if (super_dir.find("CD ") == 0
            && std::all_of(std::next(super_dir.begin(), 3), super_dir.end(), ::isdigit)
           )
        {
            has_discnumber = true;
        }
    }

    return std::string(base_dir) + std::string(has_discnumber ? rel_song_dir_path.substr(0, super_dir_sep_pos + 1) : rel_song_dir_path);
}

std::string string_from_ptr(char const * const ptr)
{
    return ptr == 0 ? "" : ptr;
}

std::string_view remove_first_directory(std::string_view path)
{
    std::size_t pos = path.find(PATH_SEP);
    return pos != std::string::npos ? path.substr(pos + 1) : path;
}

std::optional<std::string> find_cover_file(std::string_view const rel_song_dir_path, std::string_view const base_dir, std::vector<std::string> names, std::vector<std::string> extensions)
{
    // Try to guess a fuzzy location if the one given by MPD doesn't work
    // (i.e. up to two times remove the directory in front).
    //
    // E.g., if on the host the songs are stored in an auto-mounted directory
    // but not on the client. Assume there is a mirror that is not auto-mounted.
    //
    // TODO Allow functionality to be tuned off.
    std::string_view p = remove_first_directory(rel_song_dir_path);
    std::string_view custom_rel_song_dir_paths[] =
        { rel_song_dir_path
        , p
        , remove_first_directory(p)
        };
    for (auto const & custom_rel_song_dir_path : custom_rel_song_dir_paths)
    {
        std::string const abs_cover_dir = absolute_cover_path(base_dir, basename(custom_rel_song_dir_path));
        for (auto const & name : names)
        {
            for (auto const & ext : extensions)
            {
                std::string const cover_path = abs_cover_dir + '/' + name + "." + ext;
                if (boost::filesystem::exists(boost::filesystem::path(cover_path)))
                {
                    return cover_path;
                }
            }
        }
    }

    return std::nullopt;
}

