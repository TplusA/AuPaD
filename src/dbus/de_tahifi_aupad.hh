/*
 * Copyright (C) 2019, 2020  T+A elektroakustik GmbH & Co. KG
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

#ifndef DE_TAHIFI_AUPAD_HH
#define DE_TAHIFI_AUPAD_HH

#include "dbus/de_tahifi_aupad.h"
#include "dbus/taddybus.hh"

namespace TDBus
{

// D-Bus interface: de.tahifi.AuPaD.Monitor
template <>
struct IfaceTraits<tdbusaupadMonitor>
{
    static tdbusaupadMonitor *skeleton_new() { return tdbus_aupad_monitor_skeleton_new(); }
};

template <>
struct ProxyTraits<tdbusaupadMonitor>
{
    static ProxyBase::ProxyNewFunction
    proxy_new_fn() { return tdbus_aupad_monitor_proxy_new; }

    static ProxyBase::ProxyNewFinishFunction<tdbusaupadMonitor>
    proxy_new_finish_fn() { return tdbus_aupad_monitor_proxy_new_finish; }
};


struct AuPaDMonitorRegister;

template <>
struct MethodHandlerTraits<AuPaDMonitorRegister>
{
  private:
    using ThisMethod = AuPaDMonitorRegister;

  public:
    using IfaceType = tdbusaupadMonitor;

    static const char *glib_signal_name() { return "handle-register"; }

    template <typename... UserDataT>
    using UserData = Iface<IfaceType>::MethodHandlerData<ThisMethod, UserDataT...>;

    template <typename... UserDataT>
    using HandlerType =
        gboolean(IfaceType *const object,
                 GDBusMethodInvocation *const invocation,
                 UserData<UserDataT...> *const d);

    static gboolean simple_method_handler(
            IfaceType *const object, GDBusMethodInvocation *const invocation,
            Iface<IfaceType> *const iface);

    static constexpr auto complete = tdbus_aupad_monitor_complete_register;
};

template <>
struct MethodCallerTraits<AuPaDMonitorRegister>
{
  public:
    using IfaceType = tdbusaupadMonitor;
    static constexpr auto invoke = tdbus_aupad_monitor_call_register;
    static constexpr auto finish = tdbus_aupad_monitor_call_register_finish;
};


struct AuPaDMonitorUnregister;

template <>
struct MethodHandlerTraits<AuPaDMonitorUnregister>
{
  private:
    using ThisMethod = AuPaDMonitorUnregister;

  public:
    using IfaceType = tdbusaupadMonitor;

    static const char *glib_signal_name() { return "handle-unregister"; }

    template <typename... UserDataT>
    using UserData = Iface<IfaceType>::MethodHandlerData<ThisMethod, UserDataT...>;

    template <typename... UserDataT>
    using HandlerType =
        gboolean(IfaceType *const object,
                 GDBusMethodInvocation *const invocation,
                 UserData<UserDataT...> *const d);

    static gboolean simple_method_handler(
            IfaceType *const object, GDBusMethodInvocation *const invocation,
            Iface<IfaceType> *const iface);

    static constexpr auto complete = tdbus_aupad_monitor_complete_unregister;
};

template <>
struct MethodCallerTraits<AuPaDMonitorUnregister>
{
  public:
    using IfaceType = tdbusaupadMonitor;
    static constexpr auto invoke = tdbus_aupad_monitor_call_unregister;
    static constexpr auto finish = tdbus_aupad_monitor_call_unregister_finish;
};

}

#endif /* !DE_TAHIFI_AUPAD_HH */
