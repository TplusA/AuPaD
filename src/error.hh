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

#ifndef ERROR_HH
#define ERROR_HH

#include <sstream>
#include <exception>

template <typename ET = std::runtime_error>
class ErrorBase
{
  private:
    std::ostringstream os_;

  public:
    ErrorBase(const ErrorBase &) = delete;
    ErrorBase(ErrorBase &&) = default;
    ErrorBase &operator=(const ErrorBase &) = delete;
    ErrorBase &operator=(ErrorBase &&) = default;

    explicit ErrorBase() = default;

    // cppcheck-suppress exceptThrowInDestructor
    [[ noreturn ]] ~ErrorBase() noexcept(false) { throw ET(os_.str()); }

    template <typename T>
    ErrorBase &operator<<(const T &d)
    {
        os_ << d;
        return *this;
    }
};

using Error = ErrorBase<>;

#endif /* !ERROR_HH */
