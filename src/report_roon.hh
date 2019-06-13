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

#include <functional>

namespace ClientPlugin
{

class Roon: public Plugin
{
  private:
    const std::function<void(const std::string &, bool)> emit_audio_signal_path_fn_;

  public:
    Roon(const Roon &) = delete;
    Roon(Roon &&) = default;
    Roon &operator=(const Roon &) = delete;
    Roon &operator=(Roon &&) = default;

    explicit Roon(std::function<void(const std::string &asp,
                                           bool is_full_signal_path)> &&emit_path):
        Plugin("Roon"),
        emit_audio_signal_path_fn_(std::move(emit_path))
    {}

    void registered() final override;
    void unregistered() final override;
    void report_changes(const ConfigStore::Settings &settings,
                        const ConfigStore::Changes &changes) const final override;
    bool full_report(const ConfigStore::Settings &settings,
                     std::string &report) const final override;
};

}

#endif /* !REPORT_ROON_HH */
