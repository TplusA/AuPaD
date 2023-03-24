/*
 * Copyright (C) 2019, 2021, 2023  T+A elektroakustik GmbH & Co. KG
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

#ifndef CONFIGSTORE_CHANGES_HH
#define CONFIGSTORE_CHANGES_HH

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#pragma GCC diagnostic ignored "-Wtype-limits"
#ifdef __clang__
#pragma GCC diagnostic ignored "-Wc++17-extensions"
#endif /* __clang__ */
#include "json.hh"
#pragma GCC diagnostic pop

#include "configvalue.hh"

#include <functional>

namespace ConfigStore
{

class ChangeLog;

class Changes
{
  private:
    std::unique_ptr<ChangeLog> changes_;

  public:
    Changes(const Changes &) = delete;
    Changes(Changes &&) = default;
    Changes &operator=(const Changes &) = delete;
    Changes &operator=(Changes &&) = default;

    explicit Changes();
    ~Changes();

    void reset(std::unique_ptr<ConfigStore::ChangeLog> changes);
    void reset();

    void for_each_changed_device(const std::function<void(const std::string &, bool)> &apply) const;
    void for_each_changed_connection(const std::function<void(const std::string &from, const std::string &to, bool)> &apply) const;
    void for_each_changed_value(const std::function<void(const std::string &name, const Value &old_value, const Value &new_value)> &apply) const;
};

}

#endif /* !CONFIGSTORE_CHANGES_HH */
