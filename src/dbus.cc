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

#include "dbus.hh"
#include "dbus/jsonio_dbus.hh"
#include "dbus/debug_dbus.hh"

static void handle_audio_path_update(const char *json)
{
    /* TODO: implementation */
}

namespace TDBus
{

gboolean MethodHandlerTraits<JSONReceiverTell>::handler(
            IfaceType *const object, GDBusMethodInvocation *const invocation,
            const gchar *const json, const gchar *const *const extra,
            Iface<IfaceType> *const iface)
{
    std::string answer;

    try
    {
        handle_audio_path_update(json);
        answer = "{}";
    }
    catch(const std::exception &e)
    {
        answer = "{\"error\":\"exception\",\"message\":\"";
        answer += e.what();
        answer += "\"}";
    }

    const char *const empty_extra[] = {nullptr};
    iface->method_done<ThisMethod>(invocation, answer.c_str(), empty_extra);
    return TRUE;
}

gboolean MethodHandlerTraits<JSONReceiverNotify>::handler(
            IfaceType *const object, GDBusMethodInvocation *const invocation,
            const gchar *const json, const gchar *const *const extra,
            Iface<IfaceType> *const iface)
{
    try
    {
        handle_audio_path_update(json);
    }
    catch(...)
    {
        /* cannot do anything about it */
    }

    iface->method_done<ThisMethod>(invocation);
    return TRUE;
}

gboolean MethodHandlerTraits<JSONEmitterGet>::handler(
            IfaceType *const object, GDBusMethodInvocation *const invocation,
            const gchar *const *const params,
            Iface<IfaceType> *const iface)
{
    iface->sanitize(object, invocation);

    const auto &object_path(iface->get_object_path());

    if(object_path == "/de/tahifi/AuPaD/Roon")
    {
        const char *const empty_extra[] = {nullptr};
        iface->method_done<ThisMethod>(invocation, "{}", empty_extra);
    }
    else
    {
        BUG("Unhandled object path \"%s\"", object_path.c_str());
        iface->method_fail(invocation, "Unhandled object path");
    }

    return TRUE;
}

void SignalHandlerTraits<JSONEmitterObject>::handler(
        IfaceType *const object,
        const gchar *const json, const gchar *const *const extra,
        Proxy<IfaceType> *const proxy)
{
}

}

/*
 * Logging levels, directly on /de/tahifi/AuPaD and from DCPD via signal.
 */
static void debugging_and_logging(TDBus::Bus &bus)
{
    static TDBus::Iface<tdbusdebugLogging> logging_iface("/de/tahifi/AuPaD");
    logging_iface.connect_method_handler<TDBus::DebugLoggingDebugLevel>();
    bus.add_auto_exported_interface(logging_iface);

    static TDBus::Proxy<tdbusdebugLoggingConfig>
    logging_config_proxy("de.tahifi.Dcpd", "/de/tahifi/Dcpd");

    bus.add_watcher("de.tahifi.Dcpd",
        [] (GDBusConnection *connection, const char *name)
        {
            msg_vinfo(MESSAGE_LEVEL_DEBUG, "Connecting to DCPD (debugging)");
            logging_config_proxy.connect_proxy(connection,
                [] (TDBus::Proxy<tdbusdebugLoggingConfig> &proxy, bool succeeded)
                {
                    if(succeeded)
                        proxy.connect_signal_handler<TDBus::DebugLoggingConfigGlobalDebugLevelChanged>();
                });
        },
        [] (GDBusConnection *connection, const char *name)
        {
            msg_vinfo(MESSAGE_LEVEL_DEBUG, "Lost DCPD (debugging)");
        });
}

/*
 * Connections to DCPD: listen to audio path updates sent by DCPD (D-Bus
 * signals that we receive and process) and get object that we can send update
 * requests and other requests to (D-Bus methods sent by us)
 */
static void audio_path_updates(TDBus::Bus &bus)
{
    static TDBus::Proxy<tdbusJSONReceiver>
    requests_for_dcpd_proxy("de.tahifi.Dcpd", "/de/tahifi/Dcpd/AudioPaths");
    static TDBus::Proxy<tdbusJSONEmitter>
    updates_from_dcpd_proxy("de.tahifi.Dcpd", "/de/tahifi/Dcpd/AudioPaths");

    bus.add_watcher("de.tahifi.Dcpd",
        [] (GDBusConnection *connection, const char *name)
        {
            msg_vinfo(MESSAGE_LEVEL_DEBUG, "Connecting to DCPD (audio paths)");
            requests_for_dcpd_proxy.connect_proxy(connection);
            updates_from_dcpd_proxy.connect_proxy(connection,
                [] (TDBus::Proxy<tdbusJSONEmitter> &proxy, bool succeeded)
                {
                    if(succeeded)
                    {
                        proxy.connect_signal_handler<TDBus::JSONEmitterObject>();
                        msg_vinfo(MESSAGE_LEVEL_DEBUG,
                                  "Connected to DCPD AuPaL emitter");
                    }
                    else
                        msg_error(0, LOG_NOTICE,
                                  "Failed connecting to DCPD AuPaL emitter");
                });
        },
        [] (GDBusConnection *connection, const char *name)
        {
            msg_vinfo(MESSAGE_LEVEL_DEBUG, "Lost DCPD (audio paths)");
        });

}

/*
 * Export interface for audio path requests sent by external processes that we
 * must process and forward to DCPD.
 */
static void change_requests(TDBus::Bus &bus)
{
    static TDBus::Iface<tdbusJSONReceiver> requests_iface("/de/tahifi/AuPaD/Request");
    requests_iface.connect_method_handler<TDBus::JSONReceiverTell>();
    requests_iface.connect_method_handler<TDBus::JSONReceiverNotify>();
    bus.add_auto_exported_interface(requests_iface);
}

/*
 * Setup stuff for Roon.
 */
static void plugin_roon(TDBus::Bus &bus)
{
    static constexpr char object_name[] = "/de/tahifi/AuPaD/Roon";

    static TDBus::Iface<tdbusJSONReceiver> command_iface(object_name);
    command_iface.connect_method_handler<TDBus::JSONReceiverTell>();
    bus.add_auto_exported_interface(command_iface);

    static TDBus::Iface<tdbusJSONEmitter> emitter_iface(object_name);
    bus.add_auto_exported_interface(emitter_iface);

    bus.add_watcher("de.tahifi.Roon",
        [] (GDBusConnection *connection, const char *name)
        {
            msg_vinfo(MESSAGE_LEVEL_DEBUG, "TARoon is running");
        },
        [] (GDBusConnection *connection, const char *name)
        {
            msg_vinfo(MESSAGE_LEVEL_DEBUG, "TARoon is not running");
        });
}

void TDBus::setup()
{
    static TDBus::Bus session_bus("de.tahifi.AuPaD", TDBus::Bus::Type::SESSION);
    static TDBus::Bus system_bus("de.tahifi.AuPaD", TDBus::Bus::Type::SYSTEM);

    debugging_and_logging(session_bus);
    audio_path_updates(session_bus);
    change_requests(session_bus);
    plugin_roon(session_bus);

    session_bus.connect(
        [] (GDBusConnection *connection)
        {
            msg_info("Session bus: Connected");
        },
        [] (GDBusConnection *)
        {
            msg_info("Session bus: Name acquired");
        },
        [] (GDBusConnection *)
        {
            msg_info("Session bus: Name lost");
        });
}
