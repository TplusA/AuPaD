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

#include "live_data.hh"

bool LiveData::SignalPathTracker::select(
        const std::string &element_name,
        const StaticModels::SignalPaths::Selector &sel)
{
    const auto *const elem = dev_.lookup_switching_element(element_name);

    if(elem == nullptr)
    {
        APPLIANCE_BUG("Cannot select nonexistent switching element %s in %s",
                      dev_.get_name().c_str(), element_name.c_str());
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

bool LiveData::SignalPathTracker::floating(const std::string &element_name)
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
    const LiveData::SignalPathTracker &tracker_;
    const TraverseCallbackFn &apply_;
    unsigned int depth_;

  public:
    DepthFirst(const DepthFirst &) = delete;

    explicit DepthFirst(const LiveData::SignalPathTracker &tracker,
                        unsigned int depth, const TraverseCallbackFn &apply):
        tracker_(tracker),
        apply_(apply),
        depth_(depth)
    {}

    bool traverse(const std::vector<std::pair<const StaticModels::SignalPaths::PathElement *, bool>> &sources)
    {
        for(const auto &source : sources)
        {
            /* only traverse from active sources */
            if(!source.second)
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
        const auto *target = elem.get_targets()[elem_target_index.get()];
        if(target == nullptr)
            return true;

        const auto target_input_index(target->find_parent_index(elem));
        if(!target_input_index.is_valid())
        {
            BUG("Failed to find input index in %s from parent %s",
                target->get_name().c_str(), elem.get_name().c_str());
            return false;
        }

        ++depth_;

        const auto &target_targets(target->get_targets());

        if(target_targets.empty())
        {
            /* found a sink */
            switch(apply_(&elem, *target, target_input_index,
                          StaticModels::SignalPaths::Output::mk_unconnected(),
                          depth_))
            {
              case TraverseAction::CONTINUE:
              case TraverseAction::SKIP:
                break;

              case TraverseAction::ABORT:
                --depth_;
                return false;
            }
        }
        else
        {
            StaticModels::SignalPaths::Output target_output_index(0);

            for(const auto &t : target_targets)
            {
                if(t == nullptr)
                {
                    ++target_output_index;
                    continue;
                }

                switch(apply_(&elem, *target, target_input_index,
                            target_output_index, depth_))
                {
                  case TraverseAction::CONTINUE:
                    if(!down(*target, target_output_index))
                    {
                        --depth_;
                        return false;
                    }

                    break;

                  case TraverseAction::SKIP:
                    break;

                  case TraverseAction::ABORT:
                    --depth_;
                    return false;
                }

                ++target_output_index;
            }
        }

        --depth_;
        return true;
    }
};

static DepthFirst::TraverseAction
collect(const StaticModels::SignalPaths::PathElement *parent,
        const StaticModels::SignalPaths::PathElement &elem,
        const StaticModels::SignalPaths::Input &elem_input_index,
        const StaticModels::SignalPaths::Output &elem_output_index,
        unsigned int depth,
        const LiveData::SignalPathTracker &tracker,
        LiveData::SignalPathTracker::ActivePath &path)
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

bool LiveData::SignalPathTracker::enumerate_active_signal_paths(const EnumerateCallbackFn &fn) const
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
        }).traverse(sources_);
}
