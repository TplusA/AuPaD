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

#ifndef ELEMENT_HH
#define ELEMENT_HH

#include "element_controls.hh"

#include <unordered_map>
#include <string>

namespace StaticModels
{

namespace Elements
{

class Element
{
  public:
    const std::string id_;
    const std::string description_;
    const nlohmann::json &original_definition_;

  protected:
    explicit Element(const nlohmann::json &original_definition,
                     std::string &&id, std::string description):
        id_(std::move(id)),
        description_(std::move(description)),
        original_definition_(original_definition)
    {}

  public:
    Element(const Element &) = delete;
    Element(Element &&) = default;
    Element &operator=(const Element &) = delete;
    Element &operator=(Element &&) = default;
    virtual ~Element() = default;

    virtual unsigned int get_number_of_inputs() const = 0;
    virtual unsigned int get_number_of_outputs() const = 0;
};

class AudioSource: public Element
{
  private:
    const AudioSource *parent_source_;

  public:
    AudioSource(const AudioSource &) = delete;
    AudioSource(AudioSource &&) = default;
    AudioSource &operator=(const AudioSource &) = delete;
    AudioSource &operator=(AudioSource &&) = default;

    explicit AudioSource(const nlohmann::json &original_definition,
                         std::string &&id, std::string &&description):
        Element(original_definition, std::move(id), std::move(description)),
        parent_source_(nullptr)
    {}

    virtual ~AudioSource() = default;

    unsigned int get_number_of_inputs() const final override { return 0; }
    unsigned int get_number_of_outputs() const final override { return 1; }

    bool set_parent_source(const AudioSource &src)
    {
        if(parent_source_ != nullptr)
            return false;

        parent_source_ = &src;
        return true;
    }

    const AudioSource *get_parent_source() const { return parent_source_; }
};

class AudioSink: public Element
{
  public:
    AudioSink(const AudioSink &) = delete;
    AudioSink(AudioSink &&) = default;
    AudioSink &operator=(const AudioSink &) = delete;
    AudioSink &operator=(AudioSink &&) = default;

    explicit AudioSink(const nlohmann::json &original_definition,
                       std::string &&id, std::string &&description):
        Element(original_definition, std::move(id), std::move(description))
    {}

    virtual ~AudioSink() = default;

    unsigned int get_number_of_inputs() const final override { return 1; }
    unsigned int get_number_of_outputs() const final override { return 0; }
};

class Internal: public Element
{
  private:
    unsigned int number_of_inputs_;
    unsigned int number_of_outputs_;
    std::unordered_map<std::string, std::unique_ptr<Control>> controls_;

  public:
    Internal(const Internal &) = delete;
    Internal(Internal &&) = default;
    Internal &operator=(const Internal &) = delete;
    Internal &operator=(Internal &&) = default;

    explicit Internal(const nlohmann::json &original_definition,
                      std::string &&id, std::string &&description,
                      unsigned int number_of_inputs,
                      unsigned int number_of_outputs,
                      std::unordered_map<std::string, std::unique_ptr<Control>> &&controls):
        Element(original_definition, std::move(id), std::move(description)),
        number_of_inputs_(number_of_inputs),
        number_of_outputs_(number_of_outputs),
        controls_(std::move(controls))
    {}

    virtual ~Internal() = default;

    unsigned int get_number_of_inputs() const final override { return number_of_inputs_; }
    unsigned int get_number_of_outputs() const final override { return number_of_outputs_; }

    const Control &get_control(const std::string &id) const { return *controls_.at(id); }

    const Control *get_control_ptr(const std::string &id) const
    {
        const auto it(controls_.find(id));
        return it != controls_.end() ? it->second.get() : nullptr;
    }

    bool contains_control(const std::string &id) const { return get_control_ptr(id) != nullptr; }

    void for_each_control(const std::function<void(const Control &ctrl)> &apply) const
    {
        for(const auto &ctrl : controls_)
            apply(*ctrl.second);
    }
};

}

}

#endif /* !ELEMENT_HH */
