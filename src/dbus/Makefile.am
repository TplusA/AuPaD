#
# Copyright (C) 2019, 2020, 2023  T+A elektroakustik GmbH & Co. KG
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

ACLOCAL_AMFLAGS = -I ../../m4

DBUS_IFACES = $(top_srcdir)/dbus_interfaces

noinst_LTLIBRARIES = \
    libtaddybus.la \
    libaupad_dbus.la \
    libjsonio_dbus.la \
    libdebug_dbus.la

AM_CFLAGS = $(CRELAXEDWARNINGS)
AM_CXXFLAGS = $(CXXRELAXEDWARNINGS)
AM_CPPFLAGS = $(AUPAD_DEPENDENCIES_CFLAGS)

libtaddybus_la_SOURCES = taddybus.cc taddybus.hh ../gerrorwrapper.hh
libtaddybus_la_CPPFLAGS = -I$(srcdir)/.. $(AM_CPPFLAGS)

nodist_libaupad_dbus_la_SOURCES = de_tahifi_aupad.c de_tahifi_aupad.h de_tahifi_aupad.hh
libaupad_dbus_la_CPPFLAGS = $(AM_CPPFLAGS)
libaupad_dbus_la_CFLAGS = $(AM_CFLAGS)
libaupad_dbus_la_CXXFLAGS = $(AM_CXXFLAGS)

nodist_libjsonio_dbus_la_SOURCES = de_tahifi_jsonio.c de_tahifi_jsonio.h de_tahifi_jsonio.hh
libjsonio_dbus_la_CPPFLAGS = $(AM_CPPFLAGS)
libjsonio_dbus_la_CFLAGS = $(AM_CFLAGS)
libjsonio_dbus_la_CXXFLAGS = $(AM_CXXFLAGS)

nodist_libdebug_dbus_la_SOURCES = de_tahifi_debug.c de_tahifi_debug.h de_tahifi_debug.hh
libdebug_dbus_la_CPPFLAGS = $(AM_CPPFLAGS)
libdebug_dbus_la_CFLAGS = $(AM_CFLAGS)
libdebug_dbus_la_CXXFLAGS = $(AM_CXXFLAGS)

DBUS_IFACE_PAGES = \
    de_tahifi_jsonio-doc.mdp \
    de_tahifi_aupad-doc.mdp \
    de_tahifi_debug-doc.mdp

BUILT_SOURCES = \
    all_interfaces_toc.md \
    all_interfaces.md \
    all_interfaces.stamp \
    $(DBUS_IFACE_PAGES) \
    $(nodist_libaupad_dbus_la_SOURCES) \
    de_tahifi_aupad-doc.h \
    de_tahifi_aupad.stamp \
    $(nodist_libjsonio_dbus_la_SOURCES) \
    de_tahifi_jsonio-doc.h \
    de_tahifi_jsonio.stamp \
    $(nodist_libdebug_dbus_la_SOURCES) \
    de_tahifi_debug-doc.h \
    de_tahifi_debug.stamp

CLEANFILES = $(BUILT_SOURCES)

de_tahifi_aupad-doc.h: de_tahifi_aupad.stamp
de_tahifi_aupad-doc.mdp: de_tahifi_aupad.stamp
de_tahifi_aupad.c: de_tahifi_aupad.stamp
de_tahifi_aupad.h: de_tahifi_aupad.stamp
de_tahifi_aupad.hh: de_tahifi_aupad.stamp
de_tahifi_aupad.stamp: $(DBUS_IFACES)/de_tahifi_aupad.xml
	$(GDBUS_CODEGEN) --generate-c-code=de_tahifi_aupad --c-namespace tdbus_aupad --interface-prefix de.tahifi.AuPaD. $<
	PYTHONHOME= $(DBUS_IFACES)/taddybus-codegen.py --c-namespace tdbus_aupad --interface-prefix de.tahifi.AuPaD. --cpp-traits-prefix AuPaD -o de_tahifi_aupad.hh $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o de_tahifi_aupad-doc.mdp -H de_tahifi_aupad-doc.h -c tdbus_aupad -s de.tahifi.AuPaD. -n "$(PACKAGE_NAME)"
	touch $@

de_tahifi_jsonio-doc.h: de_tahifi_jsonio.stamp
de_tahifi_jsonio-doc.mdp: de_tahifi_jsonio.stamp
de_tahifi_jsonio.c: de_tahifi_jsonio.stamp
de_tahifi_jsonio.h: de_tahifi_jsonio.stamp
de_tahifi_jsonio.hh: de_tahifi_jsonio.stamp
de_tahifi_jsonio.stamp: $(DBUS_IFACES)/de_tahifi_jsonio.xml
	$(GDBUS_CODEGEN) --generate-c-code=de_tahifi_jsonio --c-namespace tdbus --interface-prefix de.tahifi. $<
	PYTHONHOME= $(DBUS_IFACES)/taddybus-codegen.py --c-namespace tdbus --interface-prefix de.tahifi. -o de_tahifi_jsonio.hh $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o de_tahifi_jsonio-doc.mdp -H de_tahifi_jsonio-doc.h -c tdbus -s de.tahifi. -n "JSON Object Exchange"
	touch $@

de_tahifi_debug-doc.h: de_tahifi_debug.stamp
de_tahifi_debug-doc.mdp: de_tahifi_debug.stamp
de_tahifi_debug.c: de_tahifi_debug.stamp
de_tahifi_debug.h: de_tahifi_debug.stamp
de_tahifi_debug.hh: de_tahifi_debug.stamp
de_tahifi_debug.stamp: $(DBUS_IFACES)/de_tahifi_debug.xml
	$(GDBUS_CODEGEN) --generate-c-code=de_tahifi_debug --c-namespace tdbus_debug --interface-prefix de.tahifi.Debug. $<
	PYTHONHOME= $(DBUS_IFACES)/taddybus-codegen.py --c-namespace tdbus_debug --interface-prefix de.tahifi.Debug. --cpp-traits-prefix Debug -o de_tahifi_debug.hh $<
	$(DBUS_IFACES)/extract_documentation.py -i $< -o de_tahifi_debug-doc.mdp -H de_tahifi_debug-doc.h -c tdbus_debug -s de.tahifi.Debug. -n "Debug Levels"
	touch $@

all_interfaces_toc.md: all_interfaces.stamp
all_interfaces.md: all_interfaces.stamp
all_interfaces.stamp: $(DBUS_IFACE_PAGES)
	$(DBUS_IFACES)/combine_iface_pages.sh -toc all_interfaces_toc.md -ifaces all_interfaces.md $(DBUS_IFACE_PAGES)
