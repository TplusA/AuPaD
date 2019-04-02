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

ACLOCAL_AMFLAGS = -I ../m4

bin_PROGRAMS = aupad

aupad_SOURCES = aupad.cc

DBUS_IFACES = $(top_srcdir)/dbus_interfaces

AM_CFLAGS = $(CWARNINGS)

AM_CXXFLAGS = $(CXXWARNINGS)

AM_CPPFLAGS = $(AUPAD_DEPENDENCIES_CFLAGS)
AM_CPPFLAGS += -I$(DBUS_IFACES)

noinst_LTLIBRARIES = \
    libjsonio_dbus.la \
    libdebug_dbus.la

aupad_LDADD = $(noinst_LTLIBRARIES) $(AUPAD_DEPENDENCIES_LIBS)

nodist_libjsonio_dbus_la_SOURCES = jsonio_dbus.c jsonio_dbus.h
libjsonio_dbus_la_CFLAGS = $(CRELAXEDWARNINGS)

nodist_libdebug_dbus_la_SOURCES = debug_dbus.c debug_dbus.h
libdebug_dbus_la_CFLAGS = $(CRELAXEDWARNINGS)

BUILT_SOURCES = \
    $(nodist_libjsonio_dbus_la_SOURCES) \
    de_tahifi_jsonio.h \
    de_tahifi_jsonio.md \
    jsonio_dbus.stamp \
    $(nodist_libdebug_dbus_la_SOURCES) \
    de_tahifi_debug.h \
    de_tahifi_debug.md \
    debug_dbus.stamp

CLEANFILES = $(BUILT_SOURCES)

de_tahifi_jsonio.h: jsonio_dbus.stamp
de_tahifi_jsonio.md: jsonio_dbus.stamp
jsonio_dbus.c: jsonio_dbus.stamp
jsonio_dbus.h: jsonio_dbus.stamp
jsonio_dbus.stamp: $(DBUS_IFACES)/de_tahifi_jsonio.xml
	$(GDBUS_CODEGEN) --generate-c-code=jsonio_dbus --c-namespace tdbus --interface-prefix de.tahifi. $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o de_tahifi_jsonio.md -H de_tahifi_jsonio.h -c tdbus -s de.tahifi. -n "$(PACKAGE_NAME) (JSON object exchange)"
	touch $@

de_tahifi_debug.h: debug_dbus.stamp
de_tahifi_debug.md: debug_dbus.stamp
debug_dbus.c: debug_dbus.stamp
debug_dbus.h: debug_dbus.stamp
debug_dbus.stamp: $(DBUS_IFACES)/de_tahifi_debug.xml
	$(GDBUS_CODEGEN) --generate-c-code=debug_dbus --c-namespace tdbus_debug --interface-prefix de.tahifi.Debug. $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o de_tahifi_debug.md -H de_tahifi_debug.h -c tdbus_debug -s de.tahifi.Debug. -n "$(PACKAGE_NAME) (debug levels)"
	touch $@
