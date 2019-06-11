/*
 * Copyright (C) 2019  T+A elektroakustik GmbH & Co. KG
 *
 * This file is part of AuPaD.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "device_models.hh"
#include "messages.h"

#include <fstream>

static bool do_load(std::ifstream &in, bool suppress_error,
                    nlohmann::json &config_data,
                    const std::function<void(const char *)> &emit_error)
{
    if(!in)
    {
        if(!suppress_error)
            emit_error("Failed reading models configuration file \"%s\"");

        return false;
    }

    try
    {
        in >> config_data;
        return true;
    }
    catch(const std::exception &e)
    {
        msg_error(0, LOG_NOTICE, "%s", e.what());
        config_data = nlohmann::json();
        return false;
    }
}

bool StaticModels::DeviceModels::load(const std::string &config,
                                      bool suppress_error)
{
    std::ifstream in(config);
    return do_load(in, suppress_error, config_data_,
                   [&config] (const char *msg)
                   { msg_error(0, LOG_ERR, msg, config.c_str()); });
}

bool StaticModels::DeviceModels::load(const char *config,
                                      bool suppress_error)
{
    std::ifstream in(config);
    return do_load(in, suppress_error, config_data_,
                   [&config] (const char *msg)
                   { msg_error(0, LOG_ERR, msg, config); });
}

const nlohmann::json
&StaticModels::DeviceModels::get_device_model(const std::string &device_id) const
{
    try
    {
        return config_data_.at("all_devices").at(device_id);
    }
    catch(...)
    {
        static const nlohmann::json empty;
        return empty;
    }
}
