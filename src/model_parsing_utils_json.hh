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

#ifndef MODEL_PARSING_UTILS_JSON_HH
#define MODEL_PARSING_UTILS_JSON_HH

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
#include "json.hh"
#pragma GCC diagnostic pop

namespace StaticModels
{

namespace Utils
{

template <typename T>
static T get(const nlohmann::json &j, const char *key, T &&fallback)
{
    const auto it(j.find(key));
    if(it != j.end())
        return it->get<T>();
    else
        return fallback;
}

}

}

#endif /* !MODEL_PARSING_UTILS_JSON_HH */
