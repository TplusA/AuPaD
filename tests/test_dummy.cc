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

#include <doctest.h>

#include "dbus/taddybus.hh"

/* dummies to avoid linking against GLib with its stupid memory management */
void g_error_free(GError *) { FAIL("unexpected"); }
void g_bus_unown_name(guint) { FAIL("unexpected"); }
void g_dbus_method_invocation_return_error_valist(
        GDBusMethodInvocation *, GQuark, gint, const gchar *, va_list) {}
void g_bus_unwatch_name(guint) { FAIL("unexpected"); }
GQuark g_dbus_error_quark() { FAIL("unexpected"); return GQuark(); }
guint g_bus_own_name(GBusType, const gchar *, GBusNameOwnerFlags,
                     GBusAcquiredCallback, GBusNameAcquiredCallback,
                     GBusNameLostCallback, gpointer, GDestroyNotify)
{
    FAIL("unexpected");
    return 0;
}
guint g_bus_watch_name_on_connection(GDBusConnection *, const gchar *,
                                     GBusNameWatcherFlags, GBusNameAppearedCallback,
                                     GBusNameVanishedCallback, gpointer,
                                     GDestroyNotify)
{
    FAIL("unexpected");
    return 0;
}


TEST_CASE("Dummy")
{
    CHECK(TDBus::log_dbus_error(nullptr, nullptr));
}
