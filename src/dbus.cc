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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "dbus.hh"
#include "dbus/de_tahifi_jsonio.hh"
#include "dbus/de_tahifi_debug.hh"

static void handle_audio_path_request(const char *json)
{
    /* TODO: implementation */
}

static gboolean audio_path_change_request(
        tdbusJSONReceiver *const object,
        GDBusMethodInvocation *const invocation,
        const gchar *const json,
        const gchar *const *const extra,
        TDBus::MethodHandlerTraits<TDBus::JSONReceiverTell>::template UserData<> *const d)
{
    std::string answer;

    try
    {
        handle_audio_path_request(json);
        answer = "{}";
    }
    catch(const std::exception &e)
    {
        answer = "{\"error\":\"exception\",\"message\":\"";
        answer += e.what();
        answer += "\"}";
    }

    const char *const empty_extra[] = {nullptr};
    d->done(invocation, answer.c_str(), empty_extra);
    return TRUE;
}

static gboolean audio_path_change_request_ignore_errors(
        tdbusJSONReceiver *const object,
        GDBusMethodInvocation *const invocation,
        const gchar *const json,
        const gchar *const *const extra,
        TDBus::MethodHandlerTraits<TDBus::JSONReceiverNotify>::template UserData<> *const d)
{
    try
    {
        handle_audio_path_request(json);
    }
    catch(...)
    {
        /* cannot do anything about it */
    }

    d->done(invocation);
    return TRUE;
}

/*
 * Logging levels, directly on /de/tahifi/AuPaD and from DCPD via signal.
 */
static void debugging_and_logging(TDBus::Bus &bus)
{
    static TDBus::Iface<tdbusdebugLogging> logging_iface("/de/tahifi/AuPaD");
    logging_iface.connect_method_handler_simple<TDBus::DebugLoggingDebugLevel>();
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
                        proxy.connect_signal_handler_simple<TDBus::DebugLoggingConfigGlobalDebugLevelChanged>();
                });
        },
        [] (GDBusConnection *connection, const char *name)
        {
            msg_vinfo(MESSAGE_LEVEL_DEBUG, "Lost DCPD (debugging)");
        });
}

/*
 * Export interface for audio path requests sent by external processes that we
 * must process and forward to DCPD.
 */
static void change_requests(TDBus::Bus &bus)
{
    static TDBus::Iface<tdbusJSONReceiver> requests_iface("/de/tahifi/AuPaD/Request");
    requests_iface.connect_method_handler<TDBus::JSONReceiverTell>(audio_path_change_request);
    requests_iface.connect_method_handler<TDBus::JSONReceiverNotify>(audio_path_change_request_ignore_errors);
    bus.add_auto_exported_interface(requests_iface);
}

void TDBus::setup(TDBus::Bus &bus)
{
    debugging_and_logging(bus);
    change_requests(bus);

    bus.connect(
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

TDBus::Bus &TDBus::session_bus()
{
    static TDBus::Bus bus("de.tahifi.AuPaD", TDBus::Bus::Type::SESSION);
    return bus;
}

TDBus::Bus &TDBus::system_bus()
{
    static TDBus::Bus bus("de.tahifi.AuPaD", TDBus::Bus::Type::SYSTEM);
    return bus;
}
