#ifndef UTIL_HPP
#define UTIL_HPP

#include <mutex>
#include <string>

typedef std::lock_guard<std::mutex> scoped_lock;

std::string basename(std::string path);

#endif
