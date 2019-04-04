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

#include "dbus/jsonio_dbus.hh"
#include "dbus/debug_dbus.hh"
#include "messages.h"
#include "messages_glib.h"
#include "versioninfo.h"

#include <iostream>
#include <cstring>

static void show_version_info(void)
{
    printf("%s\n"
           "Revision %s%s\n"
           "         %s+%d, %s\n",
           PACKAGE_STRING,
           VCS_FULL_HASH, VCS_WC_MODIFIED ? " (tainted)" : "",
           VCS_TAG, VCS_TICK, VCS_DATE);
}

static void log_version_info(void)
{
    msg_vinfo(MESSAGE_LEVEL_IMPORTANT, "Rev %s%s, %s+%d, %s",
              VCS_FULL_HASH, VCS_WC_MODIFIED ? " (tainted)" : "",
              VCS_TAG, VCS_TICK, VCS_DATE);
}

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

struct Parameters
{
    bool run_in_foreground_;
    MessageVerboseLevel verbose_level_;

    Parameters(const Parameters &) = delete;
    Parameters(Parameters &&) = default;
    Parameters &operator=(const Parameters &) = delete;
    Parameters &operator=(Parameters &&) = default;

    explicit Parameters():
        run_in_foreground_(false),
        verbose_level_(MESSAGE_LEVEL_NORMAL)
    {}
};

static void usage(const char *program_name)
{
    std::cout <<
        "Usage: " << program_name << " [options]\n"
        "\n"
        "Options:\n"
        "  --help         Show this help.\n"
        "  --version      Print version information to stdout.\n"
        "  --verbose lvl  Set verbosity level to given level.\n"
        "  --quiet        Short for \"--verbose quite\".\n"
        "  --fg           Run in foreground, don't run as daemon.\n"
        ;
}

static bool check_argument(int argc, char *argv[], int &i)
{
    if(i + 1 >= argc)
    {
        std::cerr << "Option " << argv[i] << " requires an argument.\n";
        return false;
    }

    ++i;

    return true;
}

static int process_command_line(int argc, char *argv[],
                                Parameters &parameters)
{
    for(int i = 1; i < argc; ++i)
    {
        if(strcmp(argv[i], "--help") == 0)
            return 1;
        else if(strcmp(argv[i], "--version") == 0)
            return 2;
        else if(strcmp(argv[i], "--fg") == 0)
            parameters.run_in_foreground_ = true;
        else if(strcmp(argv[i], "--verbose") == 0)
        {
            if(!check_argument(argc, argv, i))
                return -1;

            parameters.verbose_level_ = msg_verbose_level_name_to_level(argv[i]);

            if(parameters.verbose_level_ == MESSAGE_LEVEL_IMPOSSIBLE)
            {
                std::cerr << "Invalid verbosity \"" << argv[i] << "\". "
                        "Valid verbosity levels are:\n";

                const char *const *names = msg_get_verbose_level_names();

                for(const char *name = *names; name != NULL; name = *++names)
                    fprintf(stderr, "    %s\n", name);

                return -1;
            }
        }
        else if(strcmp(argv[i], "--quiet") == 0)
            parameters.verbose_level_ = MESSAGE_LEVEL_QUIET;
        else
        {
            std::cerr << "Unknown option \"" << argv[i]
                      << "\". Please try --help.\n";
            return -1;
        }
    }

    return 0;
}

/*!
 * Set up logging, daemonize.
 */
static bool setup(const Parameters &parameters)
{
    msg_enable_syslog(!parameters.run_in_foreground_);
    msg_enable_glib_message_redirection();
    msg_set_verbose_level(parameters.verbose_level_);

    if(!parameters.run_in_foreground_)
    {
        openlog("aupad", LOG_PID, LOG_DAEMON);

        if(daemon(0, 0) < 0)
        {
            msg_error(errno, LOG_EMERG, "Failed to run as daemon");
            return false;
        }
    }

    log_version_info();

    return true;
}

int main(int argc, char *argv[])
{
    static Parameters parameters;
    const int ret = process_command_line(argc, argv, parameters);

    if(ret == -1)
        return EXIT_FAILURE;
    else if(ret == 1)
    {
        usage(argv[0]);
        return EXIT_SUCCESS;
    }
    else if(ret == 2)
    {
        show_version_info();
        return EXIT_SUCCESS;
    }

    if(!setup(parameters))
        return EXIT_FAILURE;

    static TDBus::Iface<tdbusdebugLogging> logging_iface("/de/tahifi/AuPaD");
    logging_iface.connect_method_handler<TDBus::DebugLoggingDebugLevel>();
    static TDBus::Proxy<tdbusdebugLoggingConfig> logging_config_proxy("de.tahifi.Dcpd", "/de/tahifi/Dcpd");

    static TDBus::Iface<tdbusJSONReceiver> receiver_iface("/de/tahifi/AuPaD");
    static TDBus::Iface<tdbusJSONEmitter> roon_emitter_iface("/de/tahifi/AuPaD/Roon");
    static TDBus::Proxy<tdbusJSONEmitter> dcpd_emitter_iface("de.tahifi.Dcpd", "/de/tahifi/Dcpd");

    receiver_iface.connect_method_handler<TDBus::JSONReceiverTell>();
    receiver_iface.connect_method_handler<TDBus::JSONReceiverNotify>();
    roon_emitter_iface.connect_method_handler<TDBus::JSONEmitterGet>();

    static TDBus::Bus session_bus("de.tahifi.AuPaD", TDBus::Bus::Type::SESSION);
    static TDBus::Bus system_bus("de.tahifi.AuPaD", TDBus::Bus::Type::SYSTEM);

    session_bus.connect(
        [] (GDBusConnection *connection)
        {
            msg_info("Session bus: Connected, exporting interfaces");
            logging_iface.export_interface(connection);
            receiver_iface.export_interface(connection);
            roon_emitter_iface.export_interface(connection);
        },
        [] (GDBusConnection *)
        {
            msg_info("Session bus: Name acquired");
        },
        [] (GDBusConnection *)
        {
            msg_info("Session bus: Name lost");
        });

    session_bus.add_watcher("de.tahifi.Roon",
        [] (GDBusConnection *connection, const char *name)
        {
        },
        [] (GDBusConnection *connection, const char *name)
        {
        });

    session_bus.add_watcher("de.tahifi.Dcpd",
        [] (GDBusConnection *connection, const char *name)
        {
            logging_config_proxy.connect_proxy(connection,
                [] (TDBus::Proxy<tdbusdebugLoggingConfig> &proxy, bool succeeded)
                {
                    if(succeeded)
                        proxy.connect_signal_handler<TDBus::DebugLoggingConfigGlobalDebugLevelChanged>();
                });

            dcpd_emitter_iface.connect_proxy(connection,
                [] (TDBus::Proxy<tdbusJSONEmitter> &proxy, bool succeeded)
                {
                    if(succeeded)
                        proxy.connect_signal_handler<TDBus::JSONEmitterObject>();
                });
        },
        [] (GDBusConnection *connection, const char *name)
        {
        });

    auto *loop = g_main_loop_new(nullptr, false);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    return 0;
}

ssize_t (*os_read)(int fd, void *dest, size_t count) = read;
ssize_t (*os_write)(int fd, const void *buf, size_t count) = write;
