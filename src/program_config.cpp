// SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <cstdlib>
#include <vector>
#include <libconfig.h++>
#include "program_config.hpp"

bool parse_vec(libconfig::Setting & s, vec & result)
{
    return s.lookupValue("width", result.w)
        && s.lookupValue("height", result.h);
}

bool parse_display_config(libconfig::Setting & s, display_config & result)
{
    return s.lookupValue("fullscreen", result.fullscreen)
        && parse_vec(s.lookup("resolution"), result.resolution);
}

bool parse_system_control_config(libconfig::Setting & s, system_control_config & result)
{
    return s.lookupValue("shutdown_command", result.shutdown_command)
        && s.lookupValue("reboot_command", result.reboot_command);
}

bool parse_dim_idle_timer_config(libconfig::Setting & s, dim_idle_timer_config & result)
{

    int tmp;
    if (s.lookupValue("delay_seconds", tmp))
    {
        result.delay = std::chrono::seconds(tmp);

        return s.lookupValue("dim_command", result.dim_command)
            && s.lookupValue("undim_command", result.undim_command);
    }

    return false;
}

/*
bool parse_swipe_config(libconfig::Setting & s, swipe_config & result)
{
    return s.lookupValue("dir_unambig_factor_threshold", result.dir_unambig_factor_threshold);
}
*/

bool parse_string_vector(libconfig::Setting & s, std::vector<std::string> & result)
{
    if (s.isArray())
    {
        result.reserve(s.getLength());
        for (auto & ns : s)
        {
            result.emplace_back(ns.operator std::string());
        }
        return true;
    }
    else
    {
        return false;
    }
}

bool parse_filesystem_cover_provider_config(libconfig::Setting & s, filesystem_cover_provider_config & result)
{
    if (s.exists("extensions") && s.exists("names") && s.exists("directory"))
    {
        if (parse_string_vector(s.lookup("extensions"), result.extensions))
        {
            if (parse_string_vector(s.lookup("names"), result.names))
            {
                return s.lookupValue("directory", result.directory);
            }
        }
    }
    return false;
}

bool parse_cover_config(libconfig::Setting & s, cover_config & result)
{
    filesystem_cover_provider_config filesystem_cover_provider;
    if (parse_filesystem_cover_provider_config(s.lookup("filesystem"), filesystem_cover_provider))
    {
        result.opt_filesystem_cover_provider = filesystem_cover_provider;
    }

    // Leave empty if it does not exist.
    parse_string_vector(s.lookup("sources"), result.sources);

    return true;
}

bool parse_on_screen_keyboard_config(libconfig::Setting & s, on_screen_keyboard_config & result)
{
    return parse_vec(s.lookup("size"), result.size)
        && s.lookupValue("keys", result.keys);
}

bool parse_font(libconfig::Setting & s, font & result)
{
    return s.lookupValue("path", result.path)
        && s.lookupValue("size", result.size);
}

bool parse_program_config(boost::filesystem::path config_path, program_config & result)
{
    libconfig::Config config;
    config.readFile(config_path.c_str());
    libconfig::Setting & program_setting = config.lookup("program");

    int tmp;
    if (program_setting.lookupValue("port", tmp))
    {
        result.opt_port = tmp;
    }

    return parse_font(program_setting.lookup("default_font"), result.default_font)
        && parse_font(program_setting.lookup("big_font"), result.big_font)
        && parse_display_config(program_setting.lookup("display"), result.display)
        && parse_system_control_config(program_setting.lookup("system_control"), result.system_control)
        && parse_dim_idle_timer_config(program_setting.lookup("dim_idle_timer"), result.dim_idle_timer)
        //&& parse_swipe_config(program_setting.lookup("swipe"), result.swipe)
        && parse_cover_config(program_setting.lookup("cover"), result.cover)
        && parse_on_screen_keyboard_config(program_setting.lookup("on_screen_keyboard"), result.on_screen_keyboard);
}
