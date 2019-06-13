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

#include "configstore.hh"
#include "client_plugin_manager.hh"
#include "device_models.hh"
#include "messages.h"

void ClientPlugin::PluginManager::register_plugin(std::unique_ptr<Plugin> plugin)
{
    log_assert(plugin != nullptr);
    plugins_.emplace_back(std::move(plugin));
    plugins_.back()->registered();
}

void ClientPlugin::PluginManager::shutdown() noexcept
{
    auto plugins(std::move(plugins_));

    for(auto &p : plugins)
    {
        try
        {
            p->unregistered();
        }
        catch(...)
        {
            /* ignore any exceptions thrown by plugins */
            BUG("Exception from plugin \"%s\" in shutdown", p->name_.c_str());
        }
    }
}

void ClientPlugin::PluginManager::report_changes(const ConfigStore::Settings &settings,
                                                 const ConfigStore::Changes &changes) const
{
    // cppcheck-suppress accessMoved
    for(const auto &p : plugins_)
        if(p->has_clients())
            p->report_changes(settings, changes);
}
