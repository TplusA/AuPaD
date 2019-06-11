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

#ifndef DEVICE_MODELS_HH
#define DEVICE_MODELS_HH

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#include "json.hh"
#pragma GCC diagnostic pop

namespace StaticModels
{

/*!
 * All models as read from the JSON database.
 *
 * This class is basically a JSON object with a name, extended by some helper
 * functions. Not much validation of model integrity is done in here.
 */
class DeviceModels
{
  private:
    nlohmann::json config_data_;

  public:
    DeviceModels(const DeviceModels &) = delete;
    DeviceModels(DeviceModels &&) = default;
    DeviceModels &operator=(const DeviceModels &) = delete;
    DeviceModels &operator=(DeviceModels &&) = default;
    explicit DeviceModels() = default;

    bool load(const std::string &config, bool suppress_error = false);
    bool load(const char *config, bool suppress_error = false);
    const nlohmann::json &get_device_model(const std::string &device_id) const;
};

}

#endif /* !DEVICE_MODELS_HH */
