/*
 * Copyright (C) 2019, 2020, 2023  T+A elektroakustik GmbH & Co. KG
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

#include "monitor_manager.hh"
#include "dbus/de_tahifi_aupad.hh"

static gboolean dbushandler_register_client(
        tdbusaupadMonitor *const object,
        GDBusMethodInvocation *const invocation,
        TDBus::MethodHandlerTraits<TDBus::AuPaDMonitorRegister>:: template UserData<
            ClientPlugin::MonitorManager &
        > *const d)
{
    try
    {
        if(!std::get<0>(d->user_data).register_client(
                g_dbus_method_invocation_get_object_path(invocation),
                g_dbus_method_invocation_get_connection(invocation),
                g_dbus_method_invocation_get_sender(invocation)))
            msg_info("D-Bus client %s tried to register again on %s",
                     g_dbus_method_invocation_get_sender(invocation),
                     g_dbus_method_invocation_get_object_path(invocation));
    }
    catch(...)
    {
        MSG_BUG("Callback %s() for unknown object path %s",
                __func__, g_dbus_method_invocation_get_object_path(invocation));
        d->iface.method_fail(invocation, "Registration locked");
        return TRUE;
    }

    d->done(invocation);
    return TRUE;
}

static gboolean dbushandler_unregister_client(
        tdbusaupadMonitor *const object,
        GDBusMethodInvocation *const invocation,
        TDBus::MethodHandlerTraits<TDBus::AuPaDMonitorUnregister>:: template UserData<
            ClientPlugin::MonitorManager &
        > *const d)
{
    try
    {
        if(!std::get<0>(d->user_data).unregister_client(
                g_dbus_method_invocation_get_object_path(invocation),
                g_dbus_method_invocation_get_sender(invocation)))
            msg_info("Unregistered D-Bus client %s tried to unregister on %s",
                     g_dbus_method_invocation_get_sender(invocation),
                     g_dbus_method_invocation_get_object_path(invocation));
    }
    catch(...)
    {
        MSG_BUG("Callback %s() for unknown object path %s",
                __func__, g_dbus_method_invocation_get_object_path(invocation));
        d->iface.method_fail(invocation, "Unregistration locked");
    }

    d->done(invocation);
    return TRUE;
}

void ClientPlugin::MonitorManager::mk_registration_interface(
        const char *object_path, Plugin &plugin)
{
    if(plugins_.find(object_path) != plugins_.end())
    {
        MSG_BUG("Monitor registration interface already created on %s", object_path);
        return;
    }

    auto iface = std::make_unique<TDBus::Iface<tdbusaupadMonitor>>(object_path);

    iface->connect_method_handler<TDBus::AuPaDMonitorRegister>(dbushandler_register_client, *this);
    iface->connect_method_handler<TDBus::AuPaDMonitorUnregister>(dbushandler_unregister_client, *this);
    bus_.add_auto_exported_interface(*iface);

    plugins_.emplace(object_path,
                     std::make_tuple(&plugin, std::set<std::string>(),
                                     std::move(iface)));
}

bool ClientPlugin::MonitorManager::register_client(
        const char *object_path, GDBusConnection *connection,
        const char *client)
{
    auto &entry = plugins_.at(object_path);

    if(!std::get<1>(entry).insert(client).second)
        return false;

    std::get<0>(entry)->add_client();

    msg_info("Client %s registered on monitor interface on %s",
             client, object_path);

    auto it(client_watchers_.find(client));
    if(it == client_watchers_.end())
    {
        auto watcher(std::make_unique<TDBus::PeerWatcher>(client, nullptr,
            [this]
            (GDBusConnection *, const char *name)
            {
                for(auto &p : plugins_)
                    if(std::get<1>(p.second).erase(name) > 0)
                        msg_info("Client %s removed from monitor interface on %s",
                                 name, std::get<0>(p.second)->name_.c_str());

                client_watchers_.erase(name);
            }));

        watcher->start(connection);
        client_watchers_.emplace(client, std::make_pair(std::move(watcher), 1));
    }
    else
        ++it->second.second;

    return true;
}

bool ClientPlugin::MonitorManager::unregister_client(const char *object_path,
                                                     const char *client)
{
    auto &entry = plugins_.at(object_path);

    if(std::get<1>(entry).erase(client) == 0)
        return false;

    std::get<0>(entry)->remove_client();

    msg_info("Client %s unregistered from monitor interface on %s",
             client, object_path);

    auto it(client_watchers_.find(client));
    if(it != client_watchers_.end() && --it->second.second == 0)
        if(client_watchers_.erase(client) > 0)
            msg_info("Removed D-Bus watcher for client %s", client);

    return true;
}
