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

#ifndef COMPOUND_SIGNAL_PATH_HH
#define COMPOUND_SIGNAL_PATH_HH

#include "signal_path_tracker.hh"
#include "configstore_iter.hh"

namespace ModelCompliant
{

class CompoundSignalPathTracker;

/*!
 * Representation of an audio path spanning multiple, connected appliances.
 */
class CompoundSignalPath
{
  private:
    std::vector<std::pair<size_t, const StaticModels::SignalPaths::PathElement *>> path_;
    std::vector<std::pair<std::string, size_t>> device_name_store_;

  public:
    CompoundSignalPath(const CompoundSignalPath &) = delete;
    CompoundSignalPath(CompoundSignalPath &&) = default;
    CompoundSignalPath &operator=(const CompoundSignalPath &) = delete;
    CompoundSignalPath &operator=(CompoundSignalPath &&) = default;

    explicit CompoundSignalPath() = default;

    auto begin() const { return path_.begin(); }
    auto end() const { return path_.end(); }
    const auto &back() const { return path_.back(); }
    bool empty() const { return path_.empty(); }

    void clear()
    {
        path_.clear();
        device_name_store_.clear();
    }

    bool operator==(const CompoundSignalPath &other) const
    {
        if(path_.size() == other.path_.size())
            return std::equal(path_.begin(), path_.end(), other.path_.begin(),
                [] (const auto &a, const auto &b) { return a.second == b.second; });
        else
            return false;
    }

    const std::string &map_path_index_to_device_name(size_t idx) const
    {
        return device_name_store_[idx].first;
    }

    friend CompoundSignalPathTracker;
};

/*!
 * Helper for enumerating compound audio paths.
 */
class CompoundSignalPathTracker
{
  public:
    using EnumerateCallbackFn = std::function<bool(const CompoundSignalPath &)>;
    const ConfigStore::SettingsIterator &settings_iterator_;

  private:
    const EnumerateCallbackFn *fn_;
    CompoundSignalPath current_path_;
    std::vector<std::pair<std::string, size_t>> device_name_store_;

  public:
    CompoundSignalPathTracker(const CompoundSignalPathTracker &) = delete;
    CompoundSignalPathTracker(CompoundSignalPathTracker &&) = default;
    CompoundSignalPathTracker &operator=(const CompoundSignalPathTracker &) = delete;
    CompoundSignalPathTracker &operator=(CompoundSignalPathTracker &&) = default;

    explicit CompoundSignalPathTracker(const ConfigStore::SettingsIterator &iter):
        settings_iterator_(iter),
        fn_(nullptr)
    {}

  private:
    void extend_path(const SignalPathTracker::ActivePath &partial)
    {
        msg_log_assert(!device_name_store_.empty());
        current_path_.path_.resize(device_name_store_.back().second);
        std::transform(partial.begin(), partial.end(),
                       std::back_inserter(current_path_.path_),
            [this] (const auto &in)
            {
                return std::make_pair(device_name_store_.size() - 1, in.first);
            });
    }

    bool enumerate_compound_signal_paths(const std::string &device_instance_name,
                                         const std::string &input_name_filter);

  public:
    bool enumerate_compound_signal_paths(const std::string &device_instance_name,
                                         const EnumerateCallbackFn &fn);

    const std::string &map_path_index_to_device_name(size_t idx) const
    {
        return device_name_store_[idx].first;
    }

    CompoundSignalPath mk_self_contained_path(const CompoundSignalPath &src) const
    {
        CompoundSignalPath dest;
        dest.path_ = src.path_;
        dest.device_name_store_ = device_name_store_;
        return dest;
    }
};

}

#endif /* !COMPOUND_SIGNAL_PATH_HH */
