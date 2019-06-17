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

#include "signal_path_tracker.hh"

bool ModelCompliant::SignalPathTracker::select(
        const std::string &element_name,
        const StaticModels::SignalPaths::Selector &sel)
{
    const auto *const elem = dev_.lookup_switching_element(element_name);

    if(elem == nullptr)
    {
        APPLIANCE_BUG("Cannot select nonexistent switching element %s in %s",
                      element_name.c_str(), dev_.get_name().c_str());
        return false;
    }

    if(!elem->is_selector_in_range(sel))
    {
        BUG("Selector value %u out of range for %s.%s",
            sel.get(), elem->get_name().c_str(),
            elem->get_selector_name().c_str());
        return false;
    }

    auto it(selector_values_.find(elem));

    if(it == selector_values_.end())
    {
        selector_values_.insert({elem, sel});
        return true;
    }

    if(it->second != sel)
    {
        it->second = sel;
        return true;
    }

    return false;
}

bool ModelCompliant::SignalPathTracker::floating(const std::string &element_name)
{
    const auto *const elem = dev_.lookup_switching_element(element_name);

    if(elem == nullptr)
    {
        APPLIANCE_BUG("Cannot float nonexistent switching element %s in %s",
                      dev_.get_name().c_str(), element_name.c_str());
        return false;
    }

    return selector_values_.erase(elem) > 0;
}

class DepthFirst
{
  public:
    enum class TraverseAction
    {
        CONTINUE,
        SKIP,
        ABORT,
    };

    using TraverseCallbackFn =
        std::function<TraverseAction(
                        const StaticModels::SignalPaths::PathElement *parent,
                        const StaticModels::SignalPaths::PathElement &elem,
                        const StaticModels::SignalPaths::Input &elem_input_index,
                        const StaticModels::SignalPaths::Output &elem_output_index,
                        unsigned int depth)>;

  private:
    const ModelCompliant::SignalPathTracker &tracker_;
    const TraverseCallbackFn &apply_;
    unsigned int depth_;

  public:
    DepthFirst(const DepthFirst &) = delete;

    explicit DepthFirst(const ModelCompliant::SignalPathTracker &tracker,
                        unsigned int depth, const TraverseCallbackFn &apply):
        tracker_(tracker),
        apply_(apply),
        depth_(depth)
    {}

    bool traverse(const std::vector<std::pair<const StaticModels::SignalPaths::PathElement *, bool>> &sources,
                  bool is_root_device)
    {
        for(const auto &source : sources)
        {
            /* only traverse from active sources */
            if(!is_root_device && !source.second)
                continue;

            /* FIXME: sub-elements need to be handled properly */
            if(source.first->is_sub_element())
                continue;

            switch(apply_(nullptr, *source.first,
                          StaticModels::SignalPaths::Input::mk_unconnected(),
                          StaticModels::SignalPaths::Output(0),
                          depth_))
            {
              case TraverseAction::CONTINUE:
                if(!down(*source.first, StaticModels::SignalPaths::Output(0)))
                    return false;

                break;

              case TraverseAction::SKIP:
                break;

              case TraverseAction::ABORT:
                return false;
            }
        }

        return true;
    }

    bool down(const StaticModels::SignalPaths::PathElement &elem,
              const StaticModels::SignalPaths::Output &elem_target_index)
    {
        using IterAction = StaticModels::SignalPaths::PathElement::IterAction;

        ++depth_;

        const auto process_edges_result =
            elem.for_each_outgoing_edge(elem_target_index,
            [this, &elem, &elem_target_index]
            (const auto &outedge)
            {
                const auto &target(outedge.get_target_element());
                const auto target_input_index = outedge.get_target_input_pad();

                if(target.is_sink())
                {
                    /* found a sink */
                    switch(apply_(&elem, target, target_input_index,
                                  StaticModels::SignalPaths::Output::mk_unconnected(),
                                  depth_))
                    {
                      case TraverseAction::CONTINUE:
                        return IterAction::CONTINUE;

                      case TraverseAction::SKIP:
                        return IterAction::DONE;

                      case TraverseAction::ABORT:
                        break;
                    }

                    return IterAction::ABORT;
                }

                const auto result = target.for_each_output(
                    [this, &elem, &target, &target_input_index]
                    (const auto target_output_index)
                    {
                        switch(apply_(&elem, target, target_input_index,
                                      target_output_index, depth_))
                        {
                          case TraverseAction::CONTINUE:
                            if(!down(target, target_output_index))
                                return IterAction::ABORT;

                            break;

                          case TraverseAction::SKIP:
                            break;

                          case TraverseAction::ABORT:
                            return IterAction::ABORT;
                        }

                        return IterAction::CONTINUE;
                    });

                return result == IterAction::DONE ? IterAction::CONTINUE : result;
            });

        --depth_;

        switch(process_edges_result)
        {
          case IterAction::EMPTY:
          case IterAction::DONE:
            return true;

          case IterAction::ABORT:
            break;

          case IterAction::CONTINUE:
            BUG("Unexpected edge processing result");
            break;
        }

        return false;
    }
};

static DepthFirst::TraverseAction
collect(const StaticModels::SignalPaths::PathElement *parent,
        const StaticModels::SignalPaths::PathElement &elem,
        const StaticModels::SignalPaths::Input &elem_input_index,
        const StaticModels::SignalPaths::Output &elem_output_index,
        unsigned int depth,
        const ModelCompliant::SignalPathTracker &tracker,
        ModelCompliant::SignalPathTracker::ActivePath &path)
{
    const auto *sw =
        dynamic_cast<const StaticModels::SignalPaths::SwitchingElement *>(&elem);

    if(sw != nullptr)
    {
        const StaticModels::SignalPaths::Selector sel = tracker.get_selector_value(sw);

        if(!sel.is_valid())
            return DepthFirst::TraverseAction::SKIP;

        if(!sw->is_connected(sel, elem_input_index, elem_output_index))
            return DepthFirst::TraverseAction::SKIP;
    }

    if(depth <= path.size())
        path.resize(depth);

    path.emplace_back(&elem, false);
    return DepthFirst::TraverseAction::CONTINUE;
}

bool ModelCompliant::SignalPathTracker::enumerate_active_signal_paths(
        const EnumerateCallbackFn &fn, bool is_root_device) const
{
    std::vector<std::pair<const StaticModels::SignalPaths::PathElement *, bool>> path;

    return DepthFirst(*this, 0,
        [this, &fn, &path]
        (const auto *parent, const auto &elem,
         const auto &elem_input_index, const auto &elem_output_index,
         unsigned int depth)
        {
            const auto collect_result =
                collect(parent, elem, elem_input_index, elem_output_index,
                        depth, *this, path);

            if(collect_result == DepthFirst::TraverseAction::CONTINUE &&
               elem.is_sink())
            {
                if(!fn(path))
                    return DepthFirst::TraverseAction::ABORT;

                for(auto &p : path)
                    p.second = true;
            }

            return collect_result;
        }).traverse(sources_, is_root_device);
}
