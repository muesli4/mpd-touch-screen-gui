#ifndef CONFIG_FILE_HPP
#define CONFIG_FILE_HPP

#include <optional>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>

std::vector<boost::filesystem::path> get_config_directories();
std::optional<boost::filesystem::path> find_or_create_config_file(std::string filename);

#endif

