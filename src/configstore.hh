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

#ifndef CONFIGSTORE_HH
#define CONFIGSTORE_HH

#include <string>
#include <memory>

namespace StaticModels { class DeviceModelsDatabase; }

namespace ConfigStore
{

class SettingsJSON;
class SettingsIterator;

/*!
 * All settings as reported by the appliance.
 *
 * The settings stored in this object are not matched against the device
 * models. Instead, they represent raw, live data as reported by the appliance
 * as AuPaL objects.
 *
 * \see
 *     #ConfigStore::ConstSettingsJSON, #ConfigStore::SettingsJSON
 */
class Settings
{
  private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    friend class SettingsJSON;
    friend class ConstSettingsJSON;
    friend class SettingsIterator;

  public:
    Settings(const Settings &) = delete;
    Settings(Settings &&) = default;
    Settings &operator=(const Settings &) = delete;
    Settings &operator=(Settings &&) = default;

    explicit Settings(const StaticModels::DeviceModelsDatabase &models_database);
    ~Settings();

    void clear();
    void update(const std::string &d);
    std::string json_string() const;
};

}

#endif /* !CONFIGSTORE_HH */
