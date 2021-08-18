/*
 * Copyright (C) 2019, 2021  T+A elektroakustik GmbH & Co. KG
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

class Cache
{
  public:
    static constexpr auto INVALID_RANK = std::numeric_limits<uint16_t>::max();

  private:
    ModelCompliant::CompoundSignalPath path_;
    uint16_t path_rank_;
    std::string path_output_method_;
    std::vector<std::pair<nlohmann::json,
                const StaticModels::Elements::Control *>> reported_fragments_;
    std::unordered_map<std::string, unsigned int> elem_to_frag_index_;

  public:
    Cache(const Cache &) = delete;
    Cache(Cache &&) = default;
    Cache &operator=(const Cache &) = delete;
    Cache &operator=(Cache &&) = default;

    explicit Cache():
        path_rank_(INVALID_RANK)
    {}

    bool clear()
    {
        const bool result = !path_.empty();
        path_.clear();
        path_rank_ = INVALID_RANK;
        path_output_method_.clear();
        reported_fragments_.clear();
        elem_to_frag_index_.clear();
        return result;
    }

    bool empty() const { return path_.empty(); }

    bool put_path(const ModelCompliant::CompoundSignalPathTracker &spt,
                  const ModelCompliant::CompoundSignalPath &path,
                  const std::pair<uint16_t, std::string> *path_rank_and_method)
    {
        if(path_rank_and_method == nullptr)
            return false;

        const auto &rm(*path_rank_and_method);

        if(rm.first >= path_rank_)
        {
            if(rm.first == path_rank_ && rm.first != INVALID_RANK)
                msg_error(0, LOG_WARNING,
                          "There are multiple equally ranked signal paths "
                          "(reporting only one of them to Roon)");

            return false;
        }

        path_ = spt.mk_self_contained_path(path);
        path_rank_ = rm.first;
        path_output_method_ = get_checked_output_method(rm.second);
        reported_fragments_.clear();
        elem_to_frag_index_.clear();
        return true;
    }

    const auto &get_path() const { return path_; }

    const std::string &get_output_method_id() const { return path_output_method_; }

    bool contains(const std::string &element_name) const
    {
        return elem_to_frag_index_.find(element_name) != elem_to_frag_index_.end();
    }

    void append_fragment(std::string &&element_name,
                         std::pair<nlohmann::json,
                         const StaticModels::Elements::Control *> &&fragment)
    {
        elem_to_frag_index_[std::move(element_name)] = reported_fragments_.size();
        reported_fragments_.emplace_back(std::move(fragment));
    }

    void update_fragment(const std::string &element_name,
                         nlohmann::json &&fragment)
    {
        reported_fragments_.at(elem_to_frag_index_.at(element_name)).first =
            std::move(fragment);
    }

    std::pair<nlohmann::json, const StaticModels::Elements::Control *> &
    lookup_fragment(const std::string &element_name)
    {
        return reported_fragments_.at(elem_to_frag_index_.at(element_name));
    }

    nlohmann::json collect_fragments() const
    {
        auto result = nlohmann::json::array();

        for(const auto &it : reported_fragments_)
            if(it.first != nullptr)
                result.push_back(it.first);

        return result;
    }

  private:
    static const std::string &get_checked_output_method(const std::string &method)
    {
        static const std::set<std::string> valid_methods
        {
            "aes", "alsa", "analog", "analog_digital", "asio",
            "digital", "headphones", "i2s", "other", "speakers", "usb",
        };

        if(valid_methods.find(method) != valid_methods.end())
            return method;

        BUG("Invalid Roon output method \"%s\" in audio path sink "
            "(replaced by \"other\")", method.c_str());

        static const std::string fallback("other");
        return fallback;
    };
};

const uint16_t Cache::INVALID_RANK;

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

enum class MappingType
{
    INVALID,
    SUPPRESS,
    DIRECT,
    TO_RANGE,
    SELECT,
};

static MappingType mapping_type_name_to_mapping_type(const std::string &mname,
                                                     const std::string &cname)
{
    static const std::unordered_map<std::string, MappingType> tab
    {
        { "suppress", MappingType::SUPPRESS },
        { "direct", MappingType::DIRECT },
        { "to_range", MappingType::TO_RANGE },
        { "select", MappingType::SELECT },
    };

    try
    {
        return tab.at(mname);
    }
    catch(...)
    {
        msg_error(0, LOG_NOTICE,
                  "Unknown value mapping type \"%s\" for control %s",
                  mname.c_str(), cname.c_str());
        return MappingType::INVALID;
    }
}

static std::pair<nlohmann::json, AddResult>
map_value_primitive(const std::string &name, const ConfigStore::Value &value,
                    const StaticModels::Elements::Control &ctrl,
                    const nlohmann::json &mapping, MappingType mapping_type,
                    ConfigStore::ValueType target_type)
{
    switch(mapping_type)
    {
      case MappingType::DIRECT:
        return std::make_pair(value.get_as(target_type), AddResult::ADDED);

      case MappingType::TO_RANGE:
        return
            map_value_to_range(
                name, value,
                dynamic_cast<const StaticModels::Elements::Range *>(&ctrl),
                mapping, target_type);

      case MappingType::SUPPRESS:
        return std::make_pair(nlohmann::json(), AddResult::IGNORED);

      case MappingType::INVALID:
      case MappingType::SELECT:
        break;
    }

    return std::make_pair(nlohmann::json(), AddResult::IGNORED);
}

static std::pair<nlohmann::json, AddResult>
map_value(const ConfigStore::DeviceContext &dev,
          const std::string &name, const ConfigStore::Value &value,
          const StaticModels::Elements::Control &ctrl,
          const nlohmann::json &mapping)
{
    const auto &mapping_type_name = mapping.at("type");
    const auto &target_type_code = mapping.at("value_type").get<std::string>();
    const auto target_type = ConfigStore::Value::type_code_to_type(target_type_code);
    const auto mapping_type =
        mapping_type_name_to_mapping_type(mapping_type_name, name);

    switch(mapping_type)
    {
      case MappingType::INVALID:
      case MappingType::SUPPRESS:
      case MappingType::DIRECT:
      case MappingType::TO_RANGE:
        return map_value_primitive(name, value, ctrl, mapping,
                                   mapping_type, target_type);

      case MappingType::SELECT:
        break;
    }

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
        {
            const auto value_mapping_type =
                mapping_type_name_to_mapping_type(it->at("type"), name);
            return map_value_primitive(name, value, ctrl, mapping,
                                       value_mapping_type, target_type);
        }
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

using RankedControls =
    std::map<unsigned int,
             std::pair<nlohmann::json, const StaticModels::Elements::Control *>>;

static void add_empty_ranked_entry(const StaticModels::Elements::Internal &elem,
                                   const StaticModels::Elements::Control &ctrl,
                                   RankedControls &ranked_controls)
{
    const auto it(ctrl.original_definition_.find("roon"));
    if(it == ctrl.original_definition_.end())
        return;

    const auto rank = StaticModels::Utils::get<unsigned int>(*it, "rank", 0);

    if(ranked_controls.find(rank) == ranked_controls.end())
        ranked_controls.emplace(rank, std::make_pair(nlohmann::json(), &ctrl));
    else
    {
        const auto name("self." + elem.id_ + '.' + ctrl.id_);
        msg_error(0, LOG_NOTICE,
                  "Duplicate Roon rank %u for element control \"%s\"",
                  rank, name.c_str());
    }
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

static const std::pair<uint16_t, std::string> *
determine_path_rank_and_output_method(
        const ModelCompliant::CompoundSignalPathTracker &spt,
        const ModelCompliant::CompoundSignalPath &p,
        std::unordered_map<const StaticModels::Elements::AudioSink *,
                           std::pair<uint16_t, std::string>> &ranks)
{
    const auto &dev(spt.settings_iterator_.with_device(spt.map_path_index_to_device_name(p.back().first)));
    const auto *sink = dev.get_model()->get_audio_sink(p.back().second->get_name());
    const auto &r(ranks.find(sink));
    if(r != ranks.end())
        return &r->second;

    if(sink == nullptr)
    {
        ranks.emplace(sink, std::make_pair(Cache::INVALID_RANK, ""));
        return nullptr;
    }

    const auto it(sink->original_definition_.find("roon"));
    if(it == sink->original_definition_.end())
    {
        ranks.emplace(sink, std::make_pair(Cache::INVALID_RANK, ""));
        return nullptr;
    }

    auto rank(StaticModels::Utils::get<uint16_t>(*it, "rank",
                                                 uint16_t(Cache::INVALID_RANK)));
    auto output_method(StaticModels::Utils::get<std::string>(*it, "method", ""));

    if(output_method.empty())
        BUG("Roon output method undefined for sink %s in model for %s",
            sink->id_.c_str(), dev.get_model()->name_.c_str());

    return &ranks.emplace(sink, std::make_pair(rank, std::move(output_method)))
                .first->second;
}

static RankedControls
collect_ranked_controls(const StaticModels::DeviceModel &device_model,
                        const std::string &element_name)
{
    RankedControls ranked_controls;

    const auto *elem = device_model.lookup_internal_element(element_name);
    if(elem  == nullptr)
        return ranked_controls;

    elem->for_each_control(
        [elem, &ranked_controls]
        (const StaticModels::Elements::Control &ctrl)
        {
            add_empty_ranked_entry(*elem, ctrl, ranked_controls);
        });

    return ranked_controls;
}

static void fill_cache_with_values_from_device_context(
        const ConfigStore::SettingsIterator &settings_iter, Cache &cache)
{
    /* fill in the values we have in the current device context, leave the rest
     * of the values the way they are */
    for(const auto &elem : cache.get_path())
    {
        const auto &device_instance_name(cache.get_path().map_path_index_to_device_name(elem.first));
        const auto &dev(settings_iter.with_device(device_instance_name));
        const auto &device_model = *dev.get_model();

        dev.for_each_setting(elem.second->get_name(),
            [&dev, &device_model, &device_instance_name, &cache]
            (const std::string &element_name, const std::string &value_name,
             const ConfigStore::Value &value)
            {
                const auto *ctrl =
                    device_model.get_control_by_name(element_name, value_name);
                if(ctrl == nullptr)
                    return true;

                const auto name(device_instance_name + '.' +
                                element_name + '.' + value_name);

                try
                {
                    auto &entry(cache.lookup_fragment(name));
                    log_assert(entry.second == ctrl);
                    patch_entry_for_name(dev, name, value,
                                         *entry.second, entry.first);
                }
                catch(const std::out_of_range &)
                {
                    /* ignore non-existent name */
                }

                return true;
            });
    }
}

static nlohmann::json generate_report_from_cache(const Cache &cache)
{
    auto output = cache.collect_fragments();

    nlohmann::json output_method =
    {
        { "type", "output" },
        { "quality", "lossless" },
        { "method", cache.get_output_method_id() },
    };
    output.emplace_back(std::move(output_method));

    return output;
}

static nlohmann::json
compute_sorted_result(const ConfigStore::SettingsIterator &settings_iter,
                      const std::string &root_device_instance_name,
                      Cache &cache,
                      std::unordered_map<const StaticModels::Elements::AudioSink *,
                                         std::pair<uint16_t, std::string>> &ranks)
{
    ModelCompliant::CompoundSignalPathTracker spt(settings_iter);

    /* find active path with highest rank */
    spt.enumerate_compound_signal_paths(
        root_device_instance_name,
        [&spt, &cache, &ranks]
        (const auto &active_path)
        {
            cache.put_path(spt, active_path,
                           determine_path_rank_and_output_method(spt, active_path,
                                                                 ranks));
            return true;
        });

    if(cache.empty())
        return nullptr;

    /* preset cache to empty JSON objects for all the values along the path
     * according to element positions and control ranks */
    for(const auto &elem : cache.get_path())
    {
        const auto &device_instance_name(cache.get_path().map_path_index_to_device_name(elem.first));
        const auto &dev(settings_iter.with_device(device_instance_name));
        auto ranked_entries(collect_ranked_controls(*dev.get_model(),
                                                    elem.second->get_name()));

        for(auto &it : ranked_entries)
        {
            auto name(device_instance_name + '.' +
                      elem.second->get_name() + '.' + it.second.second->id_);
            cache.append_fragment(std::move(name), std::move(it.second));
        }
    }

    fill_cache_with_values_from_device_context(settings_iter, cache);

    return generate_report_from_cache(cache);
}

void ClientPlugin::Roon::report_changes(const ConfigStore::Settings &settings,
                                        const ConfigStore::Changes &changes) const
{
    std::string report;
    std::vector<std::string> extra;

    if(full_report(settings, report, extra))
        emit_audio_signal_path_fn_(report, extra);
}

bool ClientPlugin::Roon::full_report(const ConfigStore::Settings &settings,
                                     std::string &report,
                                     std::vector<std::string> &extra) const
{
    const ConfigStore::SettingsIterator si(settings);
    nlohmann::json output;
    Cache cache;

    try
    {
        output = compute_sorted_result(si, "self", cache, ranks_);
    }
    catch(const std::out_of_range &e)
    {
        /* have no data yet, but that's OK */
    }

    if(output == previous_roon_report_)
    {
        report.clear();
        return false;
    }

    previous_roon_report_ =
        output != nullptr ? output : nlohmann::json::array();
    report = previous_roon_report_.dump();

    return true;
}
