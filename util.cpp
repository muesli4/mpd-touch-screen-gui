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

int utf8_byte_count(uint8_t start_byte)
{
    if (start_byte & 0b10000000)
    {
        int count = 0;
        while (start_byte & 0b10000000)
        {
            count++;
            start_byte <<= 1;
        }
        return count;
    }
    else
    {
        return 1;
    }
}

// extracts a utf8 character writes it to buff and return the length
int next_utf8(char * buff, char const * ptr)
{
    if (*ptr == 0)
    {
        return 0;
    }
    else
    {
        int num_bytes = utf8_byte_count(*ptr);

        for (int pos = 0; pos < num_bytes; pos++)
        {
            buff[pos] = *ptr;
            ptr++;
        }
        buff[num_bytes] = 0;

        return num_bytes;
    }
}

bool is_utf8_following_byte(uint8_t b)
{
    // following bytes have most significant bit set and 2nd most significant
    // bit cleared
    return (b & 0b10000000) && !(b & 0b01000000);
}

int count_utf8_backwards(char const * ptr)
{
    int count = 1;
    while (is_utf8_following_byte(*ptr))
    {
        count++;
        ptr--;
    }
    return count;
}

unsigned int inc_ensure_upper(unsigned int new_pos, unsigned int old_pos, unsigned int upper_bound)
{
    return new_pos < old_pos ? upper_bound : std::min(new_pos, upper_bound);
}

unsigned int dec_ensure_lower(unsigned int new_pos, unsigned int old_pos, unsigned int lower_bound)
{
    return new_pos > old_pos ? lower_bound : std::max(new_pos, lower_bound);
}
