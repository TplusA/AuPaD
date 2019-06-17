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

#ifndef SIGNAL_PATHS_HH
#define SIGNAL_PATHS_HH

#include "error.hh"
#include "messages.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <forward_list>
#include <set>
#include <functional>
#include <algorithm>
#include <memory>
#include <limits>

namespace StaticModels
{

namespace SignalPaths
{

class IndexBase
{
  protected:
    static constexpr auto INVALID = std::numeric_limits<unsigned int>::max();

    unsigned int value_;

    explicit IndexBase(unsigned int value): value_(value) {}

  public:
    IndexBase(const IndexBase &) = default;
    IndexBase(IndexBase &&) = default;
    IndexBase &operator=(const IndexBase &) = default;
    IndexBase &operator=(IndexBase &&) = default;

    virtual ~IndexBase() = default;

    bool is_valid() const { return value_ != INVALID; }
    unsigned int get() const { return value_; }

    IndexBase &operator++() { ++value_; return *this; }
};

class Input: public IndexBase
{
  public:
    explicit Input(unsigned int value): IndexBase(value) {}
    virtual ~Input() = default;

    static Input mk_unconnected() { return Input(INVALID); }

    bool operator==(const Input &other) const { return value_ == other.value_; }
    bool operator!=(const Input &other) const { return value_ != other.value_; }
    bool operator>=(const Input &other) const { return value_ >= other.value_; }
    bool operator<(const Input &other) const { return value_ < other.value_; }
};

class Output: public IndexBase
{
  public:
    explicit Output(unsigned int value): IndexBase(value) {}
    virtual ~Output() = default;

    static Output mk_unconnected() { return Output(INVALID); }

    bool operator==(const Output &other) const { return value_ == other.value_; }
    bool operator!=(const Output &other) const { return value_ != other.value_; }
    bool operator>=(const Output &other) const { return value_ >= other.value_; }
    bool operator<(const Output &other) const { return value_ < other.value_; }
};

class Selector: public IndexBase
{
  public:
    explicit Selector(unsigned int value): IndexBase(value) {}
    virtual ~Selector() = default;

    static Selector mk_invalid() { return Selector(INVALID); }

    bool operator==(const Selector &other) const { return value_ == other.value_; }
    bool operator!=(const Selector &other) const { return value_ != other.value_; }
    bool operator>=(const Selector &other) const { return value_ >= other.value_; }
};

class PathElement;

/*!
 * Representation of a signal path connection from an element to another
 * element.
 */
class OutgoingEdge
{
  private:
    Output from_pad_;
    Input to_pad_;
    const PathElement &to_elem_;

  public:
    OutgoingEdge(const OutgoingEdge &) = delete;
    OutgoingEdge(OutgoingEdge &&) = default;
    OutgoingEdge &operator=(const OutgoingEdge &) = delete;
    OutgoingEdge &operator=(OutgoingEdge &&) = default;

    explicit OutgoingEdge(Output from_pad,
                          Input to_pad, const PathElement &to_elem):
        from_pad_(from_pad),
        to_pad_(to_pad),
        to_elem_(to_elem)
    {
        log_assert(from_pad_.is_valid());
        log_assert(to_pad_.is_valid());
    }

    Output get_output_pad() const { return from_pad_; }
    Input get_target_input_pad() const { return to_pad_; }
    const PathElement &get_target_element() const { return to_elem_; }
};

/*!
 * Base class for any element on the signal path defined in the model.
 */
class PathElement
{
  public:
    enum class IterAction
    {
        EMPTY,
        DONE,
        ABORT,
        CONTINUE,
    };

  private:
    static constexpr auto UNREASONABLE = 50;

  protected:
    std::string name_;
    std::set<const PathElement *> sources_;
    std::forward_list<OutgoingEdge> all_outgoing_edges_;
    std::map<Output,
             std::unordered_map<std::string, const OutgoingEdge *>> edges_by_output_;
    const PathElement *parent_element_;

    explicit PathElement(std::string &&name):
        name_(std::move(name)),
        parent_element_(nullptr)
    {}

  public:
    PathElement(const PathElement &) = delete;
    PathElement(PathElement &&) = default;
    PathElement &operator=(const PathElement &) = delete;
    PathElement &operator=(PathElement &&) = default;
    virtual ~PathElement() = default;

    const std::string get_name() const { return name_; }

    IterAction for_each_output(
            const std::function<IterAction(const Output)> &apply) const
    {
        if(edges_by_output_.empty())
            return IterAction::EMPTY;

        for(const auto &it : edges_by_output_)
        {
            const auto temp(apply(it.first));
            if(temp != IterAction::CONTINUE)
                return temp;
        }

        return IterAction::DONE;
    }

    IterAction for_each_outgoing_edge(
            Output output,
            const std::function<IterAction(const OutgoingEdge &)> &apply) const
    {
        const auto &it(edges_by_output_.find(output));

        if(it == edges_by_output_.end())
            return IterAction::EMPTY;

        for(const auto &e : it->second)
        {
            const auto temp(apply(*e.second));
            if(temp != IterAction::CONTINUE)
                return temp;
        }

        return IterAction::DONE;
    }

    bool is_source() const { return sources_.empty(); }
    bool is_sink() const { return all_outgoing_edges_.empty(); }

    void connect(const Output &this_output_index,
                 PathElement &other, const Input &other_input_index)
    {
        if(!this_output_index.is_valid() || !other_input_index.is_valid())
        {
            BUG("Tried connecting %s to %s using bad index (%u -> %u)",
                name_.c_str(), other.name_.c_str(),
                this_output_index.get(), other_input_index.get());
            return;
        }
        else if(this_output_index >= Output(UNREASONABLE) ||
                other_input_index >= Input(UNREASONABLE))
        {
            BUG("Unreasonably large index when trying to connect "
                "%s to %s (%u -> %u) [connection ignored]",
                name_.c_str(), other.name_.c_str(),
                this_output_index.get(), other_input_index.get());
            return;
        }

        const auto edges(edges_by_output_.find(this_output_index));
        if(edges != edges_by_output_.end() &&
           edges->second.find(other.name_) != edges->second.end())
        {
            BUG("Duplicate edge from %s.%u to %s.%u",
                name_.c_str(), this_output_index.get(),
                other.name_.c_str(), other_input_index.get());
            return;
        }

        all_outgoing_edges_.emplace_front(
            OutgoingEdge(this_output_index, other_input_index, other));
        edges_by_output_[this_output_index][other.name_] = &all_outgoing_edges_.front();
        other.sources_.insert(this);
    }

    void connect_to_parent(const Output &this_output_index,
                           PathElement &other)
    {
        if(&other == this)
            Error() << "Path element cannot be its own parent (" << name_ << ")";

        parent_element_ = &other;
    }

    const bool is_sub_element() const { return parent_element_ != nullptr; }

    virtual void finalize(const std::string &device_id) const
    {
        if(sources_.empty() && all_outgoing_edges_.empty() && parent_element_ == nullptr)
            msg_error(0, LOG_NOTICE,
                      "Element %s.%s is unconnected",
                      device_id.c_str(), name_.c_str());
    }
};

/*!
 * Elements with a static, direct input to output relation.
 *
 * Most elements with a single input and a single output will be stored in
 * objects of this class.
 */
class StaticElement: public PathElement
{
  public:
    StaticElement(StaticElement &&) = default;
    StaticElement &operator=(StaticElement &&) = default;

    explicit StaticElement(std::string &&name):
        PathElement(std::move(name))
    {}

    virtual ~StaticElement() = default;
};

static std::string append_fqname(
        const std::string &device_id, const std::string &element_id,
        const std::string &control_id)
{
    return " (" + device_id + '.' + element_id + '.' + control_id + ')';
}

/*!
 * I/O mapping for all possible selector assignments.
 *
 * Conceptually, such a mapping is a set of C binary NxM matrices, where N is
 * the number of inputs, M the number of outputs, and C the number of choices
 * for the selector. Element (n, m) in the c-th matrix is 1 if input n is to be
 * routed to output m while the selector set to c, otherwise it is 0.
 *
 * Frequently, these matrices are very specific and sparse, so they are not
 * necessarily stored as matrices. Their concrete storage scheme is implemented
 * in derived classes.
 */
class Mapping
{
  protected:
    explicit Mapping() = default;

  public:
    Mapping(const Mapping &) = delete;
    Mapping(Mapping &&) = default;
    Mapping &operator=(const Mapping &) = delete;
    Mapping &operator=(Mapping &&) = default;

    virtual ~Mapping() = default;

    virtual void finalize(const std::string &device_id,
                          const std::string &element_id,
                          const std::string &control_id,
                          unsigned int num_of_inputs,
                          unsigned int num_of_outputs) const = 0;

    virtual unsigned int number_of_choices() const = 0;

    virtual bool is_connected(const Selector &sel,
                              const Input &in, const Output &out) const = 0;
};

template <typename T>
static void throw_on_invalid_mux_demux_values(
        const std::vector<T> &values, unsigned int number_of_pads,
        const char *elem_kind, const char *context,
        const std::string &device_id, const std::string &element_id,
        const std::string &control_id)
{
    if(number_of_pads > 0 &&
       std::any_of(values.begin(), values.end(),
            [maximum_value = number_of_pads - 1] (const auto &v)
            {
                return v.is_valid() && v.get() > maximum_value;
            }))
        Error() << context << ": " << elem_kind
                << " mapping contains values greater than "
                << (number_of_pads - 1)
                << append_fqname(device_id, element_id, control_id);

    if(values.size() < 2)
        Error() << context << ": empty mapping"
                << append_fqname(device_id, element_id, control_id);
}

/*!
 * I/O mapping: one out of many to one.
 *
 * For each possible value of the selector, the index of its designated input
 * is stored. These indices can be assigned in any order and may also refer to
 * no input at all (check #StaticModels::SignalPaths::Input::is_valid()).
 */
class MappingMux: public Mapping
{
  private:
    const std::vector<Input> input_by_selector_;

  public:
    explicit MappingMux(std::vector<Input> &&m):
        input_by_selector_(std::move(m))
    {}

    virtual ~MappingMux() = default;

    void finalize(const std::string &device_id, const std::string &element_id,
                  const std::string &control_id, unsigned int num_of_inputs,
                  unsigned int num_of_outputs) const final override
    {
        throw_on_invalid_mux_demux_values(input_by_selector_, num_of_inputs,
                                          "input", "MappingMux",
                                          device_id, element_id, control_id);

        if(num_of_outputs != 1)
            Error() << "MappingMux: number of outputs must be 1, but have "
                    << num_of_outputs
                    << append_fqname(device_id, element_id, control_id);
    }

    unsigned int number_of_choices() const final override
    {
        return input_by_selector_.size();
    }

    bool is_connected(const Selector &sel,
                      const Input &in, const Output &out) const
        final override
    {
        if(out != Output(0) || !in.is_valid())
            return false;

        const auto from = input_by_selector_.at(sel.get());
        return from == in;
    }
};

/*!
 * I/O mapping: one to one out of many.
 *
 * For each possible value of the selector, its index of its designated output
 * is stored. These indices can be assigned in any order and may also refer to
 * no output at all (check #StaticModels::SignalPaths::Output::is_valid()).
 */
class MappingDemux: public Mapping
{
  private:
    const std::vector<Output> output_by_selector_;

  public:
    explicit MappingDemux(std::vector<Output> &&m):
        output_by_selector_(std::move(m))
    {}

    virtual ~MappingDemux() = default;

    void finalize(const std::string &device_id, const std::string &element_id,
                  const std::string &control_id, unsigned int num_of_inputs,
                  unsigned int num_of_outputs) const final override
    {
        throw_on_invalid_mux_demux_values(output_by_selector_, num_of_outputs,
                                          "output", "MappingDemux",
                                          device_id, element_id, control_id);

        if(num_of_inputs != 1)
            Error() << "MappingDemux: number of inputs must be 1, but have "
                    << num_of_inputs
                    << append_fqname(device_id, element_id, control_id);
    }

    unsigned int number_of_choices() const final override
    {
        return output_by_selector_.size();
    }

    bool is_connected(const Selector &sel,
                      const Input &in, const Output &out) const
        final override
    {
        if(in != Input(0) || !out.is_valid())
            return false;

        const auto to = output_by_selector_.at(sel.get());
        return out == to;
    }
};

/*!
 * I/O mapping: free mapping based on LUT.
 */
class MappingTable: public Mapping
{
  public:
    using Table = std::set<std::pair<Input, Output>>;

  private:
    std::vector<Table> tables_;

  public:
    explicit MappingTable(std::vector<Table> &&m):
        tables_(std::move(m))
    {}

    virtual ~MappingTable() = default;

    void finalize(const std::string &device_id, const std::string &element_id,
                  const std::string &control_id, unsigned int num_of_inputs,
                  unsigned int num_of_outputs) const final override
    {
        bool found_edges = false;

        for(const auto &table : tables_)
        {
            for(const auto &edge : table)
            {
                if(edge.first >= Input(num_of_inputs))
                    Error()
                        << "MappingTable: table contains input values greater than "
                        << (num_of_inputs - 1)
                        << append_fqname(device_id, element_id, control_id);

                if(edge.second >= Output(num_of_outputs))
                    Error()
                        << "MappingTable: table contains output values greater than "
                        << (num_of_outputs - 1)
                        << append_fqname(device_id, element_id, control_id);

                found_edges = true;
            }
        }

        if(!found_edges)
            Error() << "MappingTable: empty mapping"
                    << append_fqname(device_id, element_id, control_id);
    }

    unsigned int number_of_choices() const final override
    {
        return tables_.size();
    }

    bool is_connected(const Selector &sel,
                      const Input &in, const Output &out) const
        final override
    {
        if(!in.is_valid() || !out.is_valid())
            return false;

        try
        {
            const auto &table(tables_.at(sel.get()));
            return table.find({in, out}) != table.end();
        }
        catch(const std::out_of_range &e)
        {
            return false;
        }
    }
};

/*!
 * Elements for which an I/O mapping is defined.
 *
 * These objects store the name of the element's control which selects a
 * specific I/O mapping; this control is called the selector. Each element can
 * have at most one selector. For each possible value of the selector, a
 * #StaticModels::SignalPaths::Mapping object is stored.
 */
class SwitchingElement: public PathElement
{
  private:
    std::string selector_;
    std::unique_ptr<Mapping> mapping_;

    explicit SwitchingElement(std::string &&element_name,
                              std::string &&selector_name,
                              std::unique_ptr<Mapping> mapping):
        PathElement(std::move(element_name)),
        selector_(std::move(selector_name)),
        mapping_(std::move(mapping))
    {
        if(mapping_ == nullptr)
            Error() << "Mapping not provided";
    }

  public:
    SwitchingElement(SwitchingElement &&) = default;
    SwitchingElement &operator=(SwitchingElement &&) = default;

    virtual ~SwitchingElement() = default;

    void finalize(const std::string &device_id) const final override
    {
        PathElement::finalize(device_id);
        mapping_->finalize(device_id, name_, selector_,
                           sources_.size(), edges_by_output_.size());
    }

    const std::string &get_selector_name() const { return selector_; }

    bool is_selector_in_range(const Selector &sel) const
    {
        return sel.is_valid() && sel.get() < mapping_->number_of_choices();
    }

    bool is_connected(const Selector &sel, const Input &in, const Output &out) const
    {
        return mapping_->is_connected(sel, in, out);
    }

    static SwitchingElement mk_mux(std::string &&element_name,
                                   std::string &&selector_name,
                                   std::vector<Input> &&m)
    {
        return SwitchingElement(std::move(element_name), std::move(selector_name),
                                std::make_unique<MappingMux>(std::move(m)));
    }

    static SwitchingElement mk_demux(std::string &&element_name,
                                     std::string &&selector_name,
                                     std::vector<Output> &&m)
    {
        return SwitchingElement(std::move(element_name), std::move(selector_name),
                                std::make_unique<MappingDemux>(std::move(m)));
    }

    static SwitchingElement mk_table(std::string &&element_name,
                                     std::string &&selector_name,
                                     std::vector<MappingTable::Table> &&m)
    {
        return SwitchingElement(std::move(element_name), std::move(selector_name),
                                std::make_unique<MappingTable>(std::move(m)));
    }
};

/*!
 * Static signal path graph as defined for an appliance.
 *
 * Note that this object describes the whole static signal path defined for one
 * appliance type, not the active path inside a specific instance.
 */
class Appliance
{
  private:
    std::string name_;
    std::vector<StaticElement> static_elements_;
    std::vector<SwitchingElement> switching_elements_;
    std::unordered_map<std::string, PathElement &> elements_by_name_;

  public:
    Appliance(const Appliance &) = delete;
    Appliance(Appliance &&) = default;
    Appliance &operator=(const Appliance &) = delete;
    Appliance &operator=(Appliance &&) = default;

    explicit Appliance(std::string &&name,
                       std::vector<StaticElement> &&static_elements,
                       std::vector<SwitchingElement> &&switching_elements,
                       std::unordered_map<std::string, PathElement &> &&elements_by_name):
        name_(std::move(name)),
        static_elements_(std::move(static_elements)),
        switching_elements_(std::move(switching_elements)),
        elements_by_name_(std::move(elements_by_name))
    {}

    const std::string &get_name() const { return name_; }

    void for_each_source(const std::function<void(const PathElement &src)> &apply) const
    {
        for(const auto &elem : static_elements_)
            if(elem.is_source())
                apply(elem);

        for(const auto &elem : switching_elements_)
            if(elem.is_source())
                apply(elem);
    }

    const PathElement *lookup_element(const std::string &name) const
    {
        try
        {
            return &elements_by_name_.at(name);
        }
        catch(const std::out_of_range &e)
        {
            return nullptr;
        }
    }

    const SwitchingElement *lookup_switching_element(const std::string &name) const
    {
        return dynamic_cast<const SwitchingElement *>(lookup_element(name));
    }
};

/*!
 * Builder for #StaticModels::SignalPaths::Appliance objects.
 */
class ApplianceBuilder
{
  private:
    std::string name_;
    std::vector<StaticElement> static_elements_;
    std::vector<SwitchingElement> switching_elements_;
    std::unordered_map<std::string, PathElement &> elements_by_name_;
    bool is_adding_elements_allowed_;

  public:
    ApplianceBuilder(const ApplianceBuilder &) = delete;

    explicit ApplianceBuilder(std::string &&name):
        name_(std::move(name)),
        is_adding_elements_allowed_(true)
    {}

    void add_element(StaticElement &&elem)
    {
        if(is_adding_elements_allowed_)
            static_elements_.emplace_back(std::move(elem));
        else
            Error() << "Adding StaticElement element not allowed";
    }

    void add_element(SwitchingElement &&elem)
    {
        if(is_adding_elements_allowed_)
            switching_elements_.emplace_back(std::move(elem));
        else
            Error() << "Adding SwitchingElement element not allowed";
    }

    PathElement &lookup_element(const std::string &name) const
    {
        return elements_by_name_.at(name);
    }

    void no_more_elements()
    {
        if(!is_adding_elements_allowed_)
            return;

        is_adding_elements_allowed_ = false;

        for(auto &e : static_elements_)
            if(!elements_by_name_.insert({e.get_name(), e}).second)
                Error() << "Duplicate element name \"" << e.get_name() << "\"";

        for(auto &e : switching_elements_)
            if(!elements_by_name_.insert({e.get_name(), e}).second)
                Error() << "Duplicate element name \"" << e.get_name() << "\"";
    }

    Appliance build()
    {
        no_more_elements();

        for(auto &e : elements_by_name_)
            e.second.finalize(name_);

        return Appliance(std::move(name_),
                         std::move(static_elements_),
                         std::move(switching_elements_),
                         std::move(elements_by_name_));
    }
};

}

}

#endif /* !SIGNAL_PATHS_HH */
