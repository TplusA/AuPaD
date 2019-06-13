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

#include "client_plugin_manager.hh"
#include "configstore.hh"
#include "configstore_changes.hh"
#include "configstore_json.hh"
#include "device_models.hh"
#include "report_roon.hh"
#include "dbus.hh"
#include "dbus/jsonio_dbus.hh"
#include "monitor_manager.hh"
#include "messages.h"
#include "messages_glib.h"
#include "versioninfo.h"

#include <glib.h>
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

struct Parameters
{
    bool run_in_foreground_;
    MessageVerboseLevel verbose_level_;
    const char *device_models_file_;

    Parameters(const Parameters &) = delete;
    Parameters(Parameters &&) = default;
    Parameters &operator=(const Parameters &) = delete;
    Parameters &operator=(Parameters &&) = default;

    explicit Parameters():
        run_in_foreground_(false),
        verbose_level_(MESSAGE_LEVEL_NORMAL),
        device_models_file_("/var/local/etc/models.json")
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
        "  --config       Path to device definitions configuration file.\n"
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
        else if(strcmp(argv[i], "--config") == 0)
        {
            if(!check_argument(argc, argv, i))
                return -1;

            parameters.device_models_file_ = argv[i];
        }
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

static void process_dcpd_audio_path_update(
        tdbusJSONEmitter *const object,
        const gchar *const json,
        const gchar *const *const extra,
        TDBus::SignalHandlerTraits<TDBus::JSONEmitterObject>::template UserData<
            ClientPlugin::PluginManager &, ConfigStore::Settings &
        > *const d)
{
    auto &settings(std::get<1>(d->user_data));

    try
    {
        msg_info("Received audio path update");
        msg_info("%s", json);
        settings.update(json);
    }
    catch(const std::exception &e)
    {
        APPLIANCE_BUG("Failed processing audio path update: %s", e.what());
        return;
    }

    ConfigStore::Changes changes;
    ConfigStore::SettingsJSON js(settings);

    if(js.extract_changes(changes))
        std::get<0>(d->user_data).report_changes(settings, changes);
}

static void dcpd_appeared(GDBusConnection *connection,
                          TDBus::Proxy<tdbusJSONReceiver> &requests_for_dcpd_proxy,
                          TDBus::Proxy<tdbusJSONEmitter> &updates_from_dcpd_proxy,
                          ClientPlugin::PluginManager &pm,
                          ConfigStore::Settings &settings)
{
    msg_vinfo(MESSAGE_LEVEL_DEBUG, "Connecting to DCPD (audio paths)");

    requests_for_dcpd_proxy.connect_proxy(connection,
        [] (TDBus::Proxy<tdbusJSONReceiver> &proxy, bool succeeded)
        {
            if(!succeeded)
            {
                msg_error(0, LOG_NOTICE,
                          "Failed connecting to DCPD audio path requesting interface");
                return;
            }

            static constexpr char request[] =
                R"({"query": {"what": "full_audio_signal_path"}})";
            const char *const empty_extra[] = {nullptr};
            proxy.call_and_forget<TDBus::JSONReceiverNotify>(request, empty_extra);
            msg_vinfo(MESSAGE_LEVEL_DEBUG,
                      "Connected to DCPD audio path requesting interface");
        });

    updates_from_dcpd_proxy.connect_proxy(connection,
        [&pm, &settings]
        (TDBus::Proxy<tdbusJSONEmitter> &proxy, bool succeeded)
        {
            if(!succeeded)
            {
                msg_error(0, LOG_NOTICE,
                          "Failed connecting to DCPD audio path update emitter");
                return;
            }

            proxy.connect_signal_handler<TDBus::JSONEmitterObject>(
                    process_dcpd_audio_path_update, pm, settings);
            msg_vinfo(MESSAGE_LEVEL_DEBUG,
                      "Connected to DCPD audio path update emitter");
        });
}

/*
 * Connections to DCPD: listen to audio path updates sent by DCPD (D-Bus
 * signals that we receive and process) and get object that we can send update
 * requests and other requests to (D-Bus methods sent by us)
 */
static void listen_to_dcpd_audio_path_updates(TDBus::Bus &bus,
                                              ClientPlugin::PluginManager &pm,
                                              ConfigStore::Settings &settings)
{
    static TDBus::Proxy<tdbusJSONReceiver>
    requests_for_dcpd_proxy("de.tahifi.Dcpd", "/de/tahifi/Dcpd/AudioPaths");
    static TDBus::Proxy<tdbusJSONEmitter>
    updates_from_dcpd_proxy("de.tahifi.Dcpd", "/de/tahifi/Dcpd/AudioPaths");

    bus.add_watcher("de.tahifi.Dcpd",
        [&pm, &settings]
        (GDBusConnection *connection, const char *name)
        {
            dcpd_appeared(connection, requests_for_dcpd_proxy,
                          updates_from_dcpd_proxy, pm, settings);
        },
        [&settings]
        (GDBusConnection *connection, const char *name)
        {
            msg_vinfo(MESSAGE_LEVEL_DEBUG, "Lost DCPD (audio paths)");
            settings.clear();
        });
}

static gboolean get_full_roon_audio_path(
        tdbusJSONEmitter *const object,
        GDBusMethodInvocation *const invocation,
        const gchar *const *const params,
        TDBus::MethodHandlerTraits<TDBus::JSONEmitterGet>::template UserData<
            const ClientPlugin::Roon &, const ConfigStore::Settings &
        > *const d)
{
    static const char *const empty_extra[] = {nullptr};

    std::string report;
    if(std::get<0>(d->user_data).full_report(std::get<1>(d->user_data), report))
        d->done(invocation, report.c_str(), empty_extra);
    else
        d->done(invocation, "[]", empty_extra);

    return TRUE;
}

static void send_audio_signal_path_to_roon(const std::string &asp,
                                           bool is_full_signal_path,
                                           TDBus::Iface<tdbusJSONEmitter> &iface)
{
    const char *const extra[] =
    {
        is_full_signal_path ? "signal_path" : "update",
        nullptr
    };

    iface.emit(tdbus_jsonemitter_emit_object, asp.c_str(), extra);
}

static std::unique_ptr<ClientPlugin::Roon>
create_roon_plugin(TDBus::Bus &bus, ClientPlugin::MonitorManager &mm,
                   const ConfigStore::Settings &settings)
{
    static constexpr char object_name[] = "/de/tahifi/AuPaD/Roon";

    static TDBus::Iface<tdbusJSONReceiver> command_iface(object_name);
    bus.add_auto_exported_interface(command_iface);

    static TDBus::Iface<tdbusJSONEmitter> emitter_iface(object_name);
    auto *work_around_gcc_bug = &emitter_iface;
    auto roon(std::make_unique<ClientPlugin::Roon>(
            [work_around_gcc_bug]
            (const auto &asp, bool is_full_signal_path)
            {
                send_audio_signal_path_to_roon(asp, is_full_signal_path,
                                               *work_around_gcc_bug);
            }));
    emitter_iface.connect_method_handler<TDBus::JSONEmitterGet>(
        get_full_roon_audio_path,
        *const_cast<const ClientPlugin::Roon *>(roon.get()), settings);
    bus.add_auto_exported_interface(emitter_iface);

    mm.mk_registration_interface(object_name, *roon);

    bus.add_watcher("de.tahifi.Roon",
        [] (GDBusConnection *connection, const char *name)
        {
            msg_vinfo(MESSAGE_LEVEL_DEBUG, "TARoon is running");
        },
        [] (GDBusConnection *connection, const char *name)
        {
            msg_vinfo(MESSAGE_LEVEL_DEBUG, "TARoon is not running");
        });

    return roon;
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

    TDBus::setup(TDBus::session_bus());

    static StaticModels::DeviceModelsDatabase models_database;
    models_database.load(parameters.device_models_file_);

    static ConfigStore::Settings settings(models_database);

    ClientPlugin::PluginManager pm;
    ClientPlugin::MonitorManager mm(TDBus::session_bus());
    pm.register_plugin(create_roon_plugin(TDBus::session_bus(), mm, settings));

    listen_to_dcpd_audio_path_updates(TDBus::session_bus(), pm, settings);

    auto *loop = g_main_loop_new(nullptr, false);
    g_main_loop_run(loop);
    g_main_loop_unref(loop);

    return 0;
}

ssize_t (*os_read)(int fd, void *dest, size_t count) = read;
ssize_t (*os_write)(int fd, const void *buf, size_t count) = write;
