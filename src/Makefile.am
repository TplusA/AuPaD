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
    aupad.cc debug_levels.cc \
    messages.h messages.c messages_glib.h messages_glib.c os.h os.c

aupad_LDADD = $(AUPAD_DEPENDENCIES_LIBS)

LIBS += \
    dbus/libtaddybus.la \
    dbus/libjsonio_dbus.la \
    dbus/libdebug_dbus.la
