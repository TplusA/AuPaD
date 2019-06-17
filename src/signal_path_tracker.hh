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

#ifndef SIGNAL_PATH_TRACKER_HH
#define SIGNAL_PATH_TRACKER_HH

#include "signal_paths.hh"

namespace ModelCompliant
{

/*!
 * Per-instance audio signal path tracking.
 */
class SignalPathTracker
{
  private:
    const StaticModels::SignalPaths::Appliance &dev_;
    std::unordered_map<const StaticModels::SignalPaths::SwitchingElement *,
                       StaticModels::SignalPaths::Selector> selector_values_;
    std::vector<std::pair<const StaticModels::SignalPaths::PathElement *, bool>> sources_;

  public:
    SignalPathTracker(const SignalPathTracker &) = delete;
    SignalPathTracker(SignalPathTracker &&) = default;
    SignalPathTracker &operator=(const SignalPathTracker &) = delete;
    SignalPathTracker &operator=(SignalPathTracker &&) = default;

    explicit SignalPathTracker(const StaticModels::SignalPaths::Appliance &dev):
        dev_(dev)
    {
        dev_.for_each_source(
            [this] (const auto &src) { sources_.push_back({&src, false}); });
    }

    bool select(const std::string &element_name,
                const StaticModels::SignalPaths::Selector &sel);

    bool floating(const std::string &element_name);

    StaticModels::SignalPaths::Selector
    get_selector_value(const StaticModels::SignalPaths::SwitchingElement *elem) const
    {
        const auto it(selector_values_.find(elem));

        if(it != selector_values_.end())
            return it->second;
        else
            return StaticModels::SignalPaths::Selector::mk_invalid();
    }

    using ActivePath =
        std::vector<std::pair<const StaticModels::SignalPaths::PathElement *, bool>>;
    using EnumerateCallbackFn = std::function<bool(const ActivePath &)>;

    bool enumerate_active_signal_paths(const EnumerateCallbackFn &fn,
                                       bool is_root_device) const;
};

}

#endif /* !SIGNAL_PATH_TRACKER_HH */
