#include <iostream>

#include "config_file.hpp"

// TODO replace with std::filesystem::path
std::vector<boost::filesystem::path> get_config_directories()
{
    std::vector<boost::filesystem::path> paths;
    if (auto result = std::getenv("XDG_CONFIG_HOME"))
    {
        paths.push_back(result);
    }
    if (auto result = std::getenv("HOME"))
    {
        auto path = result / boost::filesystem::path(".config");
        if (paths.empty() || paths.back() != path)
            paths.push_back(path);
    }
    paths.emplace_back(".");
    return paths;
}

std::optional<boost::filesystem::path> find_or_create_config_file(std::string filename)
{
    using namespace std;
    using namespace boost::filesystem;

    optional<path> opt_cfg_path;

    auto const & cfg_dirs = get_config_directories();
    for (auto const & cfg_dir : cfg_dirs)
    {
        auto tmp_path = cfg_dir / PACKAGE_NAME / filename;

        if (exists(tmp_path))
        {
            cout << "Found configuration file: " << tmp_path.c_str() << endl;
            opt_cfg_path = tmp_path;
            break;
        }
    }

    path cfg_path;
    if (opt_cfg_path.has_value())
    {
        return opt_cfg_path;
    }
    else
    {
        boost::system::error_code ec;

        // Copy the default configuration to the best path.
        auto cfg_base_path = cfg_dirs.front() / PACKAGE_NAME;

        if (create_directories(cfg_base_path, ec))
        {
            cout << "Created directory: " << cfg_base_path.c_str() << endl;
        }

        if (ec != boost::system::errc::success)
        {
            cerr << "Failed to create config directory: " << ec.message() << endl;
            return std::nullopt;
        }

        cfg_path = cfg_base_path / filename;
        copy(path(PKGDATA) / filename, cfg_path, ec);
        if (ec != boost::system::errc::success)
        {
            cerr << "Failed to create configuration file: " << ec.message() << endl;
            return std::nullopt;
        }
        else
        {

            cout << "Created configuration file: " << cfg_path.c_str() << endl;
            return std::make_optional(cfg_path);

        }
    }
}
