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

#ifndef CONFIGSTORE_ITER_HH
#define CONFIGSTORE_ITER_HH

#include <functional>
#include <string>

class Device;

namespace ConfigStore
{

class Value;
class Settings;

/*!
 * Context for iterating of live settings in an appliance instance.
 */
class DeviceContext
{
  public:
    using SettingReportFn =
        std::function<bool(const std::string &element_name,
                           const std::string &value_name, const Value &value)>;

  private:
    const Device &device_;

  public:
    DeviceContext(const DeviceContext &) = delete;
    DeviceContext(DeviceContext &&) = default;
    DeviceContext &operator=(const DeviceContext &) = delete;
    DeviceContext &operator=(DeviceContext &&) = default;

    explicit DeviceContext(const Device &device):
        device_(device)
    {}

    void for_each_setting(const SettingReportFn &apply) const;
};

/*!
 * Iterator manager over live settings as reported by the appliance.
 */
class SettingsIterator
{
  private:
    const Settings &settings_;

  public:
    SettingsIterator(const SettingsIterator &) = delete;
    SettingsIterator(SettingsIterator &&) = default;
    SettingsIterator &operator=(const SettingsIterator &) = delete;
    SettingsIterator &operator=(SettingsIterator &&) = default;

    explicit SettingsIterator(const Settings &settings):
        settings_(settings)
    {}

    DeviceContext with_device(const char *device_name) const;
    DeviceContext with_device(const std::string &device_name) const;
};

}

#endif /* !CONFIGSTORE_ITER_HH */
