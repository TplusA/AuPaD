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

#ifndef DEBUG_DBUS_HH
#define DEBUG_DBUS_HH

#include "dbus/debug_dbus.h"
#include "dbus/taddybus.hh"

namespace TDBus
{

// D-Bus interface: de.tahifi.Debug.Logging
template <>
struct IfaceTraits<tdbusdebugLogging>
{
    static tdbusdebugLogging *skeleton_new() { return tdbus_debug_logging_skeleton_new(); }
};

struct DebugLoggingDebugLevel;
template <>
struct MethodHandlerTraits<DebugLoggingDebugLevel>
{
  private:
    using ThisMethod = DebugLoggingDebugLevel;

  public:
    using IfaceType = tdbusdebugLogging;

    static const char *dbus_method_name() { return "DebugLevel"; }
    static const char *glib_signal_name() { return "handle-debug-level"; }

    static gboolean handler(IfaceType *const object,
                            GDBusMethodInvocation *const invocation,
                            const gchar *const arg_new_level,
                            TDBus::Iface<IfaceType> *const iface);

    static void complete(IfaceType *object, GDBusMethodInvocation *invocation,
                         const char *const name)
    {
        tdbus_debug_logging_complete_debug_level(object, invocation, name);
    }
};

// D-Bus interface: de.tahifi.Debug.LoggingConfig
template <>
struct IfaceTraits<tdbusdebugLoggingConfig>
{
    static tdbusdebugLoggingConfig *skeleton_new() { return tdbus_debug_logging_config_skeleton_new(); }
};

template <>
struct ProxyTraits<tdbusdebugLoggingConfig>
{
    static ProxyBase::ProxyNewFunction
    proxy_new_fn() { return tdbus_debug_logging_config_proxy_new; }

    static ProxyBase::ProxyNewFinishFunction<tdbusdebugLoggingConfig>
    proxy_new_finish_fn() { return tdbus_debug_logging_config_proxy_new_finish; }
};

struct DebugLoggingConfigGlobalDebugLevelChanged;
template <>
struct SignalHandlerTraits<DebugLoggingConfigGlobalDebugLevelChanged>
{
    using IfaceType = tdbusdebugLoggingConfig;

    static const char *dbus_signal_name() { return "GlobalDebugLevelChanged"; }
    static const char *glib_signal_name() { return "global-debug-level-changed"; }

    static void handler(IfaceType *const object,
                        const gchar *const new_level_name,
                        TDBus::Proxy<IfaceType> *const proxy);
};

}

#endif /* !DEBUG_DBUS_HH */
