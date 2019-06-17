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

#include "device_models.hh"
#include "model_parsing_utils.hh"
#include "model_parsing_utils_json.hh"
#include "messages.h"

#include <fstream>

static bool do_load(std::ifstream &in, bool suppress_error,
                    nlohmann::json &config_data,
                    const std::function<void(const char *)> &emit_error)
{
    if(!in)
    {
        if(!suppress_error)
            emit_error("Failed reading models configuration file \"%s\"");

        return false;
    }

    try
    {
        in >> config_data;
        return true;
    }
    catch(const std::exception &e)
    {
        msg_error(0, LOG_NOTICE, "%s", e.what());
        config_data = nlohmann::json();
        return false;
    }
}

bool StaticModels::DeviceModelsDatabase::load(const std::string &config,
                                              bool suppress_error)
{
    std::ifstream in(config);
    return do_load(in, suppress_error, config_data_,
                   [&config] (const char *msg)
                   { msg_error(0, LOG_ERR, msg, config.c_str()); });
}

bool StaticModels::DeviceModelsDatabase::load(const char *config,
                                              bool suppress_error)
{
    std::ifstream in(config);
    return do_load(in, suppress_error, config_data_,
                   [&config] (const char *msg)
                   { msg_error(0, LOG_ERR, msg, config); });
}

const nlohmann::json &
StaticModels::DeviceModelsDatabase::get_device_model_definition(const std::string &device_id) const
{
    try
    {
        return config_data_.at("all_devices").at(device_id);
    }
    catch(...)
    {
        static const nlohmann::json empty;
        return empty;
    }
}

using DefinedControls =
    std::unordered_map<std::string, std::unique_ptr<StaticModels::Elements::Control>>;

static DefinedControls parse_controls(const nlohmann::json &elem)
{
    DefinedControls result;

    const auto &it(elem.find("controls"));
    if(it == elem.end())
        return result;

    for(const auto &control : it->items())
    {
        const auto &ctrltype(control.value().at("type").get<std::string>());
        const auto &val(control.value());

        auto label(StaticModels::Utils::get<std::string>(val, "label", ""));
        auto desc(StaticModels::Utils::get<std::string>(val, "description", ""));

        if(ctrltype == "choice")
        {
            const auto &ch(val.at("choices"));
            result.emplace(
                control.key(),
                std::make_unique<StaticModels::Elements::Choice>(
                    val, std::string(control.key()),
                    std::move(label), std::move(desc),
                    std::vector<std::string>(ch.begin(), ch.end()),
                    StaticModels::Utils::get<std::string>(val, "neutral_setting", "")));
        }
        else if(ctrltype == "range")
        {
            const auto &vtype(val.at("value_type").get<std::string>());
            auto neutral_setting(val.contains("neutral_setting")
                    ? ConfigStore::Value(vtype, nlohmann::json(val["neutral_setting"]))
                    : ConfigStore::Value());

            result.emplace(
                control.key(),
                std::make_unique<StaticModels::Elements::Range>(
                    val, std::string(control.key()),
                    std::move(label), std::move(desc),
                    val.at("scale").get<std::string>(),
                    ConfigStore::Value(vtype, nlohmann::json(val.at("min"))),
                    ConfigStore::Value(vtype, nlohmann::json(val.at("max"))),
                    ConfigStore::Value(vtype, nlohmann::json(val.at("step"))),
                    std::move(neutral_setting)));
        }
        else if(ctrltype == "on_off")
            result.emplace(
                control.key(),
                std::make_unique<StaticModels::Elements::OnOff>(
                    val, std::string(control.key()),
                    std::move(label), std::move(desc),
                    StaticModels::Utils::get<std::string>(val, "neutral_setting", "off")));
        else
            Error() << "Invalid control type \"" << ctrltype << "\"";
    }

    return result;
}

using DefinedElements =
    std::unordered_map<std::string, std::unique_ptr<StaticModels::Elements::Element>>;

/*!
 * Parse all audio path elements (sources, sinks, and internal).
 *
 * These elements are read from the "audio_sources", "audio_sinks", and
 * "elements" arrays defined in the given model. All of these are expected to
 * exist.
 */
static DefinedElements parse_elements(const nlohmann::json &model)
{
    DefinedElements elements;

    try
    {
        for(const auto &src : model.at("audio_sources"))
        {
            auto src_obj =
                std::make_unique<StaticModels::Elements::AudioSource>(
                    src, std::string(src.at("id").get<std::string>()),
                    StaticModels::Utils::get<std::string>(src, "description", ""));
            const auto &key(src_obj->id_);
            elements.emplace(key, std::move(src_obj));
        }

        for(const auto &src : model.at("audio_sources"))
        {
            const auto &parent_id(StaticModels::Utils::get<std::string>(src, "parent", ""));
            if(parent_id.empty())
                continue;

            try
            {
                const auto *const parent =
                    dynamic_cast<StaticModels::Elements::AudioSource *>(elements.at(parent_id).get());
                if(parent != nullptr)
                {
                    if(static_cast<StaticModels::Elements::AudioSource *>(
                        elements[src.at("id").get<std::string>()].get())
                        ->set_parent_source(*parent))
                        continue;

                    Error()
                        << "Duplicate parent source definition for audio source \""
                        << src << "\"";
                }
            }
            catch(const std::out_of_range &e)
            {
                /* handled below */
            }

            Error()
                << "Audio source \"" << parent_id
                << "\" does not exist, but is specified as parent of \""
                << src << "\"";
        }
    }
    catch(const std::exception &e)
    {
        msg_error(0, LOG_NOTICE, "%s", e.what());
        throw;
    }

    try
    {
        for(const auto &sink : model.at("audio_sinks"))
        {
            auto sink_obj =
                std::make_unique<StaticModels::Elements::AudioSink>(
                    sink, std::string(sink.at("id").get<std::string>()),
                    StaticModels::Utils::get<std::string>(sink, "description", ""));
            const auto &key(sink_obj->id_);
            elements.emplace(key, std::move(sink_obj));
        }
    }
    catch(const std::exception &e)
    {
        msg_error(0, LOG_NOTICE, "%s", e.what());
        throw;
    }

    try
    {
        for(const auto &elem : model.at("elements"))
        {
            const auto &e(elem.at("element"));
            auto controls(parse_controls(e));
            auto elem_obj =
                std::make_unique<StaticModels::Elements::Internal>(
                    elem, std::string(elem.at("id").get<std::string>()),
                    StaticModels::Utils::get<std::string>(e, "description", ""),
                    StaticModels::Utils::get<unsigned int>(e, "stereo_inputs", 1),
                    StaticModels::Utils::get<unsigned int>(e, "stereo_outputs", 1),
                    std::move(controls));
            const auto &key(elem_obj->id_);
            elements.emplace(key, std::move(elem_obj));
        }
    }
    catch(const std::exception &e)
    {
        msg_error(0, LOG_NOTICE, "%s", e.what());
        throw;
    }

    return elements;
}

using DefinedIOMappings =
    std::unordered_map<std::string, std::pair<const nlohmann::json &, std::string>>;

/*!
 * Parse all I/O mappings defined for the given model.
 *
 * The mappings are read from the objects in the "audio_signal_paths" array
 * which contain an "io_mapping" object.
 *
 * This function also makes sure that the objects in "audio_signal_paths"
 * contain either an "io_mapping" or a "connections" object (otherwise it
 * throws).
 */
static DefinedIOMappings
get_io_mappings_from_model(const nlohmann::json &model,
                           const DefinedElements &defined_elements,
                           const std::string &device_name)
{
    try
    {
        DefinedIOMappings mappings;

        for(const auto &sp_block : model.at("audio_signal_paths"))
        {
            const auto it(sp_block.find("io_mapping"));
            if(it == sp_block.end())
                continue;

            if(sp_block.contains("connections"))
                Error()
                    << "Found \"connections\" and \"io_mapping\" in same "
                       "\"audio_signal_paths\" entry of device \""
                    << device_name << "\"";

            const auto &sel(it->at("select").get<std::string>());
            auto spec(StaticModels::Utils::split_mapping_spec(sel));

            const auto &elem(defined_elements.find(std::get<0>(spec)));

            if(elem == defined_elements.end())
                Error()
                    << "Use of undefined element \"" << std::get<0>(spec)
                    << "\" in I/O mapping of device \"" << device_name << "\"";

            const auto *ielem =
                dynamic_cast<const StaticModels::Elements::Internal *>(elem->second.get());

            if(ielem == nullptr)
                Error()
                    << "Use of non-switchable element \"" << std::get<0>(spec)
                    << "\" in I/O mapping of device \"" << device_name << "\"";

            if(!ielem->contains_control(std::get<1>(spec)))
                Error()
                    << "Use of undefined control \"" << std::get<0>(spec)
                    << "." << std::get<1>(spec)
                    << "\" in I/O mapping of device \"" << device_name << "\"";

            mappings.emplace(std::move(std::get<0>(spec)),
                             std::make_pair(std::cref(*it),
                                            std::move(std::get<1>(spec))));
        }

        return mappings;
    }
    catch(const std::exception &e)
    {
        msg_error(0, LOG_NOTICE, "%s", e.what());
        throw;
    }
}

static StaticModels::SignalPaths::SwitchingElement
make_mux_element(const StaticModels::Elements::Internal &element,
                 const StaticModels::Elements::Control &selector,
                 const nlohmann::json *mapping_table)
{
    const auto choices = selector.get_number_of_choices();
    std::vector<StaticModels::SignalPaths::Input> m;
    m.reserve(choices);
    for(auto i = 0U; i < choices; ++i)
        m.push_back(StaticModels::SignalPaths::Input(i));

    if(mapping_table != nullptr)
        BUG("TODO: Mapping table present for mux %s, but not supported yet",
            element.id_.c_str());

    if(m.size() != selector.get_number_of_choices())
        Error() << "I/O mapping size " << m.size()
                << " does not match number of choices "
                << selector.get_number_of_choices() << " for selector "
                << selector.id_;

    return
        StaticModels::SignalPaths::SwitchingElement::mk_mux(
            std::string(element.id_), std::string(selector.id_), std::move(m));
}

static StaticModels::SignalPaths::SwitchingElement
make_demux_element(const StaticModels::Elements::Internal &element,
                   const StaticModels::Elements::Control &selector,
                   const nlohmann::json *mapping_table)
{
    const auto choices = selector.get_number_of_choices();
    std::vector<StaticModels::SignalPaths::Output> m;
    m.reserve(choices);
    for(auto i = 0U; i < choices; ++i)
        m.push_back(StaticModels::SignalPaths::Output(i));

    if(mapping_table != nullptr)
        BUG("TODO: Mapping table present for demux %s, but not supported yet",
            element.id_.c_str());

    if(m.size() != selector.get_number_of_choices())
        Error() << "I/O mapping size " << m.size()
                << " does not match number of choices "
                << selector.get_number_of_choices() << " for selector "
                << selector.id_;

    return
        StaticModels::SignalPaths::SwitchingElement::mk_demux(
            std::string(element.id_), std::string(selector.id_), std::move(m));
}

static unsigned int parse_pad_index(const std::string &name, bool is_input,
                                    const std::string &elem)
{
    if(name.empty())
        return 0;

    static const std::string in_prefix  = "in";
    static const std::string out_prefix = "out";
    const std::string &prefix = is_input ? in_prefix : out_prefix;

    if(name.length() > prefix.length() &&
       name.compare(0, prefix.length(), prefix) == 0)
    {
        if(std::all_of(name.begin() + prefix.length(), name.end(), isdigit))
            return std::atoi(name.c_str() + prefix.length());
    }

    Error()
        << "Invalid pad name \"" << name << "\" for element \""
        << elem << "\" (expected something like \"" << prefix << "0\")";
}

static unsigned int parse_pad_index(
        const std::tuple<std::string, std::string> &elem_and_pad,
        bool is_input)
{
    return parse_pad_index(std::get<1>(elem_and_pad), is_input,
                           std::get<0>(elem_and_pad));
}

static void
extend_mapping(std::vector<StaticModels::SignalPaths::MappingTable::Table> &m,
               std::vector<bool> &seen_selector,
               const StaticModels::Elements::Internal &element,
               const nlohmann::json &mapping_table,
               unsigned int selector_index, const std::string &selector_value)
{

    if(seen_selector[selector_index])
        Error()
            << "Duplicate entry for selector choice \"" << selector_value
            << "\" in I/O mapping for element \"" << element.id_ << "\"";

    seen_selector[selector_index] = true;

    try
    {
        const auto &values(mapping_table.at(selector_value));
        StaticModels::SignalPaths::MappingTable::Table &table(m[selector_index]);

        auto it(values.begin());

        while(it != values.end())
        {
            if(!it->is_string())
                Error()
                    << "Input name is not a string for selector choice \""
                    << selector_value << "\" in I/O mapping for element \""
                    << element.id_ << "\"";

            const StaticModels::SignalPaths::Input
                input(parse_pad_index(*it, true, element.id_));

            if(++it == values.end())
                Error()
                    << "Premature end of array for selector choice \""
                    << selector_value << "\" in I/O mapping for element \""
                    << element.id_ << "\"";

            if(it->is_null())
                ++it;
            else if(it->is_string())
            {
                const StaticModels::SignalPaths::Output
                    output(parse_pad_index(*it, false, element.id_));
                ++it;
                table.emplace(input, output);
            }
            else
                Error()
                    << "Input name is not a string for selector choice \""
                    << selector_value << "\" in I/O mapping for element \""
                    << element.id_ << "\"";
        }
    }
    catch(const std::out_of_range &e)
    {
        Error()
            << "I/O mapping for element \"" << element.id_
            << "\" does not define values for selector choice \""
            << selector_value << "\"";
    }
}

static StaticModels::SignalPaths::SwitchingElement
make_table_element(const StaticModels::Elements::Internal &element,
                   const StaticModels::Elements::Control &selector,
                   const nlohmann::json &mapping_table)
{
    if(mapping_table.size() != selector.get_number_of_choices())
        Error()
            << "Size of I/O mapping table for element \"" << element.id_
            << "\" (" << mapping_table.size()
            << ") does not match the number of choices for selector \""
            << selector.id_ << "\" (" << selector.get_number_of_choices()
            << ")";

    std::vector<StaticModels::SignalPaths::MappingTable::Table> m(selector.get_number_of_choices());
    std::vector<bool> seen_selector(selector.get_number_of_choices(), false);

    selector.for_each_choice(
        [&m, &seen_selector, &element, &mapping_table]
        (unsigned int idx, const std::string &choice)
        {
            extend_mapping(m, seen_selector, element, mapping_table, idx, choice);
        });

    return
        StaticModels::SignalPaths::SwitchingElement::mk_table(
            std::string(element.id_), std::string(selector.id_), std::move(m));
}

static StaticModels::SignalPaths::SwitchingElement
make_switching_element(const StaticModels::Elements::Internal &element,
                       const StaticModels::Elements::Control &selector,
                       const nlohmann::json &io_mapping)
{
    const auto &mtype(io_mapping.at("mapping").get<std::string>());
    const auto maybe_table(io_mapping.find("mapping_table"));
    const nlohmann::json *table;

    if(maybe_table == io_mapping.end())
        table = nullptr;
    else
    {
        table = &*maybe_table;
        if(!table->is_object())
            Error() << "Malformed I/O mapping table for element \""
                    << element.id_ << "\"";
    }

    if(mtype == "mux")
        return make_mux_element(element, selector, table);
    else if(mtype == "demux")
        return make_demux_element(element, selector, table);
    else if(mtype == "table")
    {
        if(table == nullptr)
            Error() << "No mapping table given for I/O mapping for element \""
                    << element.id_ << "\"";
        return make_table_element(element, selector, *table);
    }
    else
        Error() << "Invalid I/O mapping type \"" << mtype << "\"";
}

/*!
 * Add all static and switching elements to the appliance builder.
 *
 * The I/O mappings are used to determine the kind of elements.
 */
static void add_elements(StaticModels::SignalPaths::ApplianceBuilder &b,
                         const DefinedElements &defined_elements,
                         const DefinedIOMappings &io_mappings)
{
    for(const auto &elem : defined_elements)
    {
        const auto mapping(io_mappings.find(elem.first));

        if(mapping != io_mappings.end())
        {
            const auto *internal_element =
                dynamic_cast<StaticModels::Elements::Internal *>(elem.second.get());
            log_assert(internal_element != nullptr);

            try
            {
                b.add_element(make_switching_element(
                                *internal_element,
                                internal_element->get_control(mapping->second.second),
                                mapping->second.first));
            }
            catch(const std::out_of_range &)
            {
                Error() << "I/O mapping refers to non-existent control \""
                        << elem.first << "." << mapping->second.second << "\"";
            }
        }
        else
            b.add_element(StaticModels::SignalPaths::StaticElement(std::string(elem.first)));
    }
}

static void add_connection(StaticModels::SignalPaths::ApplianceBuilder &b,
                           const std::string &from_element,
                           StaticModels::SignalPaths::Output output,
                           const std::string &target_spec,
                           const DefinedElements &defined_elements,
                           const std::string &device_name)
{
    auto to_name_and_input(StaticModels::Utils::split_qualified_name(target_spec, true));
    const StaticModels::SignalPaths::Input input(
            parse_pad_index(to_name_and_input, true));
    const auto &to_element(std::get<0>(to_name_and_input));

    if(defined_elements.find(to_element) == defined_elements.end())
        Error()
            << "Undefined target element \"" << to_element
            << "\" in signal path definition of device \""
            << device_name << "\"";

    bool have_from = false;

    try
    {
        auto &from(b.lookup_element(from_element));
        have_from = true;
        auto &to(b.lookup_element(to_element));

        from.connect(output, to, input);
    }
    catch(const std::out_of_range &e)
    {
        Error()
            << (have_from ? "Target" : "Source") << " element \""
            << (have_from ? to_element : from_element) << "\" not defined";
    }
}

/*!
 * Add all explicitly defined audio signal connections between elements to the
 * appliance builder.
 *
 * The connections are read from the objects in the "audio_signal_paths" array
 * which contain a "connections" object.
 */
static void add_explicit_connections(StaticModels::SignalPaths::ApplianceBuilder &b,
                                     const nlohmann::json &model,
                                     const DefinedElements &defined_elements,
                                     const std::string &device_name)
{
    try
    {
        for(const auto &sp_block : model.at("audio_signal_paths"))
        {
            const auto it(sp_block.find("connections"));
            if(it == sp_block.end())
                continue;

            for(const auto &edge_spec : it->items())
            {
                auto from_name_and_output(StaticModels::Utils::split_qualified_name(
                        edge_spec.key(), true));
                const StaticModels::SignalPaths::Output output(
                        parse_pad_index(from_name_and_output, false));
                const auto &from_element(std::get<0>(from_name_and_output));

                if(defined_elements.find(from_element) == defined_elements.end())
                    Error()
                        << "Undefined source element \"" << from_element
                        << "\" in signal path definition of device \""
                        << device_name << "\"";

                if(edge_spec.value().is_string())
                    add_connection(b, from_element, output,
                                   edge_spec.value().get<std::string>(),
                                   defined_elements, device_name);
                else if(edge_spec.value().is_array())
                {
                    for(const auto &target : edge_spec.value())
                        add_connection(b, from_element, output,
                                       target.get<std::string>(),
                                       defined_elements, device_name);
                }
                else
                    Error()
                        << "Invalid connection value in I/O mapping from \""
                        << from_element << "\" for device \""
                        << device_name << "\"";
            }

            /*
            const auto &sel(it->at("select").get<std::string>());
            auto spec(StaticModels::Utils::split_mapping_spec(sel));

            const auto &elem(defined_elements.find(std::get<0>(spec)));

            if(elem == defined_elements.end())
                Error()
                    << "Use of undefined element \"" << std::get<0>(spec)
                    << "\" in I/O mapping of device \"" << device_name << "\"";

            const auto *ielem =
                dynamic_cast<const StaticModels::Elements::Internal *>(elem->second.get());

            if(ielem == nullptr)
                Error()
                    << "Use of non-switchable element \"" << std::get<0>(spec)
                    << "\" in I/O mapping of device \"" << device_name << "\"";

            if(!ielem->contains_control(std::get<1>(spec)))
                Error()
                    << "Use of undefined control \"" << std::get<0>(spec)
                    << "." << std::get<1>(spec)
                    << "\" in I/O mapping of device \"" << device_name << "\"";

            mappings.emplace(std::move(std::get<0>(spec)),
                             std::make_pair(std::cref(*it),
                                            std::move(std::get<1>(spec))));
            */
        }
    }
    catch(const std::exception &e)
    {
        msg_error(0, LOG_NOTICE, "%s", e.what());
        throw;
    }
}

static void add_parent_connections(StaticModels::SignalPaths::ApplianceBuilder &b,
                                   const DefinedElements &defined_elements,
                                   const std::string &device_name)
{
    for(const auto &element : defined_elements)
    {
        auto *src =
            dynamic_cast<StaticModels::Elements::AudioSource *>(element.second.get());

        if(src == nullptr)
            continue;

        if(src->get_parent_source() == nullptr)
            continue;

        try
        {
            b.lookup_element(src->id_)
                .connect_to_parent(StaticModels::SignalPaths::Output(0),
                                   b.lookup_element(src->get_parent_source()->id_));
        }
        catch(const std::exception &e)
        {
            msg_error(0, LOG_NOTICE, "%s", e.what());
            throw;
        }
    }
}

StaticModels::DeviceModel
StaticModels::DeviceModel::mk_model(std::string &&name,
                                    const nlohmann::json &definition)
{
    auto defined_elements(parse_elements(definition));
    const auto io_mappings(get_io_mappings_from_model(
                                    definition, defined_elements, name));

    SignalPaths::ApplianceBuilder b(std::move(std::string(name)));
    add_elements(b, defined_elements, io_mappings);

    b.no_more_elements();

    add_explicit_connections(b, definition, defined_elements, name);
    add_parent_connections(b, defined_elements, name);

    return DeviceModel(std::move(name),
                       std::move(defined_elements),
                       SignalPaths::Appliance(std::move(b.build())));
}

static const StaticModels::Elements::Control *
get_selector_control(
        const StaticModels::SignalPaths::Appliance &signal_path,
        const std::unordered_map<std::string,
                                 std::unique_ptr<StaticModels::Elements::Element>> &es,
        const std::string &element_id, const std::string &control_id)
{
    const auto *sw_elem = signal_path.lookup_switching_element(element_id);
    if(sw_elem == nullptr)
        return nullptr;

    if(sw_elem->get_selector_name() != control_id)
        return nullptr;

    const auto found_elem(es.find(element_id));
    if(found_elem == es.end())
        return nullptr;

    const auto *elem =
        dynamic_cast<const StaticModels::Elements::Internal *>(found_elem->second.get());
    if(elem == nullptr)
        return nullptr;

    return elem->get_control_ptr(control_id);
}

bool StaticModels::DeviceModel::has_selector(const std::string &element_id,
                                             const std::string &control_id) const
{
    return get_selector_control(signal_path_, elements_,
                                element_id, control_id) != nullptr;
}

const StaticModels::Elements::Control *
StaticModels::DeviceModel::get_selector_control_ptr(const std::string &element_id,
                                                    const std::string &control_id) const
{
    return get_selector_control(signal_path_, elements_,
                                element_id, control_id);
}

const StaticModels::Elements::Control *
StaticModels::DeviceModel::get_control_by_name(const std::string &element_id,
                                               const std::string &control_id) const
{
    const auto found_elem(elements_.find(element_id));
    if(found_elem == elements_.end())
        return nullptr;

    const auto *elem =
        dynamic_cast<const StaticModels::Elements::Internal *>(found_elem->second.get());
    if(elem == nullptr)
        return nullptr;

    return elem->get_control_ptr(control_id);
}

StaticModels::SignalPaths::Selector
StaticModels::DeviceModel::to_selector_index(const std::string &element_id,
                                             const std::string &control_id,
                                             const ConfigStore::Value &value) const
{
    const auto *ctrl = get_selector_control_ptr(element_id, control_id);
    if(ctrl == nullptr)
        return SignalPaths::Selector::mk_invalid();

    try
    {
        return SignalPaths::Selector(ctrl->to_selector_index(value));
    }
    catch(const std::exception &e)
    {
        msg_error(0, LOG_NOTICE, "%s.%s: %s",
                  element_id.c_str(), control_id.c_str(), e.what());
        return SignalPaths::Selector::mk_invalid();
    }
}
