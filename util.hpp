#ifndef UTIL_HPP
#define UTIL_HPP

#include <mutex>
#include <string>

#ifdef _WIN32
char const PATH_SEP = '\\';
#else
char const PATH_SEP = '/';
#endif

typedef std::lock_guard<std::mutex> scoped_lock;

std::string basename(std::string path);

std::string absolute_cover_path(std::string base_dir, std::string rel_song_dir_path);

std::string string_from_ptr(char const * const ptr);

int utf8_byte_count(uint8_t start_byte);

int next_utf8(char * buff, char const * ptr);

bool is_utf8_following_byte(uint8_t b);

// assumes a non-empty and well-formed utf8 string
int count_utf8_backwards(char const * ptr);
#endif
