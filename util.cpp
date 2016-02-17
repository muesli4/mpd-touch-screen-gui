#include "util.hpp"

std::string basename(std::string path)
{
    return path.substr(0, path.find_last_of(PATH_SEP) + 1);
}
