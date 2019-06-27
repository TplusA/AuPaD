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

#ifndef REPORT_ROON_HH
#define REPORT_ROON_HH

#include "client_plugin.hh"
#include "signal_path_tracker.hh"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#include "json.hh"
#pragma GCC diagnostic pop

#include <functional>

namespace StaticModels
{
    namespace Elements
    {
        class Control;
        class AudioSink;
    }
}

namespace ClientPlugin
{

class Roon: public Plugin
{
  public:
    using EmitSignalPathFn =
        std::function<void(const std::string &asp,
                           const std::vector<std::string> &extra)>;
    class Cache
    {
      public:
        static constexpr auto INVALID_RANK = std::numeric_limits<uint16_t>::max();

      private:
        ModelCompliant::SignalPathTracker::ActivePath path_;
        uint16_t path_rank_;
        std::vector<std::pair<nlohmann::json,
                              const StaticModels::Elements::Control *>>
            reported_fragments_;
        std::unordered_map<std::string, unsigned int> elem_to_frag_index_;

      public:
        Cache(const Cache &) = delete;
        Cache(Cache &&) = default;
        Cache &operator=(const Cache &) = delete;
        Cache &operator=(Cache &&) = default;

        explicit Cache():
            path_rank_(INVALID_RANK)
        {}

        bool clear()
        {
            const bool result = !path_.empty();
            path_.clear();
            path_rank_ = INVALID_RANK;
            reported_fragments_.clear();
            elem_to_frag_index_.clear();
            return result;
        }

        bool empty() const { return path_.empty(); }

        bool put_path(const ModelCompliant::SignalPathTracker::ActivePath &path,
                      uint16_t path_rank)
        {
            if(path_rank >= path_rank_)
            {
                if(path_rank == path_rank_ && path_rank != INVALID_RANK)
                    msg_error(0, LOG_WARNING,
                              "There are multiple equally ranked signal paths "
                              "(reporting only one of them to Roon)");

                return false;
            }

            path_ = path;
            path_rank_ = path_rank;
            reported_fragments_.clear();
            elem_to_frag_index_.clear();
            return true;
        }

        const auto &get_path() const { return path_; }

        void append_fragment(
                std::string &&element_name,
                std::pair<nlohmann::json,
                          const StaticModels::Elements::Control *> &&fragment)
        {
            elem_to_frag_index_[std::move(element_name)] = reported_fragments_.size();
            reported_fragments_.emplace_back(std::move(fragment));
        }

        void update_fragment(const std::string &element_name,
                             nlohmann::json &&fragment)
        {
            reported_fragments_.at(elem_to_frag_index_.at(element_name)).first =
                std::move(fragment);
        }

        std::pair<nlohmann::json, const StaticModels::Elements::Control *> &
        lookup_fragment(const std::string &element_name)
        {
            return reported_fragments_.at(elem_to_frag_index_.at(element_name));
        }
    };

  private:
    const EmitSignalPathFn emit_audio_signal_path_fn_;
    mutable Cache cache_;
    mutable std::unordered_map<const StaticModels::Elements::AudioSink *, uint16_t> ranks_;

  public:
    Roon(const Roon &) = delete;
    Roon(Roon &&) = default;
    Roon &operator=(const Roon &) = delete;
    Roon &operator=(Roon &&) = default;

    explicit Roon(EmitSignalPathFn &&emit_path):
        Plugin("Roon"),
        emit_audio_signal_path_fn_(std::move(emit_path))
    {}

    void registered() final override;
    void unregistered() final override;
    void report_changes(const ConfigStore::Settings &settings,
                        const ConfigStore::Changes &changes) const final override;
    bool full_report(const ConfigStore::Settings &settings,
                     std::string &report, std::vector<std::string> &extra) const
        final override;
};

}

#endif /* !REPORT_ROON_HH */
