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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include "report_roon.hh"
#include "configstore.hh"
#include "configstore_json.hh"
#include "configstore_changes.hh"
#include "configstore_iter.hh"
#include "device_models.hh"
#include "messages.h"

void ClientPlugin::Roon::registered()
{
    msg_info("Registered plugin \"%s\"", name_.c_str());
}

void ClientPlugin::Roon::unregistered()
{
    msg_info("Unregistered plugin \"%s\"", name_.c_str());
}

enum class AddResult
{
    IGNORED, /* soft error */
    NEUTRAL,
    ADDED,
};

static AddResult add_flag_entry(const std::string &name,
                                nlohmann::json &output,
                                const ConfigStore::Value &value,
                                const nlohmann::json &ctrl,
                                const nlohmann::json &roon_conversion)
{
    const auto neutral(ctrl.find("neutral_setting"));

    if(neutral != ctrl.end())
    {
        if(!neutral->is_string())
            msg_error(0, LOG_NOTICE,
                      "Invalid \"neutral_setting\" for control %s",
                      name.c_str());
        else if((*neutral == "off" && value.get_value() == false) ||
                (*neutral == "on" && value.get_value() == true))
            return AddResult::NEUTRAL;
    }
    else if(value.get_value() == false)
        return AddResult::NEUTRAL;

    output.push_back(roon_conversion.at("template"));
    return AddResult::ADDED;
}

template <ConfigStore::ValueType VT>
static double compute_value_ratio(
        const nlohmann::json &value,
        const std::pair<const nlohmann::json &, const nlohmann::json &> &range)
{
    const auto v(ConfigStore::get_range_checked<VT>(value));
    const auto min(ConfigStore::get_range_checked<VT>(range.first));
    const auto max(ConfigStore::get_range_checked<VT>(range.second));

    if(v >= min && v <= max && min <= max)
        return double(v - min) / (max - min);
    else
        return std::numeric_limits<double>::infinity();
}

template <ConfigStore::ValueType VT, typename Traits = ConfigStore::ValueTypeTraits<VT>>
static std::pair<nlohmann::json, AddResult>
range_pick(double ratio,
           const std::pair<const nlohmann::json &, const nlohmann::json &> &range)
{
    if(ratio < 0.0 || ratio > 1.0)
    {
        BUG("Invalid ratio %f", ratio);
        return std::make_pair(nlohmann::json(), AddResult::IGNORED);
    }

    const auto min(ConfigStore::get_range_checked<VT>(range.first));
    const auto max(ConfigStore::get_range_checked<VT>(range.second));
    const double value = (min <= max)
        ? min + ratio * (max - min)
        : max + (1.0 - ratio) * (min - max);

    return std::make_pair(nlohmann::json(value), AddResult::ADDED);
}

static std::pair<nlohmann::json, AddResult>
map_value(const std::string &name, const ConfigStore::Value &value,
          const nlohmann::json &ctrl, const nlohmann::json &mapping)
{
    const auto &mapping_type = mapping.at("type");
    const auto &target_type_code = mapping.at("value_type").get<std::string>();
    const auto target_type = ConfigStore::Value::type_code_to_type(target_type_code);

    if(mapping_type == "direct")
        return std::make_pair(value.get_as(target_type), AddResult::ADDED);
    else if(mapping_type == "to_range")
    {
        const auto &input_type_code = ctrl.at("value_type");
        const auto input_type = ConfigStore::Value::type_code_to_type(input_type_code);
        const auto input_range(std::make_pair(std::cref(ctrl.at("min")),
                                              std::cref(ctrl.at("max"))));
        const auto output_range(std::make_pair(std::cref(mapping.at("from")),
                                               std::cref(mapping.at("to"))));

        auto ratio = std::numeric_limits<double>::infinity();

        switch(input_type)
        {
          case ConfigStore::ValueType::VT_VOID:
          case ConfigStore::ValueType::VT_ASCIIZ:
          case ConfigStore::ValueType::VT_BOOL:
          case ConfigStore::ValueType::VT_TA_FIX_POINT:
            break;

          case ConfigStore::ValueType::VT_INT8:
            ratio = compute_value_ratio<ConfigStore::ValueType::VT_INT8>(
                        value.get_value(), input_range);
            break;

          case ConfigStore::ValueType::VT_INT16:
            ratio = compute_value_ratio<ConfigStore::ValueType::VT_INT16>(
                        value.get_value(), input_range);
            break;

          case ConfigStore::ValueType::VT_INT32:
            ratio = compute_value_ratio<ConfigStore::ValueType::VT_INT32>(
                        value.get_value(), input_range);
            break;

          case ConfigStore::ValueType::VT_INT64:
            ratio = compute_value_ratio<ConfigStore::ValueType::VT_INT64>(
                        value.get_value(), input_range);
            break;

          case ConfigStore::ValueType::VT_UINT8:
            ratio = compute_value_ratio<ConfigStore::ValueType::VT_UINT8>(
                        value.get_value(), input_range);
            break;

          case ConfigStore::ValueType::VT_UINT16:
            ratio = compute_value_ratio<ConfigStore::ValueType::VT_UINT16>(
                        value.get_value(), input_range);
            break;

          case ConfigStore::ValueType::VT_UINT32:
            ratio = compute_value_ratio<ConfigStore::ValueType::VT_UINT32>(
                        value.get_value(), input_range);
            break;

          case ConfigStore::ValueType::VT_UINT64:
            ratio = compute_value_ratio<ConfigStore::ValueType::VT_UINT64>(
                        value.get_value(), input_range);
            break;

          case ConfigStore::ValueType::VT_DOUBLE:
            ratio = compute_value_ratio<ConfigStore::ValueType::VT_DOUBLE>(
                        value.get_value(), input_range);
            break;
        }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
        if(ratio == std::numeric_limits<double>::infinity())
#pragma GCC diagnostic pop
        {
            msg_error(0, LOG_NOTICE,
                      "Unsupported input mapping type \"%s\" for control %s",
                      target_type_code.c_str(), name.c_str());
            return std::make_pair(nlohmann::json(), AddResult::IGNORED);
        }

        switch(target_type)
        {
          case ConfigStore::ValueType::VT_VOID:
          case ConfigStore::ValueType::VT_ASCIIZ:
          case ConfigStore::ValueType::VT_BOOL:
          case ConfigStore::ValueType::VT_TA_FIX_POINT:
            break;

          case ConfigStore::ValueType::VT_INT8:
            return range_pick<ConfigStore::ValueType::VT_INT8>(ratio, output_range);

          case ConfigStore::ValueType::VT_INT16:
            return range_pick<ConfigStore::ValueType::VT_INT16>(ratio, output_range);

          case ConfigStore::ValueType::VT_INT32:
            return range_pick<ConfigStore::ValueType::VT_INT32>(ratio, output_range);

          case ConfigStore::ValueType::VT_INT64:
            return range_pick<ConfigStore::ValueType::VT_INT64>(ratio, output_range);

          case ConfigStore::ValueType::VT_UINT8:
            return range_pick<ConfigStore::ValueType::VT_UINT8>(ratio, output_range);

          case ConfigStore::ValueType::VT_UINT16:
            return range_pick<ConfigStore::ValueType::VT_UINT16>(ratio, output_range);

          case ConfigStore::ValueType::VT_UINT32:
            return range_pick<ConfigStore::ValueType::VT_UINT32>(ratio, output_range);

          case ConfigStore::ValueType::VT_UINT64:
            return range_pick<ConfigStore::ValueType::VT_UINT64>(ratio, output_range);

          case ConfigStore::ValueType::VT_DOUBLE:
            return range_pick<ConfigStore::ValueType::VT_DOUBLE>(ratio, output_range);
        }

        msg_error(0, LOG_NOTICE,
                  "Unsupported target mapping type \"%s\" for control %s",
                  target_type_code.c_str(), name.c_str());
        return std::make_pair(nlohmann::json(), AddResult::IGNORED);
    }
    else
    {
        msg_error(0, LOG_NOTICE,
                  "Unknown value mapping type \"%s\" for control %s",
                  mapping_type.get<std::string>().c_str(), name.c_str());
        return std::make_pair(nlohmann::json(), AddResult::IGNORED);
    }
}

static AddResult add_value_entry(const std::string &name,
                                 nlohmann::json &output,
                                 const ConfigStore::Value &value,
                                 const nlohmann::json &ctrl,
                                 const nlohmann::json &roon_conversion)
{
    const auto vm(roon_conversion.find("value_mapping"));
    const auto vn(roon_conversion.find("value_name"));

    if(vm == roon_conversion.end() || vn == roon_conversion.end())
    {
        if(vm == roon_conversion.end() && vn == roon_conversion.end())
            msg_error(0, LOG_NOTICE,
                      "Need \"value_name\" and \"value_mapping\" for control %s",
                      name.c_str());

        return AddResult::IGNORED;
    }
    else if(!vn->is_string())
    {
        msg_error(0, LOG_NOTICE,
                  "The \"value_name\" must be a string in control %s",
                  name.c_str());
        return AddResult::IGNORED;
    }

    const auto neutral(ctrl.find("neutral_setting"));
    if(neutral != ctrl.end() && *neutral == value.get_value())
        return AddResult::NEUTRAL;

    auto mapped(map_value(name, value, ctrl, *vm));

    switch(mapped.second)
    {
      case AddResult::IGNORED:
        return mapped.second;

      case AddResult::NEUTRAL:
        BUG("%s(%d): case shouldn't occur", __func__, __LINE__);
        return mapped.second;

      case AddResult::ADDED:
        break;
    }

    nlohmann::json entry = roon_conversion.at("template");
    entry[vn->get<std::string>()] = std::move(mapped.first);
    output.emplace_back(std::move(entry));
    return AddResult::ADDED;
}

static AddResult add_entry(const std::string &name, nlohmann::json &output,
                           const ConfigStore::Value &value,
                           const nlohmann::json &ctrl,
                           const nlohmann::json &roon_conversion)
{
    if(ctrl["type"] == "on_off")
        return add_flag_entry(name, output, value, ctrl, roon_conversion);
    else
        return add_value_entry(name, output, value, ctrl, roon_conversion);
}

static bool add_entry_for_name(const ConfigStore::ConstSettingsJSON &js,
                               const std::string &name,
                               const ConfigStore::Value &value,
                               nlohmann::json &output)
{
    const auto ctrl(js.retrieve_control_definition_from_model(name));
    const auto it(ctrl.find("roon"));
    if(it == ctrl.end())
        return false;

    try
    {
        switch(add_entry(name, output, value, ctrl, *it))
        {
          case AddResult::IGNORED:
          case AddResult::ADDED:
            return false;

          case AddResult::NEUTRAL:
            return true;
        }
    }
    catch(...)
    {
        msg_error(0, LOG_NOTICE,
                  "Failed adding entry for Roon for element control \"%s\"",
                  name.c_str());
    }

    return false;
}

void ClientPlugin::Roon::report_changes(const ConfigStore::Settings &settings,
                                        const ConfigStore::Changes &changes) const
{
    nlohmann::json output;
    const ConfigStore::ConstSettingsJSON js(settings);
    bool force_update = false;

    changes.for_each_changed_value(
        [&js, &output, &force_update]
        (const auto &name, const auto &old_value, const auto &new_value)
        {
            if(add_entry_for_name(js, name, new_value, output))
                force_update = true;
        });

    if(output != nullptr)
        emit_audio_signal_path_fn_(output.dump(), false);
    else if(force_update)
        emit_audio_signal_path_fn_("[]", false);
}

bool ClientPlugin::Roon::full_report(const ConfigStore::Settings &settings,
                                     std::string &report) const
{
    nlohmann::json output;
    const ConfigStore::ConstSettingsJSON js(settings);
    const ConfigStore::SettingsIterator si(settings);

    try
    {
        si.with_device("self").for_each_setting(
            [&output, &js]
            (const std::string &element_name, const std::string &value_name,
             const ConfigStore::Value &value)
            {
                const auto name("self." + element_name + '.' + value_name);
                add_entry_for_name(js, name, value, output);
                return true;
            });
    }
    catch(...)
    {
        return false;
    }

    if(output != nullptr)
        report = output.dump();
    else
        report = "[]";

    return true;
}
