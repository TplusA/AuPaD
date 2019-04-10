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

#ifndef CONFIGVALUE_HH
#define CONFIGVALUE_HH

#include "fixpoint.hh"
#include "error.hh"

#include <string>

namespace ConfigStore
{

enum class ValueType
{
    VT_VOID,
    VT_ASCIIZ,
    VT_BOOL,
    VT_INT8,
    VT_UINT8,
    VT_INT16,
    VT_UINT16,
    VT_INT32,
    VT_UINT32,
    VT_INT64,
    VT_UINT64,
    VT_TA_FIX_POINT,

    VT_LAST_VALUE = VT_TA_FIX_POINT,
};

template <ValueType VT> struct ValueTypeTraits;

/*!
 * Simple variant type based on nlohmann::json (for simplicity).
 */
class Value
{
  private:
    ValueType type_;
    nlohmann::json value_;

    static const std::array<const std::pair<const char, const ValueType>, 12>
    TYPE_CODE_TO_VALUE_TYPE;
    static_assert(TYPE_CODE_TO_VALUE_TYPE.size() == size_t(ValueType::VT_LAST_VALUE) + 1,
                  "unexpected array size");


    static const std::array<const char, 12> VALUE_TYPE_TO_TYPE_CODE;
    static_assert(VALUE_TYPE_TO_TYPE_CODE.size() == size_t(ValueType::VT_LAST_VALUE) + 1,
                  "unexpected array size");

  public:
    Value(const Value &) = default;
    Value(Value &&) = default;
    Value &operator=(const Value &) = delete;
    Value &operator=(Value &&) = default;

    explicit Value(): type_(ValueType::VT_VOID) {}

    explicit Value(const std::string &type_code, nlohmann::json &&value):
        type_(type_code_to_type(type_code)),
        value_(std::move(value))
    {
        validate();
    }

    const auto &get_value() const { return value_; }
    const char get_type_code() const { return type_to_type_code(type_); }

    static char type_to_type_code(ValueType vt)
    {
        return VALUE_TYPE_TO_TYPE_CODE[size_t(vt)];
    }

    static ValueType type_code_to_type(const std::string &type_code);

    bool operator==(const Value &other) const
    {
        return type_ == other.type_ && value_ == other.value_;
    }

    bool operator<(const Value &other) const
    {
        return type_ == other.type_ && value_ < other.value_;
    }

  private:
    void validate() const;
};

template <>
struct ValueTypeTraits<ValueType::VT_INT8>
{ using TargetType = int8_t; using GetType = int64_t; };

template <>
struct ValueTypeTraits<ValueType::VT_INT16>
{ using TargetType = int16_t; using GetType = int64_t; };

template <>
struct ValueTypeTraits<ValueType::VT_INT32>
{ using TargetType = int32_t; using GetType = int64_t; };

template <>
struct ValueTypeTraits<ValueType::VT_INT64>
{ using TargetType = int64_t; using GetType = int64_t; };

template <>
struct ValueTypeTraits<ValueType::VT_UINT8>
{ using TargetType = uint8_t; using GetType = uint64_t; };

template <>
struct ValueTypeTraits<ValueType::VT_UINT16>
{ using TargetType = uint16_t; using GetType = uint64_t; };

template <>
struct ValueTypeTraits<ValueType::VT_UINT32>
{ using TargetType = uint32_t; using GetType = uint64_t; };

template <>
struct ValueTypeTraits<ValueType::VT_UINT64>
{ using TargetType = uint64_t; using GetType = uint64_t; };

template <ValueType VT, typename Traits = ValueTypeTraits<VT>>
void range_check(const nlohmann::json &value)
{
    const auto v(value.get<typename Traits::GetType>());
    if(v > std::numeric_limits<typename Traits::TargetType>::max() ||
       v < std::numeric_limits<typename Traits::TargetType>::min())
    {
        Error() <<
            "value " << value << " out of range [" <<
            typename Traits::GetType(std::numeric_limits<typename Traits::TargetType>::min()) <<
            ", " <<
            typename Traits::GetType(std::numeric_limits<typename Traits::TargetType>::max()) <<
            "] according to type code " << Value::type_to_type_code(VT);
    }
}

}

#endif /* !CONFIGVALUE_HH */
