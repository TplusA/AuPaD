#
# Copyright (C) 2020  T+A elektroakustik GmbH & Co. KG
#
# This file is part of AuPaD.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA  02110-1301, USA.
#

config_h = declare_dependency(
    sources: configure_file(input: '../config.h.meson',
                            output: 'config.h',
                            configuration: config_data),
    include_directories: '.')

subdir('dbus')

configstore_lib = static_library('configstore',
    ['configstore.cc', 'client_plugin.cc', 'device_models.cc'],
    dependencies: config_h
)

configstore_roon_lib = static_library('configstore_roon',
    'report_roon.cc', dependencies: config_h
)

sigpath_lib = static_library('sigpath',
    'signal_path_tracker.cc', dependencies: config_h
)

executable(
    'aupad',
    [
        'aupad.cc', 'dbus.cc', 'debug_levels.cc', 'monitor_manager.cc',
        'backtrace.c', 'messages.c', 'messages_glib.c', 'os.c',
        version_info,
    ],
    dependencies: [dbus_deps, glib_deps, config_h],
    link_with: [
        configstore_lib, configstore_roon_lib, sigpath_lib,
        taddybus_lib,
    ],
    install: true
)
