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

#ifndef CONFIGSTORE_JSON_HH
#define CONFIGSTORE_JSON_HH

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#include "json.hh"
#pragma GCC diagnostic pop

namespace ConfigStore
{

class Changes;
class Settings;

/*!
 * Read-only wrapper around #ConfigStore::Settings for direct use of JSON.
 *
 * \see
 *     #ConfigStore::Settings
 */
class ConstSettingsJSON
{
  private:
    const Settings &settings_;

  public:
    ConstSettingsJSON(const ConstSettingsJSON &) = delete;
    ConstSettingsJSON(ConstSettingsJSON &&) = default;
    ConstSettingsJSON &operator=(const ConstSettingsJSON &) = delete;
    ConstSettingsJSON &operator=(ConstSettingsJSON &&) = default;

    explicit ConstSettingsJSON(const Settings &settings):
        settings_(settings)
    {}

    nlohmann::json json() const;
};

/*!
 * Wrapper around #ConfigStore::Settings for direct use of JSON.
 *
 * This wrapper is a compile-time optimization. Including the json.hh file
 * consumes a lot of time, so we make it optional. Use the
 * #ConfigStore::Settings class directly if it suits your needs, otherwise, if
 * you have or need JSON object, use this one.
 */
class SettingsJSON
{
  private:
    Settings &settings_;
    ConstSettingsJSON const_settings_;

  public:
    SettingsJSON(const SettingsJSON &) = delete;
    SettingsJSON(SettingsJSON &&) = default;
    SettingsJSON &operator=(const SettingsJSON &) = delete;
    SettingsJSON &operator=(SettingsJSON &&) = default;

    explicit SettingsJSON(Settings &settings):
        settings_(settings),
        const_settings_(settings)
    {}

    const ConstSettingsJSON &const_iface() const { return const_settings_; }

    void update(const nlohmann::json &j);
    bool extract_changes(Changes &changes);
};

}

#endif /* !CONFIGSTORE_JSON_HH */
