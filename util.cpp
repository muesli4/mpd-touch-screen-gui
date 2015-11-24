#include "util.hpp"

std::string basename(std::string path)
{
#ifdef _WIN32
    char const PATH_SEP = '\\';
#else
    char const PATH_SEP = '/';
#endif

    return path.substr(0, path.find_last_of(PATH_SEP) + 1);
}
