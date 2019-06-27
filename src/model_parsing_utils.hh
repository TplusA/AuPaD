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

#ifndef MODEL_PARSING_UTILS_HH
#define MODEL_PARSING_UTILS_HH

#include "error.hh"

#include <string>
#include <tuple>

namespace StaticModels
{

namespace Utils
{

static bool contains_separator(const std::string &name, const char sep,
                               size_t *out_sep_pos)
{
    const auto sep_pos = name.find(sep);

    if(sep_pos == std::string::npos || sep_pos == 0 ||
       sep_pos == name.length() - 1)
        return false;

    if(out_sep_pos != nullptr)
        *out_sep_pos = sep_pos;

    return true;
}

static bool is_qualified_name(const std::string &name,
                              size_t *out_sep_pos = nullptr)
{
    return contains_separator(name, '.', out_sep_pos);
}

static inline std::tuple<std::string, std::string>
split_qualified_name(const std::string &name, bool allow_unqualified = false)
{
    size_t sep_pos;
    if(!is_qualified_name(name, &sep_pos))
    {
        if(allow_unqualified)
            return std::make_tuple(name, "");

        Error() << "element name \"" << name <<
            "\" is not a fully qualified name";
    }

    return std::make_tuple(name.substr(0, sep_pos), name.substr(sep_pos + 1));
}

static bool is_mapping_spec(const std::string &spec,
                            size_t *out_sep_pos = nullptr)
{
    return contains_separator(spec, '@', out_sep_pos);
}

static inline std::tuple<std::string, std::string>
split_mapping_spec(const std::string &spec)
{
    size_t sep_pos;
    if(!is_mapping_spec(spec, &sep_pos))
        Error() << "string \"" << spec <<
            "\" is not a mapping selector specification";

    return std::make_tuple(spec.substr(0, sep_pos), spec.substr(sep_pos + 1));
}

}

}

#endif /* !MODEL_PARSING_UTILS_HH */
