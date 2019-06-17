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

#ifndef DEVICE_MODELS_HH
#define DEVICE_MODELS_HH

#include "element.hh"
#include "signal_paths.hh"

namespace StaticModels
{

/*!
 * All models as read from the JSON database.
 *
 * This class is basically a JSON object with a name, extended by some helper
 * functions. Not much validation of model integrity is done in here.
 */
class DeviceModelsDatabase
{
  private:
    nlohmann::json config_data_;

  public:
    DeviceModelsDatabase(const DeviceModelsDatabase &) = delete;
    DeviceModelsDatabase(DeviceModelsDatabase &&) = default;
    DeviceModelsDatabase &operator=(const DeviceModelsDatabase &) = delete;
    DeviceModelsDatabase &operator=(DeviceModelsDatabase &&) = default;
    explicit DeviceModelsDatabase() = default;

    bool load(const std::string &config, bool suppress_error = false);
    bool load(const char *config, bool suppress_error = false);
    const nlohmann::json &get_device_model_definition(const std::string &device_id) const;
};

/*!
 * A complete model for a specific appliance, fully checked.
 */
class DeviceModel
{
  private:
    std::string name_;
    std::unordered_map<std::string, std::unique_ptr<Elements::Element>> elements_;
    SignalPaths::Appliance signal_path_;

    explicit DeviceModel(
            std::string &&name,
            std::unordered_map<std::string, std::unique_ptr<Elements::Element>> &&elements,
            SignalPaths::Appliance &&signal_path):
        name_(std::move(name)),
        elements_(std::move(elements)),
        signal_path_(std::move(signal_path))
    {}

  public:
    DeviceModel(const DeviceModel &) = delete;
    DeviceModel(DeviceModel &&) = default;
    DeviceModel &operator=(const DeviceModel &) = delete;
    DeviceModel &operator=(DeviceModel &&) = default;

    static DeviceModel mk_model(std::string &&name, const nlohmann::json &definition);

    void for_each_element(const std::function<void(const Elements::Element &)> &apply) const
    {
        for(const auto &elem : elements_)
            apply(*elem.second);
    }

    const Elements::AudioSink *get_audio_sink(const std::string &sink_name) const
    {
        const auto &it(elements_.find(sink_name));
        return it != elements_.end()
            ? dynamic_cast<const Elements::AudioSink *>(it->second.get())
            : nullptr;
    }

    bool has_selector(const std::string &element_id, const std::string &control_id) const;
    SignalPaths::Selector to_selector_index(const std::string &element_id,
                                            const std::string &control_id,
                                            const ConfigStore::Value &value) const;
    const StaticModels::Elements::Control *
    get_selector_control_ptr(const std::string &element_id,
                             const std::string &control_id) const;
    const StaticModels::Elements::Control *
    get_control_by_name(const std::string &element_id,
                        const std::string &control_id) const;

    const SignalPaths::Appliance &get_signal_path_graph() const { return signal_path_; }
};

}

#endif /* !DEVICE_MODELS_HH */
