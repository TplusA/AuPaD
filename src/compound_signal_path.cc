/*
 * Copyright (C) 2021, 2023  T+A elektroakustik GmbH & Co. KG
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

#include "compound_signal_path.hh"
#include "device_models.hh"

bool ModelCompliant::CompoundSignalPathTracker::enumerate_compound_signal_paths(
        const std::string &device_instance_name,
        const EnumerateCallbackFn &fn)
{
    fn_ = &fn;
    const auto result = enumerate_compound_signal_paths(device_instance_name, "");
    fn_ = nullptr;
    return result;
}

bool ModelCompliant::CompoundSignalPathTracker::enumerate_compound_signal_paths(
        const std::string &device_instance_name,
        const std::string &input_name_filter)
{
    const auto &dev_ctx(settings_iterator_.with_device(device_instance_name));
    if(dev_ctx.get_model() == nullptr)
        return true;

    device_name_store_.emplace_back(device_instance_name,
                                    current_path_.path_.size());

    const auto result = dev_ctx.for_each_signal_path(
        [this, &dev_ctx, &input_name_filter]
        (const SignalPathTracker::ActivePath &partial)
        {
            if(!input_name_filter.empty() &&
               partial.front().first->get_name() != input_name_filter)
                return true;

            extend_path(partial);

            bool has_connected_device = false;
            const auto &sink_elem(*partial.back().first);

            dev_ctx.for_each_outgoing_connection_from_sink(
                sink_elem.get_name(),
                [this, &has_connected_device]
                (const std::string &target_instance_name,
                 const std::string &input_name)
                {
                    if(enumerate_compound_signal_paths(target_instance_name,
                                                       input_name))
                        has_connected_device = true;
                }
            );

            return has_connected_device || (*fn_)(current_path_);
        }
    );

    msg_log_assert(!device_name_store_.empty());
    device_name_store_.pop_back();

    return result;
}
