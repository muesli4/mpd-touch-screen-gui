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

#endif
