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

#ifndef CONFIGSTORE_PLUGIN_MANAGER_HH
#define CONFIGSTORE_PLUGIN_MANAGER_HH

#include "configstore_plugin.hh"

#include <list>
#include <memory>

namespace ConfigStore
{

class Settings;

class PluginManager
{
  private:
    std::list<std::unique_ptr<Plugin>> plugins_;

  public:
    PluginManager(const PluginManager &) = delete;
    PluginManager(PluginManager &&) = default;
    PluginManager &operator=(const PluginManager &) = delete;
    PluginManager &operator=(PluginManager &&) = default;
    explicit PluginManager() = default;

    ~PluginManager()
    {
        shutdown();
    }

    void register_plugin(std::unique_ptr<Plugin> plugin);
    void shutdown() noexcept;
    void report_changes(const Settings &settings, const Changes &changes);
};

}

#endif /* !CONFIGSTORE_PLUGIN_MANAGER_HH */
