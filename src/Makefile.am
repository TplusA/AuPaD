#
# Copyright (C) 2019  T+A elektroakustik GmbH & Co. KG
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

SUBDIRS = dbus

ACLOCAL_AMFLAGS = -I ../m4

AM_CFLAGS = $(CWARNINGS)
AM_CXXFLAGS = $(CXXWARNINGS)
AM_CPPFLAGS = $(AUPAD_DEPENDENCIES_CFLAGS)

bin_PROGRAMS = aupad

aupad_SOURCES = \
    aupad.cc \
    configstore.hh device_models.hh json.hh error.hh \
    dbus.hh dbus.cc debug_levels.cc \
    monitor_manager.cc monitor_manager.hh \
    messages.h messages.c messages_glib.h messages_glib.c os.h os.c
aupad_LDADD = $(AUPAD_DEPENDENCIES_LIBS) $(noinst_LTLIBRARIES)

noinst_LTLIBRARIES = \
    libconfigstore_roon.la \
    libconfigstore.la \
    libsigpath.la

libconfigstore_la_SOURCES = \
    configstore.cc configstore.hh configvalue.hh fixpoint.hh \
    client_plugin.cc client_plugin_manager.hh client_plugin.hh \
    configstore_json.hh configstore_iter.hh configstore_changes.hh \
    device_models.cc device_models.hh
libconfigstore_la_CPPFLAGS = $(AM_CPPFLAGS)
libconfigstore_la_CXXFLAGS = $(AM_CXXFLAGS)

libconfigstore_roon_la_SOURCES = \
    report_roon.cc report_roon.hh \
    client_plugin.hh json.hh \
    configstore.hh configvalue.hh fixpoint.hh \
    configstore_json.hh configstore_iter.hh configstore_changes.hh
libconfigstore_roon_la_CPPFLAGS = $(AM_CPPFLAGS)
libconfigstore_roon_la_CXXFLAGS = $(AM_CXXFLAGS)

libsigpath_la_SOURCES = \
    signal_path_tracker.cc signal_path_tracker.hh \
    signal_paths.hh
libsigpath_la_CPPFLAGS = $(AM_CPPFLAGS)
libsigpath_la_CXXFLAGS = $(AM_CXXFLAGS)

LIBS += \
    dbus/libtaddybus.la \
    dbus/libaupad_dbus.la \
    dbus/libjsonio_dbus.la \
    dbus/libdebug_dbus.la
