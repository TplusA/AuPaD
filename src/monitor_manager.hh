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

#ifndef MONITOR_MANAGER_HH
#define MONITOR_MANAGER_HH

#include "dbus.hh"
#include "dbus/taddybus.hh"
#include "configstore_plugin.hh"

#include <map>
#include <set>
#include <tuple>
#include <memory>

struct _tdbusaupadMonitor;

/*!
 * Management of client registration objects for plugins.
 *
 * This class maps object paths to their plugins. For each plugin, a set of
 * registered client names are stored so that we know exactly which client has
 * registered with our plugins. We need to know this to detect and avoid
 * multiple registrations of same clients with the same plugin.
 *
 * For each client, a D-Bus watcher is maintained. If the client dies, all its
 * occurrences and the watcher are removed. Each watcher is reference-counted
 * so that there is only a single watcher per client.
 */
class MonitorManager
{
  private:
    TDBus::Bus &bus_;

    /*!
     * Mapping of object path to plugin-related data associated with the path.
     */
    std::map<std::string,
             std::tuple<ConfigStore::Plugin *, std::set<std::string>,
                        std::unique_ptr<TDBus::Iface<struct _tdbusaupadMonitor>>>>
    plugins_;

    /*!
     * Mapping of D-Bus client name to its refcounted peer watcher.
     */
    std::map<std::string,
             std::pair<std::unique_ptr<TDBus::PeerWatcher>, size_t>>
    client_watchers_;

  public:
    MonitorManager(const MonitorManager &) = delete;
    MonitorManager(MonitorManager &&) = default;
    MonitorManager &operator=(const MonitorManager &) = delete;
    MonitorManager &operator=(MonitorManager &&) = default;

    explicit MonitorManager(TDBus::Bus &bus):
        bus_(bus)
    {}

    /*!
     * Export the \c de.tahifi.AuPaD.Monitor interface on given path.
     */
    void mk_registration_interface(const char *object_path,
                                   ConfigStore::Plugin &plugin);

    /*!
     * Called in D-Bus context from method handler.
     */
    bool register_client(const char *object_path,
                         GDBusConnection *connection, const char *client);

    /*!
     * Called in D-Bus context from method handler.
     */
    bool unregister_client(const char *object_path, const char *client);
};

#endif /* !MONITOR_MANAGER_HH */
