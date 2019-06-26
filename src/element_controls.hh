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

#ifndef ELEMENT_CONTROLS_HH
#define ELEMENT_CONTROLS_HH

#include "configvalue.hh"

#include <string>

namespace StaticModels
{

namespace Elements
{

/*!
 * Base class for all audio path element controls.
 */
class Control
{
  public:
    const nlohmann::json &original_definition_;
    const std::string id_;
    const std::string label_;
    const std::string description_;

  protected:
    explicit Control(const nlohmann::json &original_definition,
                    std::string &&id, std::string &&label,
                    std::string &&description):
        original_definition_(original_definition),
        id_(std::move(id)),
        label_(std::move(label)),
        description_(std::move(description))
    {
        if(id_.empty())
            Error() << "Empty control ID";
    }

  public:
    Control(const Control &) = delete;
    Control(Control &&) = default;
    Control &operator=(const Control &) = delete;
    Control &operator=(Control &&) = default;
    virtual ~Control() = default;

    virtual ConfigStore::ValueType get_value_type() const = 0;
    virtual bool is_neutral_value(const ConfigStore::Value &value) const = 0;
    virtual unsigned int get_number_of_choices() const = 0;

    using ForEachChoiceFn =
        std::function<void(unsigned int idx, const std::string &choice)>;
    virtual void for_each_choice(const ForEachChoiceFn &apply) const = 0;
    virtual unsigned int to_selector_index(const ConfigStore::Value &value) const = 0;

    const nlohmann::json &get_original_definition() const
    {
        return original_definition_;
    }
};

/*!
 * Control which allows to pick one out of a finite range of values.
 *
 * The values are always strings. These can be mapped to a zero-based range of
 * integers and vice versa, using the exact order as defined in the device
 * model.
 */
class Choice: public Control
{
  private:
    const std::vector<std::string> choices_;
    const std::string neutral_setting_;
    const std::unordered_map<std::string, unsigned int> choice_to_index_;

  public:
    Choice(const Choice &) = delete;
    Choice(Choice &&) = default;
    Choice &operator=(const Choice &) = delete;
    Choice &operator=(Choice &&) = default;

    explicit Choice(const nlohmann::json &original_definition,
                    std::string &&id, std::string &&label,
                    std::string &&description,
                    std::vector<std::string> &&choices,
                    std::string &&neutral_setting):
        Control(original_definition, std::move(id), std::move(label),
                std::move(description)),
        choices_(std::move(choices)),
        neutral_setting_(std::move(neutral_setting)),
        choice_to_index_(std::move(Choice::hash_choices(choices_)))
    {
        if(choices_.size() < 2)
            Error() << "Not enough choices for control \"" << id_ << "\"";

        for(const auto &ch : choices_)
            if(ch.empty())
                Error() << "Empty choice value in control \"" << id_ << "\"";

        if(!neutral_setting_.empty() &&
           std::find(choices_.begin(), choices_.end(),
                     neutral_setting_) == choices_.end())
            Error() << "Neutral setting not a valid choice for control \""
                    << id_ << "\"";
    }

    ConfigStore::ValueType get_value_type() const final override
    {
        return ConfigStore::ValueType::VT_ASCIIZ;
    }

    bool is_neutral_value(const ConfigStore::Value &value) const final override
    {
        return
            !neutral_setting_.empty() &&
            value.is_of_type(ConfigStore::ValueType::VT_ASCIIZ) &&
            value.get_value().get<std::string>() == neutral_setting_;
    }

    unsigned int get_number_of_choices() const final override
    {
        return choices_.size();
    }

    void for_each_choice(const ForEachChoiceFn &apply) const final override
    {
        unsigned int i = 0;
        for(const auto &c : choices_)
            apply(i++, c);
    }

    unsigned int to_selector_index(const ConfigStore::Value &value) const
        final override
    {
        if(!value.is_of_type(ConfigStore::ValueType::VT_ASCIIZ))
            Error() << "Selector values for choices must be a string";

        const auto &s(value.get_value().get<std::string>());
        return choice_to_index_.at(s);
    }

  private:
    static std::unordered_map<std::string, unsigned int>
    hash_choices(const std::vector<std::string> &choices)
    {
        std::unordered_map<std::string, unsigned int> result;

        for(unsigned int i = 0; i < choices.size(); ++i)
            result[choices[i]] = i;

        return result;
    }
};

/*!
 * Control which allows selecting any value between two boundaries.
 *
 * This control is the natural choice for numeric values.
 */
class Range: public Control
{
  private:
    const std::string scale_;
    const ConfigStore::Value min_;
    const ConfigStore::Value max_;
    const ConfigStore::Value step_;
    const ConfigStore::Value neutral_setting_;

  public:
    Range(const Range &) = delete;
    Range(Range &&) = default;
    Range &operator=(const Range &) = delete;
    Range &operator=(Range &&) = default;

    explicit Range(const nlohmann::json &original_definition,
                   std::string &&id, std::string &&label,
                   std::string &&description, std::string &&scale,
                   ConfigStore::Value &&min, ConfigStore::Value &&max,
                   ConfigStore::Value &&step,
                   ConfigStore::Value &&neutral_setting):
        Control(original_definition, std::move(id), std::move(label),
                std::move(description)),
        scale_(std::move(scale)),
        min_(std::move(min)),
        max_(std::move(max)),
        step_(std::move(step)),
        neutral_setting_(std::move(neutral_setting))
    {
        if(!min_.is_numeric() ||
           !max_.equals_type_of(min_) || !step_.equals_type_of(min_))
            Error() << "Range limits and step width must be numeric values "
                       "in control \""
                    << id_ <<  "\", and all of the same type";

        if(max_ < min_)
            Error() << "Minimum value is greater than maximum value "
                       "of control \"" << id_ << "\"";

        if(!neutral_setting_.is_of_type(ConfigStore::ValueType::VT_VOID))
        {
            if(neutral_setting_ < min_)
                Error() << "Neutral value is smaller than minimum value "
                           "of control \"" << id_ << "\"";

            if(max_ < neutral_setting_)
                Error() << "Neutral value is greater than minimum value "
                           "of control \"" << id_ << "\"";
        }
    }

    ConfigStore::ValueType get_value_type() const final override
    {
        return min_.get_type();
    }

    bool is_neutral_value(const ConfigStore::Value &value) const final override
    {
        return
            !neutral_setting_.is_of_type(ConfigStore::ValueType::VT_VOID) &&
            value == neutral_setting_;
    }

    unsigned int get_number_of_choices() const final override
    {
        Error() << "Ranges cannot be used as selector";
    }

    void for_each_choice(const ForEachChoiceFn &apply) const final override
    {
        Error() << "Cannot enumerate range selectors";
    }

    unsigned int to_selector_index(const ConfigStore::Value &value) const
        final override
    {
        return std::numeric_limits<unsigned int>::max();
    }

    const ConfigStore::Value &get_min() const { return min_; }
    const ConfigStore::Value &get_max() const { return max_; }
};

/*!
 * Control which can be either on or off.
 */
class OnOff: public Control
{
  private:
    const bool neutral_setting_;

  public:
    OnOff(const OnOff &) = delete;
    OnOff(OnOff &&) = default;
    OnOff &operator=(const OnOff &) = delete;
    OnOff &operator=(OnOff &&) = default;

    explicit OnOff(const nlohmann::json &original_definition,
                   std::string &&id, std::string &&label,
                   std::string &&description,
                   const std::string &neutral_setting):
        Control(original_definition, std::move(id), std::move(label),
                std::move(description)),
        neutral_setting_(neutral_setting == "on")
    {
        if(neutral_setting != "on" && neutral_setting != "off")
            Error() << "Neutral setting for on_off control must be either "
                       "\"on\" or \"off\" in control \"" << id_ << "\"";
    }

    ConfigStore::ValueType get_value_type() const final override
    {
        return ConfigStore::ValueType::VT_BOOL;
    }

    bool is_neutral_value(const ConfigStore::Value &value) const final override
    {
        return
            value.is_of_type(ConfigStore::ValueType::VT_BOOL) &&
            value.get_value().get<bool>() == neutral_setting_;
    }

    unsigned int get_number_of_choices() const final override
    {
        return 2;
    }

    void for_each_choice(const ForEachChoiceFn &apply) const final override
    {
        apply(0, "off");
        apply(1, "on");
    }

    unsigned int to_selector_index(const ConfigStore::Value &value) const
        final override
    {

        if(value.is_of_type(ConfigStore::ValueType::VT_BOOL))
            return value.get_value().get<bool>() ? 1 : 0;

        if(value.is_of_type(ConfigStore::ValueType::VT_ASCIIZ))
        {
            const auto &s(value.get_value().get<std::string>());
            if(s == "off")
                return 0;
            else if(s == "on")
                return 1;
            else
                Error() << "String-type selector value for on_off must be "
                           "either \"on\" or \"off\"";
        }

        Error() << "Selector values for on_off must be boolean or string";
    }

    bool get_neutral_value() const { return neutral_setting_; }
};

}

}

#endif /* !ELEMENT_CONTROLS_HH */
