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

std::string basename(std::string path);

std::string absolute_cover_path(std::string base_dir, std::string rel_song_dir_path);

std::string string_from_ptr(char const * const ptr);

std::optional<std::string> find_cover_file(std::string rel_song_dir_path, std::string base_dir, std::vector<std::string> names, std::vector<std::string> extensions);

#endif
