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

#ifndef CLIENT_PLUGIN_HH
#define CLIENT_PLUGIN_HH

#include <string>
#include <vector>

namespace ConfigStore
{
class Settings;
class Changes;
}

namespace ClientPlugin
{

class Plugin
{
  public:
    const std::string name_;

  private:
    unsigned int reference_count_;

  protected:
    explicit Plugin(std::string &&name):
        name_(std::move(name)),
        reference_count_(0)
    {}

  public:
    Plugin(const Plugin &) = delete;
    Plugin(Plugin &&) = default;
    Plugin &operator=(const Plugin &) = delete;
    Plugin &operator=(Plugin &&) = default;
    virtual ~Plugin() = default;

    virtual void registered() = 0;
    virtual void unregistered() = 0;
    virtual void report_changes(const ConfigStore::Settings &settings,
                                const ConfigStore::Changes &changes) const = 0;
    virtual bool full_report(const ConfigStore::Settings &settings,
                             std::string &report, std::vector<std::string> &extra) const = 0;

    bool has_clients() const { return reference_count_ > 0; }
    void add_client() { ++reference_count_; }
    void remove_client() { --reference_count_; }
};

}

#endif /* !CLIENT_PLUGIN_HH */
