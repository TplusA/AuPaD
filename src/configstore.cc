/*
 * Copyright (C) 2019, 2020, 2021  T+A elektroakustik GmbH & Co. KG
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

#include "configstore.hh"
#include "configstore_json.hh"
#include "configstore_changes.hh"
#include "configstore_iter.hh"
#include "configvalue.hh"
#include "device_models.hh"
#include "signal_path_tracker.hh"
#include "model_parsing_utils.hh"
#include "messages.h"

#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>

/*
 * Must be sorted lexicographically for binary search.
 *
 * \see
 *     https://dbus.freedesktop.org/doc/dbus-specification.html#idm477
 */
const std::array<const std::pair<const char, const ConfigStore::ValueType>, 13>
ConfigStore::Value::TYPE_CODE_TO_VALUE_TYPE =
{
    {
        { '@', ValueType::VT_VOID },
        { 'D', ValueType::VT_TA_FIX_POINT },
        { 'Y', ValueType::VT_INT8 },
        { 'b', ValueType::VT_BOOL },
        { 'd', ValueType::VT_DOUBLE },
        { 'i', ValueType::VT_INT32 },
        { 'n', ValueType::VT_INT16 },
        { 'q', ValueType::VT_UINT16 },
        { 's', ValueType::VT_ASCIIZ },
        { 't', ValueType::VT_UINT64 },
        { 'u', ValueType::VT_UINT32 },
        { 'x', ValueType::VT_INT64 },
        { 'y', ValueType::VT_UINT8 },
    }
};

/*
 * Must be sorted according to #ConfigStore::Value.
 */
const std::array<const char, 13> ConfigStore::Value::VALUE_TYPE_TO_TYPE_CODE
{
    '@', 's', 'b', 'Y', 'y', 'n', 'q', 'i', 'u', 'x', 't', 'd', 'D',
};

namespace std
{

template <>
struct hash<pair<string, string>>
{
    size_t
    operator()(const pair<string, string> &conn) const noexcept
    {
        return hash<string>{}(conn.first) ^
               (hash<string>{}(conn.second) << 1);
    }
};

}

class ConfigStore::ChangeLog
{
  private:
    /*
     * Mapping of device name to original and current state (presence) of the
     * device.
     */
    std::unordered_map<std::string, std::pair<const bool, bool>> device_changes_;

    /*
     * Mapping of qualified audio sink to audio source connection to the
     * original and current state (presence) of the connection.
     *
     * Note that the changes in this container only represent inter-device
     * changes, not the device-internal audio path changes possibly triggered
     * by internal value changes.
     */
    std::unordered_map<std::pair<std::string, std::string>, std::pair<const bool, bool>> connection_changes_;

    /*
     * Mapping of qualified element name to its original and current values.
     * This mapping keeps track of addition of new names and their values,
     * removal of existing names, and value changes.
     */
    std::unordered_map<std::string, std::pair<const ConfigStore::Value, ConfigStore::Value>> value_changes_;

  public:
    ChangeLog(const ChangeLog &) = delete;
    ChangeLog(ChangeLog &&) = default;
    ChangeLog &operator=(const ChangeLog &) = delete;
    ChangeLog &operator=(ChangeLog &&) = default;
    explicit ChangeLog() = default;

    void clear()
    {
        device_changes_.clear();
        connection_changes_.clear();
        value_changes_.clear();
    }

    void optimize()
    {
        optimize_changes(device_changes_,
                         [] (const auto &d) { return !d.second.first; });
        optimize_changes(connection_changes_, [] (const auto &) { return true; });
        optimize_changes(value_changes_, [] (const auto &) { return true; });
    }

    bool has_changes() const
    {
        return !device_changes_.empty() || !connection_changes_.empty() ||
               !value_changes_.empty();
    }

    const auto &get_device_changes() const { return device_changes_; }
    const auto &get_connection_changes() const { return connection_changes_; }
    const auto &get_value_changes() const { return value_changes_; }

    void add_device(std::string &&name)
    {
        auto it(device_changes_.find(name));

        if(it == device_changes_.end())
            device_changes_.emplace(std::move(name), std::make_pair(false, true));
        else
            it->second.second = true;
    }

    void remove_device(std::string &&name)
    {
        auto it(device_changes_.find(name));

        if(it == device_changes_.end())
            device_changes_.emplace(std::move(name), std::make_pair(true, false));
        else
            it->second.second = false;
    }

    void add_connection(std::string &&from, std::string &&to)
    {
        auto conn(std::make_pair(std::move(from), std::move(to)));
        auto it(connection_changes_.find(conn));

        if(it == connection_changes_.end())
            connection_changes_.emplace(std::move(conn), std::make_pair(false, true));
        else
            it->second.second = true;
    }

    void remove_connection(std::string &&from, std::string &&to)
    {
        auto conn(std::make_pair(std::move(from), std::move(to)));
        auto it(connection_changes_.find(conn));

        if(it == connection_changes_.end())
            connection_changes_.emplace(std::move(conn), std::make_pair(true, false));
        else
            it->second.second = false;
    }

    void set_value(std::string &&name, ConfigStore::Value &&old_value,
                   ConfigStore::Value &&new_value)
    {
        auto it(value_changes_.find(name));

        if(it == value_changes_.end())
            value_changes_.emplace(
                std::move(name),
                std::make_pair(std::move(old_value), std::move(new_value)));
        else
            it->second.second = std::move(new_value);
    }

    void unset_values(const std::string &element_name,
                      std::unordered_map<std::string, ConfigStore::Value> &&old_values)
    {
        for(auto &val : old_values)
            set_value(element_name + '.' + val.first, std::move(val.second),
                      ConfigStore::Value());
    }

  private:
    template <typename T>
    static void optimize_changes(
            T &changes,
            const std::function<bool(const typename T::value_type &)> &allow)
    {
        for(auto it = changes.begin(); it != changes.end(); /* nothing */)
        {
            if(allow(*it) && it->second.first == it->second.second)
                it = changes.erase(it);
            else
                ++it;
        }
    }
};

ConfigStore::Changes::Changes() {}

ConfigStore::Changes::~Changes() {}

void ConfigStore::Changes::reset(std::unique_ptr<ConfigStore::ChangeLog> changes)
{
    changes_ = std::move(changes);
}

void ConfigStore::Changes::reset()
{
    changes_ = nullptr;
}

void ConfigStore::Changes::for_each_changed_device(
        const std::function<void(const std::string &, bool)> &apply) const
{
    if(changes_ != nullptr)
        for(const auto &it : changes_->get_device_changes())
            apply(it.first, it.second.second);
}

void ConfigStore::Changes::for_each_changed_connection(
        const std::function<void(const std::string &from,
                                 const std::string &to, bool)> &apply) const
{
    if(changes_ != nullptr)
        for(const auto &it : changes_->get_connection_changes())
            apply(it.first.first, it.first.second, it.second.second);
}

void ConfigStore::Changes::for_each_changed_value(
        const std::function<void(const std::string &name, const Value &old_value,
                                 const Value &new_value)> &apply) const
{
    if(changes_ != nullptr)
        for(const auto &it : changes_->get_value_changes())
            apply(it.first, it.second.first, it.second.second);
}

/*!
 * Representation of an audio path element with values.
 */
class ReportedElement
{
  public:
    const std::string name_;

  private:
    std::unordered_map<std::string, ConfigStore::Value> values_;

  public:
    ReportedElement(const ReportedElement &) = delete;
    ReportedElement(ReportedElement &&) = default;
    ReportedElement &operator=(const ReportedElement &) = delete;
    ReportedElement &operator=(ReportedElement &&) = default;

    explicit ReportedElement(std::string &&name):
        name_(std::move(name))
    {}

    const ConfigStore::Value &set_value(const std::string &parameter_name,
                                        ConfigStore::Value &old_value,
                                        ConfigStore::Value &&new_value)
    {
        auto it(values_.find(parameter_name));

        if(it != values_.end())
        {
            old_value = std::move(it->second);
            it->second = std::move(new_value);
            return it->second;
        }
        else
            return values_.emplace(parameter_name, std::move(new_value)).first->second;
    }

    void unset_value(const std::string &parameter_name,
                     ConfigStore::Value &old_value)
    {
        auto it(values_.find(parameter_name));

        if(it != values_.end())
        {
            old_value = std::move(it->second);
            values_.erase(parameter_name);
        }
        else
            Error() << "element " << name_ << " has no parameter named \"" <<
                parameter_name << "\"";
    }

    void unset_values(std::unordered_map<std::string, ConfigStore::Value> &old_values)
    {
        old_values = std::move(values_);
    }

    const auto &get_values() const
    {
        // cppcheck-suppress accessMoved
        return values_;
    }
};

/*!
 * Representation of any audio device instances reported by the appliance.
 *
 * For each reported device instance, a #Device object is created. All its
 * element configurations and audio connections are stored in these objects.
 */
class Device
{
  public:
    const std::string name_;
    const std::string device_id_;

  private:
    const StaticModels::DeviceModel *const model_;
    std::unique_ptr<ModelCompliant::SignalPathTracker> current_signal_path_;

    std::unordered_map<std::string, ReportedElement> elements_;

    /*!
     * Outgoing connections from this device.
     *
     * Mapping of pair of sink name defined for this device and target device
     * name to a list of all connected input names defined for the target
     * device.
     */
    std::map<std::pair<std::string, std::string>, std::unordered_set<std::string>>
    outgoing_connections_;

  public:
    Device(const Device &) = delete;
    Device(Device &&) = default;
    Device &operator=(const Device &) = delete;
    Device &operator=(Device &&) = default;

    explicit Device(std::string &&name, std::string &&device_id,
                    const StaticModels::DeviceModel *model):
        name_(std::move(name)),
        device_id_(std::move(device_id)),
        model_(model),
        current_signal_path_(model_ != nullptr
                ? std::make_unique<ModelCompliant::SignalPathTracker>(model_->get_signal_path_graph())
                : nullptr)
    {}

    const ConfigStore::Value &set_value(
                const std::string &element_id,
                const std::string &element_parameter_name,
                const std::string &type_code, const nlohmann::json &value,
                ConfigStore::Value &old_value)
    {
        const auto &new_value(get_element(element_id)
                              .set_value(element_parameter_name, old_value,
                                         ConfigStore::Value(type_code,
                                                            nlohmann::json(value))));

        if(current_signal_path_ != nullptr)
        {
            const auto sel(model_->to_selector_index(element_id,
                                                     element_parameter_name,
                                                     new_value));
            if(sel.is_valid())
                current_signal_path_->select(element_id, sel);
        }

        return new_value;
    }

    void unset_value(const std::string &element_id,
                     const std::string &element_parameter_name,
                     ConfigStore::Value &old_value)
    {
        get_element(element_id).unset_value(element_parameter_name, old_value);

        if(current_signal_path_ != nullptr &&
           model_->has_selector(element_id, element_parameter_name))
            current_signal_path_->floating(element_id);
    }

    void unset_values(const std::string &element_id,
                      std::unordered_map<std::string, ConfigStore::Value> &old_values)
    {
        get_element(element_id).unset_values(old_values);

        if(current_signal_path_ != nullptr)
            if(std::any_of(old_values.begin(), old_values.end(),
                    [this, &element_id] (const auto &v)
                    { return model_->has_selector(element_id, v.first); }))
            {
                current_signal_path_->floating(element_id);
                return;
            }
    }

    void add_connection(const std::string &sink_name,
                        const std::string &target_dev,
                        const std::string &target_conn);

    const auto *get_model() const { return model_; }
    const auto &get_elements() const { return elements_; }
    const auto &get_outgoing_connections() const { return outgoing_connections_; }
    const auto *get_signal_paths() const { return current_signal_path_.get(); }

    void remove_connections(ConfigStore::ChangeLog &log);
    void remove_connections_with_target(const std::string &target_device,
                                        ConfigStore::ChangeLog &log);
    void remove_connections_with_target(const std::string &target_device,
                                        const std::string &target_audio_sink_name,
                                        ConfigStore::ChangeLog &log);
    void remove_connections_on_sink(const std::string &audio_sink_name,
                                    ConfigStore::ChangeLog &log);
    void remove_connections_on_sink(const std::string &audio_sink_name,
                                    const std::string &target_device,
                                    ConfigStore::ChangeLog &log);
    void remove_connection_on_sink(const std::string &audio_sink_name,
                                   const std::string &target_device,
                                   const std::string &target_audio_sink_name,
                                   ConfigStore::ChangeLog &log);

  private:
    /* get or insert element by name */
    ReportedElement &get_element(const std::string &element_id);
};

/*!
 * Implementation details of the audio path configuration store.
 */
class ConfigStore::Settings::Impl
{
  private:
    /* models */
    const StaticModels::DeviceModelsDatabase &models_database_;
    std::unordered_map<std::string, std::unique_ptr<StaticModels::DeviceModel>> models_;
    const StaticModels::DeviceModel *root_appliance_model_;

    /* instances */
    std::unordered_map<std::string, Device> devices_;
    std::unique_ptr<ChangeLog> log_;

  public:
    Impl(const Impl &) = delete;
    Impl(Impl &&) = default;
    Impl &operator=(const Impl &) = delete;
    Impl &operator=(Impl &&) = default;

    explicit Impl(const StaticModels::DeviceModelsDatabase &models_database):
        models_database_(models_database),
        root_appliance_model_(nullptr)
    {}

    /*
     * Create new object from old one, ditching most the old one's data.
     * This is sort of a lossy move constructor.
     */
    static std::unique_ptr<Impl> make_fresh(std::unique_ptr<Impl> old)
    {
        return std::make_unique<Impl>(old->models_database_);
    }

    void update(const nlohmann::json &j);
    nlohmann::json json() const;

    bool extract_changes(Changes &changes)
    {
        if(log_ != nullptr)
            log_->optimize();

        const bool result = log_ != nullptr ? log_->has_changes() : false;
        changes.reset(std::move(log_));
        return result;
    }

    const Device &get_device(const char *name) const
    {
        try
        {
            return devices_.at(name);
        }
        catch(const std::out_of_range &)
        {
            ErrorBase<std::out_of_range>() << "device \"" << name << "\" is unknown";
        }
    }

    const Device &get_device(const std::string &name) const
    {
        try
        {
            return devices_.at(name);
        }
        catch(const std::out_of_range &)
        {
            ErrorBase<std::out_of_range>() << "device \"" << name << "\" is unknown";
        }
    }

  private:
    void add_instance(std::string &&name, std::string &&device_id);
    bool remove_instance(const std::string &name, bool must_exist);
    void clear_instances();
    void set_element_values(const std::string &qualified_name,
                            const nlohmann::json &kv, bool is_reset);
    void clear_element_value(const std::string &qualified_name,
                             const std::string &element_parameter_name);
    void clear_element_values(const std::string &qualified_name);
    void add_connection(const std::string &from, const std::string &to);
    void remove_connections(const std::string &from, const std::string &to);
    void remove_outgoing_connections(const std::string &from);
    void remove_ingoing_connections(const std::string &to);
    void remove_all_connections();
    const StaticModels::DeviceModel *get_device_model(const std::string &name);
};

ConfigStore::ValueType
ConfigStore::Value::type_code_to_type(const std::string &type_code)
{
    if(type_code.length() != 1)
        Error() << "type code \"" << type_code << "\" is invalid (wrong length)";

    const auto it(std::lower_bound(
        TYPE_CODE_TO_VALUE_TYPE.begin(), TYPE_CODE_TO_VALUE_TYPE.end(),
        type_code[0],
        [] (const auto &a, const auto &b) { return a.first < b; }));

    if(it == TYPE_CODE_TO_VALUE_TYPE.end() || it->first != type_code[0])
        Error() << "type code \"" << type_code << "\" is invalid (unknown code)";

    return it->second;
}

void ConfigStore::Value::validate() const
{
    switch(type_)
    {
      case ValueType::VT_VOID:
        if(value_.is_null())
            return;

        break;

      case ValueType::VT_ASCIIZ:
        if(value_.is_string())
            return;

        break;

      case ValueType::VT_BOOL:
        if(value_.is_boolean())
            return;

        break;

      case ValueType::VT_INT8:
      case ValueType::VT_INT16:
      case ValueType::VT_INT32:
      case ValueType::VT_INT64:
        if(!value_.is_number_integer())
            break;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
        switch(type_)
        {
          case ValueType::VT_INT8:  range_check<ValueType::VT_INT8>(value_);  return;
          case ValueType::VT_INT16: range_check<ValueType::VT_INT16>(value_); return;
          case ValueType::VT_INT32: range_check<ValueType::VT_INT32>(value_); return;
          case ValueType::VT_INT64: range_check<ValueType::VT_INT64>(value_); return;
          default: os_abort();
        }
#pragma GCC diagnostic pop

        break;

      case ValueType::VT_UINT8:
      case ValueType::VT_UINT16:
      case ValueType::VT_UINT32:
      case ValueType::VT_UINT64:
        if(!value_.is_number_integer() || !value_.is_number_unsigned())
            break;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch-enum"
        switch(type_)
        {
          case ValueType::VT_UINT8:  range_check<ValueType::VT_UINT8>(value_);  return;
          case ValueType::VT_UINT16: range_check<ValueType::VT_UINT16>(value_); return;
          case ValueType::VT_UINT32: range_check<ValueType::VT_UINT32>(value_); return;
          case ValueType::VT_UINT64: range_check<ValueType::VT_UINT64>(value_); return;
          default: os_abort();
        }
#pragma GCC diagnostic pop

        break;

      case ValueType::VT_DOUBLE:
        if(value_.is_number())
            return;

        break;

      case ValueType::VT_TA_FIX_POINT:
        if(!value_.is_number())
            break;

        if(FixPoint::is_in_range(value_.get<double>()))
            return;

        break;
    }

    Error() << "mismatch between type code \"" << type_to_type_code(type_) <<
        "\" and value \"" << value_ << "\"";
}

void Device::add_connection(const std::string &sink_name,
                            const std::string &target_dev,
                            const std::string &target_conn)
{
    outgoing_connections_[{sink_name, target_dev}].insert(target_conn);
}

enum class ModifyResult { KEEP, DROP_AND_LOG, };

static inline void modify_connections(
        const std::string &source_name,
        std::map<std::pair<std::string, std::string>,
                 std::unordered_set<std::string>> &outgoing_connections,
        ConfigStore::ChangeLog &log,
        const std::function<ModifyResult(std::remove_reference<decltype(outgoing_connections)>::type::reference)> &modify)
{
    if(outgoing_connections.empty())
        return;

    std::remove_reference<decltype(outgoing_connections)>::type new_outgoing;

    for(auto &conn : outgoing_connections)
    {
        switch(modify(conn))
        {
          case ModifyResult::KEEP:
            if(!conn.second.empty())
                new_outgoing[conn.first] = std::move(conn.second);

            break;

          case ModifyResult::DROP_AND_LOG:
            for(const auto &target : conn.second)
                log.remove_connection(source_name + '.' + conn.first.first,
                                      conn.first.second + '.' + target);

            break;
        }
    }

    outgoing_connections = std::move(new_outgoing);
}


void Device::remove_connections(ConfigStore::ChangeLog &log)
{
    for(const auto &conns : outgoing_connections_)
        for(const auto &conn : conns.second)
            log.remove_connection(name_ + '.' + conns.first.first,
                                  conns.first.second + '.' + conn);

    outgoing_connections_.clear();
}

void Device::remove_connections_with_target(const std::string &target_device,
                                            ConfigStore::ChangeLog &log)
{
    modify_connections(name_, outgoing_connections_, log,
        [&target_device] (auto &conn)
        {
            return (conn.first.second != target_device
                    ? ModifyResult::KEEP
                    : ModifyResult::DROP_AND_LOG);
        });
}

void Device::remove_connections_with_target(const std::string &target_device,
                                            const std::string &target_audio_sink_name,
                                            ConfigStore::ChangeLog &log)
{
    modify_connections(name_, outgoing_connections_, log,
        [this, &target_device, &target_audio_sink_name, &log]
        (auto &conn)
        {
            if(conn.first.second == target_device &&
               conn.second.erase(target_audio_sink_name) != 0)
                log.remove_connection(name_ + '.' + conn.first.first,
                                      target_device + '.' + target_audio_sink_name);

            return ModifyResult::KEEP;
        });
}

void Device::remove_connections_on_sink(const std::string &audio_sink_name,
                                        ConfigStore::ChangeLog &log)
{
    modify_connections(name_, outgoing_connections_, log,
        [&audio_sink_name] (auto &conn)
        {
            return (conn.first.first != audio_sink_name
                    ? ModifyResult::KEEP
                    : ModifyResult::DROP_AND_LOG);
        });
}

void Device::remove_connections_on_sink(const std::string &audio_sink_name,
                                        const std::string &target_device,
                                        ConfigStore::ChangeLog &log)
{
    modify_connections(name_, outgoing_connections_, log,
        [&audio_sink_name, &target_device]
        (auto &conn)
        {
            return (conn.first.first != audio_sink_name ||
                    conn.first.second != target_device
                    ? ModifyResult::KEEP
                    : ModifyResult::DROP_AND_LOG);
        });
}

void Device::remove_connection_on_sink(const std::string &audio_sink_name,
                                       const std::string &target_device,
                                       const std::string &target_audio_sink_name,
                                       ConfigStore::ChangeLog &log)
{
    modify_connections(name_, outgoing_connections_, log,
        [this, &audio_sink_name, &target_device, &target_audio_sink_name, &log]
        (auto &conn)
        {
            if(conn.first.first == audio_sink_name &&
               conn.first.second == target_device &&
               conn.second.erase(target_audio_sink_name) != 0)
                log.remove_connection(name_ + '.' + audio_sink_name,
                                      target_device + '.' + target_audio_sink_name);

            return ModifyResult::KEEP;
        });
}

ReportedElement &Device::get_element(const std::string &element_id)
{
    try
    {
        return elements_.at(element_id);
    }
    catch(const std::out_of_range &e)
    {
        /* handled below */
    }

    if(elements_.emplace(element_id, ReportedElement(std::string(element_id))).second)
        return elements_.at(element_id);

    Error() << "internal error while inserting element " <<
        element_id << " into device " << name_;

    /* never reached, suppress compiler warning */
    return *static_cast<ReportedElement *>(nullptr);
}

void ConfigStore::Settings::Impl::update(const nlohmann::json &j)
{
    if(log_ == nullptr)
        log_ = std::make_unique<ChangeLog>();

    for(const auto &change : j.at("audio_path_changes"))
    {
        const auto &op(change.at("op").get<std::string>());

        if(op == "add_instance")
            add_instance(change.at("name"), change.at("id"));
        else if(op == "rm_instance")
            remove_instance(change.at("name"), true);
        else if(op == "clear_instances")
            clear_instances();
        else if(op == "set")
            set_element_values(change.at("element"), change.at("kv"), true);
        else if(op == "update")
            set_element_values(change.at("element"), change.at("kv"), false);
        else if(op == "unset")
            clear_element_value(change.at("element"), change.at("v"));
        else if(op == "unset_all")
            clear_element_values(change.at("element"));
        else if(op == "connect")
            add_connection(change.at("from"), change.at("to"));
        else if(op == "disconnect")
        {
            if(change.find("from") == change.end())
            {
                if(change.find("to") == change.end())
                    remove_all_connections();
                else
                    remove_ingoing_connections(change.at("to"));
            }
            else
                if(change.find("to") == change.end())
                    remove_outgoing_connections(change.at("from"));
                else
                    remove_connections(change.at("from"), change.at("to"));
        }
        else
            Error() << "invalid audio path change op \"" << op << "\"";
    }
}

void ConfigStore::Settings::Impl::add_instance(std::string &&name,
                                               std::string &&device_id)
{
    if(name.empty())
        Error() <<
            "cannot create new instance with empty name for device ID \"" <<
            device_id << "\"";

    if(device_id.empty())
        Error() << "empty device ID for new instance \"" << name << "\"";

    remove_instance(name, false);

    log_->add_device(std::string(name));

    const auto *dm = get_device_model(std::string(device_id));

    if(name == "self")
        root_appliance_model_ = dm;

    std::string key(name);
    devices_.emplace(std::move(key),
                     Device(std::move(name), std::move(device_id), dm));
}

bool ConfigStore::Settings::Impl::remove_instance(const std::string &name,
                                                  bool must_exist)
{
    if(name.empty())
        Error() << "cannot remove instance with empty name";

    auto dev(devices_.find(name));

    if(dev == devices_.end())
    {
        if(must_exist)
            Error() << "cannot remove nonexistent device instance named \"" <<
                name << "\"";
        else
            return false;
    }

    for(auto &d : devices_)
    {
        d.second.remove_connections_with_target(name, *log_);

        for(const auto &elem : d.second.get_elements())
        {
            std::unordered_map<std::string, ConfigStore::Value> old_values;
            d.second.unset_values(elem.first, old_values);
            log_->unset_values(d.first + '.' + elem.first, std::move(old_values));
        }
    }

    dev->second.remove_connections(*log_);

    devices_.erase(dev);
    log_->remove_device(std::string(name));

    if(name == "self")
        root_appliance_model_ = nullptr;

    return true;
}

void ConfigStore::Settings::Impl::clear_instances()
{
    for(const auto &dev : devices_)
        log_->remove_device(std::string(dev.second.name_));

    devices_.clear();
    root_appliance_model_ = nullptr;
}

static std::tuple<Device &, std::string>
get_device_and_element_name(const std::string &qualified_name,
                            std::unordered_map<std::string, Device> &devices)
{
    const auto qname(StaticModels::Utils::split_qualified_name(qualified_name));
    const auto &device_name(std::get<0>(qname));

    try
    {
        return std::make_tuple(std::ref(devices.at(device_name)), std::move(std::get<1>(qname)));
    }
    catch(const std::out_of_range &e)
    {
        /* handled below */
    }

    Error() << "unknown device \"" << device_name << "\"";
}

void ConfigStore::Settings::Impl::set_element_values(const std::string &qualified_name,
                                                     const nlohmann::json &kv,
                                                     bool is_reset)
{
    auto d(get_device_and_element_name(qualified_name, devices_));
    auto &dev(std::get<0>(d));
    const auto &element_id(std::get<1>(d));

    if(is_reset)
    {
        try
        {
            std::unordered_map<std::string, ConfigStore::Value> old_values;
            dev.unset_values(element_id, old_values);
            log_->unset_values(element_id, std::move(old_values));
        }
        catch(const std::exception &e)
        {
            msg_error(0, LOG_NOTICE, "%s", e.what());
        }
    }

    for(const auto &value : kv.items())
    {
        try
        {
            ConfigStore::Value old_value;
            const auto &val(
                dev.set_value(element_id, value.key(),
                              value.value()["type"].get<const std::string>(),
                              value.value()["value"], old_value));
            log_->set_value(std::string(qualified_name) + '.' + value.key(),
                            std::move(old_value), ConfigStore::Value(val));
        }
        catch(const std::exception &e)
        {
            msg_error(0, LOG_NOTICE, "%s", e.what());
        }
    }
}

void ConfigStore::Settings::Impl::clear_element_value(
        const std::string &qualified_name, const std::string &element_parameter_name)
{
    auto d(get_device_and_element_name(qualified_name, devices_));
    ConfigStore::Value old_value;
    std::get<0>(d).unset_value(std::get<1>(d), element_parameter_name, old_value);
    log_->set_value(std::string(qualified_name), std::move(old_value),
                    ConfigStore::Value());
}

void ConfigStore::Settings::Impl::clear_element_values(const std::string &qualified_name)
{
    auto d(get_device_and_element_name(qualified_name, devices_));
    std::unordered_map<std::string, ConfigStore::Value> old_values;
    std::get<0>(d).unset_values(std::get<1>(d), old_values);
    log_->unset_values(qualified_name, std::move(old_values));
}

void ConfigStore::Settings::Impl::add_connection(const std::string &from,
                                                 const std::string &to)
{
    auto from_dev(get_device_and_element_name(from, devices_));
    auto to_dev(get_device_and_element_name(to, devices_));
    std::get<0>(from_dev).add_connection(
        std::get<1>(from_dev), std::get<0>(to_dev).name_, std::get<1>(to_dev));
    log_->add_connection(std::string(from), std::string(to));
}

void ConfigStore::Settings::Impl::remove_connections(const std::string &from,
                                                     const std::string &to)
{
    if(StaticModels::Utils::is_qualified_name(from))
    {
        auto d(get_device_and_element_name(from, devices_));
        auto &dev(std::get<0>(d));
        const auto &element_id(std::get<1>(d));

        if(StaticModels::Utils::is_qualified_name(to))
        {
            const auto &target(StaticModels::Utils::split_qualified_name(to));
            dev.remove_connection_on_sink(element_id, std::get<0>(target),
                                          std::get<1>(target), *log_);
        }
        else
            dev.remove_connections_on_sink(element_id, to, *log_);
    }
    else
    {
        auto &dev(devices_.at(from));

        if(StaticModels::Utils::is_qualified_name(to))
        {
            const auto &target(StaticModels::Utils::split_qualified_name(to));
            dev.remove_connections_with_target(std::get<0>(target),
                                               std::get<1>(target),
                                               *log_);
        }
        else
            dev.remove_connections_with_target(to, *log_);
    }
}

void ConfigStore::Settings::Impl::remove_outgoing_connections(const std::string &from)
{
    if(StaticModels::Utils::is_qualified_name(from))
    {
        auto d(get_device_and_element_name(from, devices_));
        auto &dev(std::get<0>(d));
        const auto &element_id(std::get<1>(d));
        dev.remove_connections_on_sink(element_id, *log_);
    }
    else
    {
        auto &dev(devices_.at(from));
        dev.remove_connections(*log_);
    }
}

void ConfigStore::Settings::Impl::remove_ingoing_connections(const std::string &to)
{
    if(!StaticModels::Utils::is_qualified_name(to))
    {
        if(devices_.find(to) != devices_.end())
            for(auto &d : devices_)
                d.second.remove_connections_with_target(to, *log_);
    }
    else
    {
        const auto &target(StaticModels::Utils::split_qualified_name(to));
        if(devices_.find(std::get<0>(target)) != devices_.end())
            for(auto &d : devices_)
                d.second.remove_connections_with_target(std::get<0>(target),
                                                        std::get<1>(target),
                                                        *log_);
    }
}

void ConfigStore::Settings::Impl::remove_all_connections()
{
    for(auto &dev : devices_)
        dev.second.remove_connections(*log_);
}

const StaticModels::DeviceModel *
ConfigStore::Settings::Impl::get_device_model(const std::string &name)
{
    const auto dm(models_.find(name));
    if(dm != models_.end())
        return dm->second.get();

    models_.emplace(name, nullptr);

    const auto &model(models_database_.get_device_model_definition(name));
    if(model.is_null())
    {
        msg_error(0, LOG_NOTICE,
                  "No model defined for device ID \"%s\"", name.c_str());
        return nullptr;
    }

    try
    {
        models_.erase(name);
        return
            models_.emplace(name,
                std::make_unique<StaticModels::DeviceModel>(
                    StaticModels::DeviceModel::mk_model(std::string(name), model)))
            .first->second.get();
    }
    catch(const std::exception &e)
    {
        msg_error(0, LOG_NOTICE, "%s", e.what());
        return nullptr;
    }
}

nlohmann::json ConfigStore::Settings::Impl::json() const
{
    nlohmann::json result({});

    for(const auto &dev : devices_)
        result["devices"][dev.second.name_] = dev.second.device_id_;

    for(const auto &dev : devices_)
    {
        for(const auto &elem : dev.second.get_elements())
        {
            if(elem.second.get_values().empty())
                continue;

            auto &e(result["settings"][dev.second.name_][elem.second.name_] = nullptr);

            for(const auto &param : elem.second.get_values())
            {
                auto &val(e[param.first]);
                val["value"] = param.second.get_value();
                val["type"] = std::string(1, param.second.get_type_code());
            }
        }
    }

    for(const auto &dev : devices_)
    {
        if(dev.second.get_outgoing_connections().empty())
            continue;

        auto &e(result["connections"][dev.second.name_] = nullptr);

        for(const auto &conn : dev.second.get_outgoing_connections())
            for(const auto &name : conn.second)
                e[conn.first.first].push_back(conn.first.second + '.' + name);
    }

    return result;
}

ConfigStore::Settings::Settings(const StaticModels::DeviceModelsDatabase &models_database):
    impl_(std::make_unique<Impl>(models_database))
{}

ConfigStore::Settings::~Settings() = default;

void ConfigStore::Settings::clear()
{
    impl_ = Impl::make_fresh(std::move(impl_));
}

void ConfigStore::Settings::update(const std::string &d)
{
    try
    {
        impl_->update(nlohmann::json::parse(d));
    }
    catch(const std::exception &e)
    {
        msg_error(0, LOG_NOTICE, "%s", e.what());
    }
}

std::string ConfigStore::Settings::json_string() const
{
    try
    {
        return impl_->json().dump();
    }
    catch(const std::exception &e)
    {
        BUG("Failed serializing audio path configuration: %s", e.what());
        return nlohmann::json().dump();
    }
}

nlohmann::json ConfigStore::ConstSettingsJSON::json() const
{
    try
    {
        return settings_.impl_->json();
    }
    catch(const std::exception &e)
    {
        BUG("Failed obtaining audio path configuration: %s", e.what());
        return nlohmann::json();
    }
}

void ConfigStore::SettingsJSON::update(const nlohmann::json &j)
{
    try
    {
        settings_.impl_->update(j);
    }
    catch(const std::exception &e)
    {
        msg_error(0, LOG_NOTICE, "%s", e.what());
    }
}

bool ConfigStore::SettingsJSON::extract_changes(Changes &changes)
{
    return settings_.impl_->extract_changes(changes);
}

ConfigStore::DeviceContext
ConfigStore::SettingsIterator::with_device(const char *device_name) const
{
    return DeviceContext(settings_.impl_->get_device(device_name));
}

ConfigStore::DeviceContext
ConfigStore::SettingsIterator::with_device(const std::string &device_name) const
{
    return DeviceContext(settings_.impl_->get_device(device_name));
}

void ConfigStore::DeviceContext::for_each_setting(const SettingReportFn &apply) const
{
    for(const auto &elem : device_.get_elements())
        for(const auto &v : elem.second.get_values())
            // cppcheck-suppress useStlAlgorithm
            if(!apply(elem.second.name_, v.first, v.second))
                return;
}

const StaticModels::DeviceModel *ConfigStore::DeviceContext::get_model() const
{
    return device_.get_model();
}

void ConfigStore::DeviceContext::for_each_setting(const std::string &element,
                                                  const SettingReportFn &apply) const
{
    const auto &it(device_.get_elements().find(element));
    if(it == device_.get_elements().end())
        return;

    for(const auto &v : it->second.get_values())
        // cppcheck-suppress useStlAlgorithm
        if(!apply(element, v.first, v.second))
            return;
}

bool ConfigStore::DeviceContext::for_each_signal_path(
        bool is_root_device,
        const ModelCompliant::SignalPathTracker::EnumerateCallbackFn &apply) const
{
    const auto *sp = device_.get_signal_paths();
    return sp != nullptr
        ? sp->enumerate_active_signal_paths(apply, is_root_device)
        : false;
}

const ConfigStore::Value *
ConfigStore::DeviceContext::get_control_value(const std::string &element_id,
                                              const std::string &control_id) const
{
    const auto &elem(device_.get_elements().find(element_id));
    if(elem == device_.get_elements().end())
        return nullptr;

    const auto &val(elem->second.get_values().find(control_id));
    if(val == elem->second.get_values().end())
        return nullptr;

    return &val->second;
}
