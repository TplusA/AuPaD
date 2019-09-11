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
#include "model_parsing_utils.hh"
#include "model_parsing_utils_json.hh"
#include "messages.h"

const uint16_t ClientPlugin::Roon::Cache::INVALID_RANK;

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

using ProcessValueFn =
    std::function<void(nlohmann::json &&, const std::string *key,
                       const StaticModels::Elements::Control &)>;

static AddResult process_flag_entry(const ConfigStore::Value &value,
                                    const StaticModels::Elements::OnOff &ctrl,
                                    const ProcessValueFn &process_fn)
{
    if(ctrl.get_neutral_value() == value.get_value())
    {
        process_fn(nullptr, nullptr, ctrl);
        return AddResult::NEUTRAL;
    }

    process_fn(true, nullptr, ctrl);
    return AddResult::ADDED;
}

template <ConfigStore::ValueType VT>
static double compute_value_ratio(
        const nlohmann::json &value,
        const ConfigStore::Value &range_min, const ConfigStore::Value &range_max)
{
    const auto v(ConfigStore::get_range_checked<VT>(value));
    const auto min(ConfigStore::get_range_checked<VT>(range_min.get_value()));
    const auto max(ConfigStore::get_range_checked<VT>(range_max.get_value()));

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
map_value_to_range(const std::string &name, const ConfigStore::Value &value,
                   const StaticModels::Elements::Range *ctrl,
                   const nlohmann::json &mapping,
                   const ConfigStore::ValueType target_type)
{
    if(ctrl == nullptr)
    {
        msg_error(0, LOG_NOTICE,
                  "Value mapping type \"to_range\" currently only works "
                  "with ranges (control %s)", name.c_str());
        return std::make_pair(nlohmann::json(), AddResult::IGNORED);
    }

    const auto output_range(std::make_pair(std::cref(mapping.at("from")),
                                           std::cref(mapping.at("to"))));
    auto ratio = std::numeric_limits<double>::infinity();

    switch(ctrl->get_value_type())
    {
      case ConfigStore::ValueType::VT_VOID:
      case ConfigStore::ValueType::VT_ASCIIZ:
      case ConfigStore::ValueType::VT_BOOL:
      case ConfigStore::ValueType::VT_TA_FIX_POINT:
        break;

      case ConfigStore::ValueType::VT_INT8:
        ratio = compute_value_ratio<ConfigStore::ValueType::VT_INT8>(
                    value.get_value(), ctrl->get_min(), ctrl->get_max());
        break;

      case ConfigStore::ValueType::VT_INT16:
        ratio = compute_value_ratio<ConfigStore::ValueType::VT_INT16>(
                    value.get_value(), ctrl->get_min(), ctrl->get_max());
        break;

      case ConfigStore::ValueType::VT_INT32:
        ratio = compute_value_ratio<ConfigStore::ValueType::VT_INT32>(
                    value.get_value(), ctrl->get_min(), ctrl->get_max());
        break;

      case ConfigStore::ValueType::VT_INT64:
        ratio = compute_value_ratio<ConfigStore::ValueType::VT_INT64>(
                    value.get_value(), ctrl->get_min(), ctrl->get_max());
        break;

      case ConfigStore::ValueType::VT_UINT8:
        ratio = compute_value_ratio<ConfigStore::ValueType::VT_UINT8>(
                    value.get_value(), ctrl->get_min(), ctrl->get_max());
        break;

      case ConfigStore::ValueType::VT_UINT16:
        ratio = compute_value_ratio<ConfigStore::ValueType::VT_UINT16>(
                    value.get_value(), ctrl->get_min(), ctrl->get_max());
        break;

      case ConfigStore::ValueType::VT_UINT32:
        ratio = compute_value_ratio<ConfigStore::ValueType::VT_UINT32>(
                    value.get_value(), ctrl->get_min(), ctrl->get_max());
        break;

      case ConfigStore::ValueType::VT_UINT64:
        ratio = compute_value_ratio<ConfigStore::ValueType::VT_UINT64>(
                    value.get_value(), ctrl->get_min(), ctrl->get_max());
        break;

      case ConfigStore::ValueType::VT_DOUBLE:
        ratio = compute_value_ratio<ConfigStore::ValueType::VT_DOUBLE>(
                    value.get_value(), ctrl->get_min(), ctrl->get_max());
        break;
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    if(ratio == std::numeric_limits<double>::infinity())
#pragma GCC diagnostic pop
    {
        msg_error(0, LOG_NOTICE,
                  "Unsupported input mapping type \"%c\" for control %s",
                  ConfigStore::Value::type_to_type_code(ctrl->get_value_type()),
                  name.c_str());
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
              "Unsupported target mapping type \"%c\" for control %s",
              ConfigStore::Value::type_to_type_code(target_type),
              name.c_str());
    return std::make_pair(nlohmann::json(), AddResult::IGNORED);
}

static std::pair<nlohmann::json, AddResult>
map_value_primitive(const std::string &name, const ConfigStore::Value &value,
                    const StaticModels::Elements::Control &ctrl,
                    const nlohmann::json &mapping, const nlohmann::json &mapping_type,
                    ConfigStore::ValueType target_type)
{
    if(mapping_type == "direct")
        return std::make_pair(value.get_as(target_type), AddResult::ADDED);

    if(mapping_type == "to_range")
        return
            map_value_to_range(
                name, value,
                dynamic_cast<const StaticModels::Elements::Range *>(&ctrl),
                mapping, target_type);

    if(mapping_type == "suppress")
        return std::make_pair(nlohmann::json(), AddResult::IGNORED);

    msg_error(0, LOG_NOTICE,
              "Unknown value mapping type \"%s\" for control %s",
              mapping_type.get<std::string>().c_str(), name.c_str());
    return std::make_pair(nlohmann::json(), AddResult::IGNORED);
}

static std::pair<nlohmann::json, AddResult>
map_value(const ConfigStore::DeviceContext &dev,
          const std::string &name, const ConfigStore::Value &value,
          const StaticModels::Elements::Control &ctrl,
          const nlohmann::json &mapping)
{
    const auto &mapping_type = mapping.at("type");
    const auto &target_type_code = mapping.at("value_type").get<std::string>();
    const auto target_type = ConfigStore::Value::type_code_to_type(target_type_code);

    if(mapping_type != "select")
        return map_value_primitive(name, value, ctrl, mapping,
                                   mapping_type, target_type);

    /* type of mapping depends on some control's value, so we match the current
     * value of the specified control against the given table of mappings */
    const auto spec(StaticModels::Utils::split_mapping_spec(
                        mapping.at("select").get<std::string>()));
    const StaticModels::Elements::Control *const selector_control =
        dev.get_model()->get_control_by_name(std::get<0>(spec), std::get<1>(spec));
    const ConfigStore::Value *const selector_value =
        dev.get_control_value(std::get<0>(spec), std::get<1>(spec));

    const unsigned int sel_index =
        selector_control != nullptr && selector_value != nullptr
        ? selector_control->to_selector_index(*selector_value)
        : std::numeric_limits<unsigned int>::max();

    if(sel_index != std::numeric_limits<unsigned int>::max())
    {
        const auto &mapping_table(mapping.at("mapping_table"));
        const auto &it(mapping_table.find(
                        selector_control->index_to_choice_string(sel_index)));

        if(it != mapping_table.end())
            return map_value_primitive(name, value, ctrl, mapping,
                                       it->at("type"), target_type);
    }

    return std::make_pair(nlohmann::json(), AddResult::IGNORED);
}

static AddResult
process_value_entry(const ConfigStore::DeviceContext &dev,
                    const std::string &name, const ConfigStore::Value &value,
                    const StaticModels::Elements::Control &ctrl,
                    const nlohmann::json &roon_conversion,
                    const ProcessValueFn &process_fn)
{
    const auto vm(roon_conversion.find("value_mapping"));
    const auto vn(roon_conversion.find("value_name"));

    if(vm == roon_conversion.end() || vn == roon_conversion.end())
    {
        if(vm == roon_conversion.end() && vn == roon_conversion.end())
            msg_error(0, LOG_NOTICE,
                      "Need \"value_name\" and \"value_mapping\" for control %s",
                      name.c_str());

        process_fn(nullptr, nullptr, ctrl);
        return AddResult::IGNORED;
    }
    else if(!vn->is_string())
    {
        msg_error(0, LOG_NOTICE,
                  "The \"value_name\" must be a string in control %s",
                  name.c_str());
        process_fn(nullptr, nullptr, ctrl);
        return AddResult::IGNORED;
    }

    if(ctrl.is_neutral_value(value))
    {
        process_fn(nullptr, nullptr, ctrl);
        return AddResult::NEUTRAL;
    }

    auto mapped(map_value(dev, name, value, ctrl, *vm));

    switch(mapped.second)
    {
      case AddResult::IGNORED:
        process_fn(nullptr, nullptr, ctrl);
        return mapped.second;

      case AddResult::NEUTRAL:
        BUG("%s(%d): case shouldn't occur", __func__, __LINE__);
        process_fn(nullptr, nullptr, ctrl);
        return mapped.second;

      case AddResult::ADDED:
        break;
    }

    const auto &key(vn->get<std::string>());
    process_fn(std::move(mapped.first), &key, ctrl);
    return AddResult::ADDED;
}

static AddResult
process_entry(const ConfigStore::DeviceContext &dev,
              const std::string &name, const ConfigStore::Value &value,
              const StaticModels::Elements::Control &ctrl,
              const nlohmann::json &roon_conversion,
              const ProcessValueFn &process_fn)
{
    if(const auto *on_off = dynamic_cast<const StaticModels::Elements::OnOff *>(&ctrl))
        return process_flag_entry(value, *on_off, process_fn);
    else
        return process_value_entry(dev, name, value, ctrl,
                                   roon_conversion, process_fn);
}

using RankedEntries =
    std::map<unsigned int,
             std::pair<nlohmann::json, const StaticModels::Elements::Control *>>;

static bool add_entry_for_name(const ConfigStore::DeviceContext &dev,
                               const std::string &name,
                               const ConfigStore::Value &value,
                               const StaticModels::Elements::Control &ctrl,
                               RankedEntries &ranked_entries)
{
    const auto it(ctrl.original_definition_.find("roon"));
    if(it == ctrl.original_definition_.end())
        return false;

    const auto rank = StaticModels::Utils::get<unsigned int>(*it, "rank", 0);

    if(ranked_entries.find(rank) != ranked_entries.end())
    {
        msg_error(0, LOG_NOTICE,
                  "Duplicate Roon rank %u for element control \"%s\"",
                  rank, name.c_str());
        return false;
    }

    try
    {
        switch(process_entry(dev, name, value, ctrl, *it,
                [&it, &ranked_entries, &rank]
                (nlohmann::json &&v, const std::string *key, const auto &c)
                {
                    if(v != nullptr)
                    {
                        nlohmann::json t = it->at("template");

                        if(key != nullptr)
                            t[*key] = std::move(v);

                        ranked_entries.emplace(rank, std::make_pair(std::move(t), &c));
                    }
                    else
                        ranked_entries.emplace(rank, std::make_pair(std::move(v), &c));
                }))
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

static bool patch_entry_for_name(const ConfigStore::DeviceContext &dev,
                                 const std::string &name,
                                 const ConfigStore::Value &value,
                                 const StaticModels::Elements::Control &ctrl,
                                 nlohmann::json &entry)
{
    const auto it(ctrl.original_definition_.find("roon"));
    if(it == ctrl.original_definition_.end())
        return false;

    try
    {
        switch(process_entry(dev, name, value, ctrl, *it,
                [&it, &entry]
                (nlohmann::json &&v, const std::string *key, const auto &c)
                {
                    if(v != nullptr)
                    {
                        nlohmann::json t = it->at("template");

                        if(key != nullptr)
                            t[*key] = std::move(v);

                        entry = std::move(t);
                    }
                    else
                        entry = std::move(v);
                }))
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
                  "Failed patching entry for Roon for element control \"%s\"",
                  name.c_str());
    }

    return false;
}

static const std::pair<uint16_t, std::string> *determine_path_rank_and_output_method(
        const ModelCompliant::SignalPathTracker::ActivePath &p,
        const StaticModels::DeviceModel *device_model,
        std::unordered_map<const StaticModels::Elements::AudioSink *,
                           std::pair<uint16_t, std::string>> &ranks)
{
    const auto *sink = device_model->get_audio_sink(p.back().first->get_name());
    const auto &r(ranks.find(sink));
    if(r != ranks.end())
        return &r->second;

    if(sink == nullptr)
    {
        ranks.emplace(sink, std::make_pair(ClientPlugin::Roon::Cache::INVALID_RANK, ""));
        return nullptr;
    }

    const auto it(sink->original_definition_.find("roon"));
    if(it == sink->original_definition_.end())
    {
        ranks.emplace(sink, std::make_pair(ClientPlugin::Roon::Cache::INVALID_RANK, ""));
        return nullptr;
    }

    auto rank(StaticModels::Utils::get<uint16_t>(*it, "rank",
                        uint16_t(ClientPlugin::Roon::Cache::INVALID_RANK)));
    auto output_method(StaticModels::Utils::get<std::string>(*it, "method", ""));

    if(output_method.empty())
        BUG("Roon output method undefined for sink %s in model for %s",
            sink->id_.c_str(), device_model->name_.c_str());

    return &ranks.emplace(sink, std::make_pair(rank, std::move(output_method)))
                .first->second;
}

static nlohmann::json
compute_sorted_result(const ConfigStore::DeviceContext &dev,
                      const StaticModels::DeviceModel *device_model,
                      ClientPlugin::Roon::Cache &cache,
                      std::unordered_map<const StaticModels::Elements::AudioSink *,
                                         std::pair<uint16_t, std::string>> &ranks)
{
    if(device_model == nullptr)
        return nullptr;

    nlohmann::json output;

    dev.for_each_signal_path(true,
        [&device_model, &dev, &cache, &ranks, &output]
        (const auto &active_path)
        {
            if(!cache.put_path(active_path,
                               determine_path_rank_and_output_method(
                                    active_path, device_model, ranks)))
                return true;

            for(const auto &elem : active_path)
            {
                RankedEntries ranked_entries;

                dev.for_each_setting(elem.first->get_name(),
                    [&device_model, &dev, &ranked_entries]
                    (const std::string &element_name, const std::string &value_name,
                     const ConfigStore::Value &value)
                    {
                        const auto *ctrl =
                            device_model->get_control_by_name(element_name, value_name);
                        if(ctrl == nullptr)
                            return true;

                        const auto name("self." + element_name + '.' + value_name);
                        add_entry_for_name(dev, name, value, *ctrl, ranked_entries);
                        return true;
                    });

                for(auto &it : ranked_entries)
                {
                    if(it.second.first != nullptr)
                        output.push_back(it.second.first);

                    auto name(elem.first->get_name() + '.' + it.second.second->id_);
                    cache.append_fragment(std::move(name), std::move(it.second));
                }
            }

            nlohmann::json output_method =
            {
                { "type", "output" },
                { "quality", "lossless" },
                { "method", cache.get_output_method_id() },
            };
            output.emplace_back(std::move(output_method));

            return true;
        });

    return output;
}

static bool is_root_appliance_unchanged(const ConfigStore::Changes &changes)
{
    bool unchanged = true;

    changes.for_each_changed_device(
        [&unchanged]
        (const std::string &device_name, bool was_added)
        {
            if(device_name == "self")
                unchanged = false;
        });

    return unchanged;
}

static bool is_signal_path_topology_unchanged(
        const ConfigStore::Settings &settings,
        const ModelCompliant::SignalPathTracker::ActivePath &path)
{
    try
    {
        const ConfigStore::SettingsIterator si(settings);
        const auto &dev(si.with_device("self"));
        bool unchanged = false;

        dev.for_each_signal_path(true,
            [&path, &unchanged]
            (const auto &active_path)
            {
                unchanged = active_path == path;
                return false;
            });

        return unchanged;
    }
    catch(const std::out_of_range &e)
    {
        return false;
    }
}

void ClientPlugin::Roon::report_changes(const ConfigStore::Settings &settings,
                                        const ConfigStore::Changes &changes) const
{
    bool cache_cleared = false;

    if(!is_root_appliance_unchanged(changes) ||
       !is_signal_path_topology_unchanged(settings, cache_.get_path()))
    {
        /* structural changes ahead, need to wipe out cached data */
        cache_cleared = cache_.clear();
    }

    std::string report;
    std::vector<std::string> extra;

    if(cache_.empty())
    {
        /* no signal path seen before, so try to compute afresh */
        full_report(settings, report, extra);

        if(!cache_.empty() || cache_cleared)
            emit_audio_signal_path_fn_(report, extra);

        return;
    }

    /* have information about previously seen, unchanged signal path, so just
     * patch the changed values */
    try
    {
        const ConfigStore::SettingsIterator si(settings);
        const auto &dev(si.with_device("self"));

        changes.for_each_changed_value(
            [this, &dev]
            (const auto &name, const auto &old_value, const auto &new_value)
            {
                const auto &device_and_qualified_control(
                    StaticModels::Utils::split_qualified_name(name));
                if(std::get<0>(device_and_qualified_control) != "self")
                    return;

                if(new_value.is_of_type(ConfigStore::ValueType::VT_VOID))
                    cache_.update_fragment(std::get<1>(device_and_qualified_control),
                                           nullptr);
                else
                {
                    auto &entry(cache_.lookup_fragment(
                                    std::get<1>(device_and_qualified_control)));

                    patch_entry_for_name(dev, name, new_value,
                                         *entry.second, entry.first);
                }
           });
    }
    catch(const std::out_of_range &e)
    {
        BUG("Failed iterating device \"self\": %s", e.what());
    }

    if(!report.empty())
        emit_audio_signal_path_fn_(report, extra);
}

bool ClientPlugin::Roon::full_report(const ConfigStore::Settings &settings,
                                     std::string &report,
                                     std::vector<std::string> &extra) const
{
    const ConfigStore::SettingsIterator si(settings);
    nlohmann::json output;

    try
    {
        const auto &dev(si.with_device("self"));
        cache_.clear();
        output = compute_sorted_result(dev, dev.get_model(), cache_, ranks_);
    }
    catch(const std::out_of_range &e)
    {
        /* have no data yet, but that's OK */
        cache_.clear();
    }

    if(output != nullptr)
        report = output.dump();
    else
        report = "[]";

    return true;
}
