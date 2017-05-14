#include "util.hpp"

#include <algorithm>

std::string basename(std::string path)
{
    return path.substr(0, path.find_last_of(PATH_SEP) + 1);
}

std::string absolute_cover_path(std::string base_dir, std::string rel_song_dir_path)
{
    // try to detect, whether we need to look for the album cover in the super directory
    std::size_t const super_dir_sep_pos = rel_song_dir_path.find_last_of(PATH_SEP, rel_song_dir_path.size() - 2);

    bool has_discnumber = false;
    if (super_dir_sep_pos != std::string::npos)
    {
        std::string const super_dir =
            rel_song_dir_path.substr(super_dir_sep_pos + 1, rel_song_dir_path.size() - 2 - super_dir_sep_pos);

        if (super_dir.find("CD ") == 0
            && std::all_of(std::next(super_dir.begin(), 3), super_dir.end(), ::isdigit)
           )
        {
            has_discnumber = true;
        }
    }

    return std::string(base_dir) + (has_discnumber ? rel_song_dir_path.substr(0, super_dir_sep_pos + 1) : rel_song_dir_path);
}

std::string string_from_ptr(char const * const ptr)
{
    return ptr == 0 ? "" : ptr;
}
