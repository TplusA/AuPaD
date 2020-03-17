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

#include "dbus/de_tahifi_debug.hh"

static enum MessageVerboseLevel do_set_debug_level(const char *new_level_name)
{
    static enum MessageVerboseLevel default_level = MESSAGE_LEVEL_IMPOSSIBLE;
    static const std::string default_level_name = "default";

    if(default_level == MESSAGE_LEVEL_IMPOSSIBLE)
        default_level = msg_get_verbose_level();

    enum MessageVerboseLevel old_level = msg_get_verbose_level();
    enum MessageVerboseLevel new_level;

    if(new_level_name == NULL || new_level_name[0] == '\0')
    {
        new_level = old_level;
        new_level_name = msg_verbose_level_to_level_name(new_level);
    }
    else if(new_level_name == default_level_name)
    {
        new_level = default_level;
        new_level_name = msg_verbose_level_to_level_name(new_level);
    }
    else
    {
        new_level = msg_verbose_level_name_to_level(new_level_name);

        if(new_level == MESSAGE_LEVEL_IMPOSSIBLE)
            old_level = MESSAGE_LEVEL_IMPOSSIBLE;
    }

    if(new_level != old_level)
    {
        msg_vinfo(MESSAGE_LEVEL_INFO_MIN,
                  "Set debug level \"%s\"", new_level_name);
        msg_set_verbose_level(new_level);
    }
    else if(old_level == MESSAGE_LEVEL_IMPOSSIBLE)
        msg_error(0, LOG_ERR, "Log level \"%s\" invalid", new_level_name);

    return old_level;
}

namespace TDBus
{

gboolean MethodHandlerTraits<DebugLoggingDebugLevel>::simple_method_handler(
            IfaceType *const object, GDBusMethodInvocation *const invocation,
            const char *const arg_new_level,
            Iface<IfaceType> *const iface)
{
    const enum MessageVerboseLevel old_level = do_set_debug_level(arg_new_level);
    const char *name = msg_verbose_level_to_level_name(old_level);

    if(name == NULL)
        name = "";

    iface->method_done<ThisMethod>(invocation, name);

    return TRUE;
}

void SignalHandlerTraits<DebugLoggingConfigGlobalDebugLevelChanged>::simple_signal_handler(
            IfaceType *const object,
            const char *const new_level_name,
            Proxy<IfaceType> *const proxy)
{
    do_set_debug_level(new_level_name);
}

}
