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

#ifndef JSONIO_DBUS_HH
#define JSONIO_DBUS_HH

#include "dbus/jsonio_dbus.h"
#include "dbus/taddybus.hh"

namespace TDBus
{

// D-Bus interface: de.tahifi.JSONReceiver
template <>
struct IfaceTraits<tdbusJSONReceiver>
{
    static tdbusJSONReceiver *skeleton_new() { return tdbus_jsonreceiver_skeleton_new(); }
};

template <>
struct ProxyTraits<tdbusJSONReceiver>
{
    static ProxyBase::ProxyNewFunction
    proxy_new_fn() { return tdbus_jsonreceiver_proxy_new; }

    static ProxyBase::ProxyNewFinishFunction<tdbusJSONReceiver>
    proxy_new_finish_fn() { return tdbus_jsonreceiver_proxy_new_finish; }
};


struct JSONReceiverNotify;

template <>
struct MethodHandlerTraits<JSONReceiverNotify>
{
  private:
    using ThisMethod = JSONReceiverNotify;

  public:
    using IfaceType = tdbusJSONReceiver;

    static const char *glib_signal_name() { return "handle-notify"; }

    template <typename... UserDataT>
    using UserData = Iface<IfaceType>::MethodHandlerData<ThisMethod, UserDataT...>;

    template <typename... UserDataT>
    using HandlerType =
        gboolean(IfaceType *const object,
                 GDBusMethodInvocation *const invocation,
                 const gchar *const json,
                 const gchar *const *const extra,
                 UserData<UserDataT...> *const d);

    static gboolean simple_method_handler(
            IfaceType *const object, GDBusMethodInvocation *const invocation,
            const gchar *const json,
            const gchar *const *const extra,
            Iface<IfaceType> *const iface);

    static constexpr auto complete = tdbus_jsonreceiver_complete_notify;
};

template <>
struct MethodCallerTraits<JSONReceiverNotify>
{
  public:
    using IfaceType = tdbusJSONReceiver;
    static constexpr auto invoke = tdbus_jsonreceiver_call_notify;
    static constexpr auto finish = tdbus_jsonreceiver_call_notify_finish;
};


struct JSONReceiverTell;

template <>
struct MethodHandlerTraits<JSONReceiverTell>
{
  private:
    using ThisMethod = JSONReceiverTell;

  public:
    using IfaceType = tdbusJSONReceiver;

    static const char *glib_signal_name() { return "handle-tell"; }

    template <typename... UserDataT>
    using UserData = Iface<IfaceType>::MethodHandlerData<ThisMethod, UserDataT...>;

    template <typename... UserDataT>
    using HandlerType =
        gboolean(IfaceType *const object,
                 GDBusMethodInvocation *const invocation,
                 const gchar *const json,
                 const gchar *const *const extra,
                 UserData<UserDataT...> *const d);

    static gboolean simple_method_handler(
            IfaceType *const object, GDBusMethodInvocation *const invocation,
            const gchar *const json,
            const gchar *const *const extra,
            Iface<IfaceType> *const iface);

    static constexpr auto complete = tdbus_jsonreceiver_complete_tell;
};

template <>
struct MethodCallerTraits<JSONReceiverTell>
{
  public:
    using IfaceType = tdbusJSONReceiver;
    static constexpr auto invoke = tdbus_jsonreceiver_call_tell;
    static constexpr auto finish = tdbus_jsonreceiver_call_tell_finish;
};


// D-Bus interface: de.tahifi.JSONEmitter
template <>
struct IfaceTraits<tdbusJSONEmitter>
{
    static tdbusJSONEmitter *skeleton_new() { return tdbus_jsonemitter_skeleton_new(); }
};

template <>
struct ProxyTraits<tdbusJSONEmitter>
{
    static ProxyBase::ProxyNewFunction
    proxy_new_fn() { return tdbus_jsonemitter_proxy_new; }

    static ProxyBase::ProxyNewFinishFunction<tdbusJSONEmitter>
    proxy_new_finish_fn() { return tdbus_jsonemitter_proxy_new_finish; }
};


struct JSONEmitterGet;

template <>
struct MethodHandlerTraits<JSONEmitterGet>
{
  private:
    using ThisMethod = JSONEmitterGet;

  public:
    using IfaceType = tdbusJSONEmitter;

    static const char *glib_signal_name() { return "handle-get"; }

    template <typename... UserDataT>
    using UserData = Iface<IfaceType>::MethodHandlerData<ThisMethod, UserDataT...>;

    template <typename... UserDataT>
    using HandlerType =
        gboolean(IfaceType *const object,
                 GDBusMethodInvocation *const invocation,
                 const gchar *const *const params,
                 UserData<UserDataT...> *const d);

    static gboolean simple_method_handler(
            IfaceType *const object, GDBusMethodInvocation *const invocation,
            const gchar *const *const params,
            Iface<IfaceType> *const iface);

    static constexpr auto complete = tdbus_jsonemitter_complete_get;
};

template <>
struct MethodCallerTraits<JSONEmitterGet>
{
  public:
    using IfaceType = tdbusJSONEmitter;
    static constexpr auto invoke = tdbus_jsonemitter_call_get;
    static constexpr auto finish = tdbus_jsonemitter_proxy_new_finish;
};


struct JSONEmitterObject;

template <>
struct SignalHandlerTraits<JSONEmitterObject>
{
  private:
    using ThisSignal = JSONEmitterObject;

  public:
    using IfaceType = tdbusJSONEmitter;

    static const char *glib_signal_name() { return "object"; }

    template <typename... UserDataT>
    using UserData = Proxy<IfaceType>::SignalHandlerData<ThisSignal, UserDataT...>;

    template <typename... UserDataT>
    using HandlerType =
        void(IfaceType *const object,
             const gchar *json, const gchar *const *const extra,
             UserData<UserDataT...> *const d);

    static void simple_signal_handler(
            IfaceType *const object,
            const gchar *json, const gchar *const *const extra,
            Proxy<IfaceType> *const proxy);
};

}

#endif /* !JSONIO_DBUS_HH */
