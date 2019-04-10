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

#ifndef ERROR_HH
#define ERROR_HH

#include <sstream>

class Error
{
  private:
    std::ostringstream os_;

  public:
    Error(const Error &) = delete;
    Error(Error &&) = default;
    Error &operator=(const Error &) = delete;
    Error &operator=(Error &&) = default;

    explicit Error() = default;

    ~Error() noexcept(false) { throw std::runtime_error(os_.str()); }

    template <typename T>
    Error &operator<<(const T &d)
    {
        os_ << d;
        return *this;
    }
};

#endif /* !ERROR_HH */
