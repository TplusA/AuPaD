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

#include <doctest.h>

#include "signal_paths.hh"
#include "signal_path_tracker.hh"

#include "mock_messages.hh"

class Fixture
{
  protected:
    std::unique_ptr<MockMessages::Mock> mock_messages;

  public:
    explicit Fixture():
        mock_messages(std::make_unique<MockMessages::Mock>())
    {
        MockMessages::singleton = mock_messages.get();
    }

    ~Fixture()
    {
        try
        {
            mock_messages->done();
        }
        catch(...)
        {
            /* no throwing from dtors */
        }

        MockMessages::singleton = nullptr;
    }
};

TEST_SUITE_BEGIN("Static signal paths");

static bool append_path(
        std::vector<std::vector<const StaticModels::SignalPaths::PathElement *>> &paths,
        const ModelCompliant::SignalPathTracker::ActivePath &path,
        bool expect_single_path)
{
    if(expect_single_path)
        CHECK(paths.empty());

    std::vector<const StaticModels::SignalPaths::PathElement *> elements;
    std::transform(path.begin(), path.end(), std::back_inserter(elements),
                   [] (const auto &p) { return p.first; });
    paths.emplace_back(std::move(elements));

    return true;
}

static void expect_audio_path(
        const ModelCompliant::SignalPathTracker &tracker,
        const std::vector<const StaticModels::SignalPaths::PathElement *> &expected)
{
    std::vector<std::vector<const StaticModels::SignalPaths::PathElement *>> paths;
    CHECK(tracker.enumerate_active_signal_paths(
                [&paths] (const auto &p) { return append_path(paths, p, true); }));
    REQUIRE(paths.size() == 1);
    CHECK(paths[0] == expected);
}

static void expect_audio_paths(
        const ModelCompliant::SignalPathTracker &tracker,
        const std::vector<std::vector<const StaticModels::SignalPaths::PathElement *>> &expected)
{
    std::vector<std::vector<const StaticModels::SignalPaths::PathElement *>> paths;
    CHECK(tracker.enumerate_active_signal_paths(
                [&paths] (const auto &p) { return append_path(paths, p, false); }));
    REQUIRE(paths.size() == expected.size());

    std::sort(paths.begin(), paths.end());

    for(size_t i = 0; i < paths.size(); ++i)
        CHECK(paths[i] == expected[i]);
}

/*
 *              +------------------+
 *              | input_select     |
 *              +------------------+
 *              | in | [sel] | out |
 * source A --->| 0  |   3   |   0 |---> sink
 * source B --->| 1  |   1   |     |
 * source C --->| 2  |   5   |     |
 * source D --->| 3  |   2   |     |
 * source E --->| 4  |   0   |     |
 *        * --->| -  |   4   |     |
 *              +------------------+
 */
TEST_CASE_FIXTURE(Fixture, "Device with one mux element")
{
    using StaticModels::SignalPaths::Input;
    using StaticModels::SignalPaths::Output;
    using StaticModels::SignalPaths::Selector;

    StaticModels::SignalPaths::ApplianceBuilder builder("MyDevice");

    builder.add_element(StaticModels::SignalPaths::StaticElement("audio_source_a"));
    builder.add_element(StaticModels::SignalPaths::StaticElement("audio_source_b"));
    builder.add_element(StaticModels::SignalPaths::StaticElement("audio_source_c"));
    builder.add_element(StaticModels::SignalPaths::StaticElement("audio_source_d"));
    builder.add_element(StaticModels::SignalPaths::StaticElement("audio_source_e"));
    builder.add_element(StaticModels::SignalPaths::StaticElement("sink"));
    builder.add_element(StaticModels::SignalPaths::SwitchingElement::mk_mux(
            "input_select", "sel",
            {
                Input(4), Input(1), Input(3), Input(0),
                Input::mk_unconnected(), Input(2)
            }));
    builder.no_more_elements();

    auto &sel(builder.lookup_element("input_select"));

    builder.lookup_element("audio_source_a").connect(Output(0), sel, Input(0));
    builder.lookup_element("audio_source_b").connect(Output(0), sel, Input(1));
    builder.lookup_element("audio_source_c").connect(Output(0), sel, Input(2));
    builder.lookup_element("audio_source_d").connect(Output(0), sel, Input(3));
    builder.lookup_element("audio_source_e").connect(Output(0), sel, Input(4));
    sel.connect(Output(0), builder.lookup_element("sink"), Input(0));

    const auto dev(builder.build());

    const auto *const tmp1 = dev.lookup_element("input_select");
    REQUIRE(tmp1 != nullptr);
    CHECK(tmp1->get_name() == "input_select");
    const auto *const tmp2 = dev.lookup_switching_element("input_select");
    REQUIRE(tmp2 != nullptr);
    CHECK(tmp2->get_name() == "input_select");
    CHECK(tmp2->get_selector_name() == "sel");

    ModelCompliant::SignalPathTracker tracker(dev);

    CHECK(tracker.select("input_select", Selector(0)));
    std::vector<const StaticModels::SignalPaths::PathElement *> expected
    {
        dev.lookup_element("audio_source_e"),
        dev.lookup_element("input_select"),
        dev.lookup_element("sink"),
    };
    expect_audio_path(tracker, expected);

    CHECK(tracker.select("input_select", Selector(1)));
    expected[0] = dev.lookup_element("audio_source_b");
    expect_audio_path(tracker, expected);

    CHECK(tracker.select("input_select", Selector(2)));
    expected[0] = dev.lookup_element("audio_source_d");
    expect_audio_path(tracker, expected);

    CHECK(tracker.select("input_select", Selector(3)));
    expected[0] = dev.lookup_element("audio_source_a");
    expect_audio_path(tracker, expected);

    CHECK(tracker.select("input_select", Selector(4)));
    CHECK(tracker.enumerate_active_signal_paths(
            [] (const auto &p) { FAIL("unexpected"); return false; }));

    CHECK(tracker.select("input_select", Selector(5)));
    expected[0] = dev.lookup_element("audio_source_c");
    expect_audio_path(tracker, expected);

    CHECK(tracker.floating("input_select"));
    CHECK(tracker.enumerate_active_signal_paths(
            [] (const auto &p) { FAIL("unexpected"); return false; }));
}

/*
 *            +------------------+
 *            | output_select    |
 *            +------------------+
 *            | in | [sel] | out |
 * source --->| 0  |   2   |   0 |---> sink A
 *            |    |   3   |   1 |---> sink B
 *            |    |   0   |   2 |---> sink C
 *            |    |   1   |   - |---> *
 *            +------------------+
 */
TEST_CASE_FIXTURE(Fixture, "Device with one demux element")
{
    using StaticModels::SignalPaths::Input;
    using StaticModels::SignalPaths::Output;
    using StaticModels::SignalPaths::Selector;

    StaticModels::SignalPaths::ApplianceBuilder builder("MyDevice");

    builder.add_element(StaticModels::SignalPaths::StaticElement("audio_source"));
    builder.add_element(StaticModels::SignalPaths::StaticElement("sink_a"));
    builder.add_element(StaticModels::SignalPaths::StaticElement("sink_b"));
    builder.add_element(StaticModels::SignalPaths::StaticElement("sink_c"));
    builder.add_element(StaticModels::SignalPaths::SwitchingElement::mk_demux(
            "output_select", "sel",
            {
                Output(2), Output::mk_unconnected(), Output(0), Output(1)
            }));
    builder.no_more_elements();

    auto &sel(builder.lookup_element("output_select"));

    builder.lookup_element("audio_source").connect(Output(0), sel, Input(0));
    sel.connect(Output(0), builder.lookup_element("sink_a"), Input(0));
    sel.connect(Output(1), builder.lookup_element("sink_b"), Input(0));
    sel.connect(Output(2), builder.lookup_element("sink_c"), Input(0));

    const auto dev(builder.build());

    ModelCompliant::SignalPathTracker tracker(dev);

    CHECK(tracker.select("output_select", Selector(0)));
    std::vector<const StaticModels::SignalPaths::PathElement *> expected
    {
        dev.lookup_element("audio_source"),
        dev.lookup_element("output_select"),
        dev.lookup_element("sink_c"),
    };
    expect_audio_path(tracker, expected);

    CHECK(tracker.select("output_select", Selector(1)));
    CHECK(tracker.enumerate_active_signal_paths(
            [] (const auto &p) { FAIL("unexpected"); return false; }));

    CHECK(tracker.select("output_select", Selector(2)));
    expected[2] = dev.lookup_element("sink_a");
    expect_audio_path(tracker, expected);

    CHECK(tracker.select("output_select", Selector(3)));
    expected[2] = dev.lookup_element("sink_b");
    expect_audio_path(tracker, expected);
}

/*
 *              +-------------------------------+
 *              | switch                        |
 *              +-------------------------------+
 *              | in | [sel]              | out |
 * source A --->| 0  | 0: 0 -> 0          |   0 |---> sink A
 * source B --->| 1  | 1: 1 -> 1          |   1 |---> sink B
 *              |    | 2: 0 -> 0, 1 -> 1  |   2 |---> sink C
 *              |    | 3: 0 -> 1, 1 -> 0  |   3 |---> sink D
 *              |    | 4: 0 -> 1, 1 -> 2  |     |
 *              |    | 5: 1 -> 2          |     |
 *              |    | 6: 0 -> 0, 0 -> 1, |     |
 *              |    |    1 -> 2, 1 -> 3  |     |
 *              |    | 7: -               |     |
 *              +-------------------------------+
 *
 */
TEST_CASE_FIXTURE(Fixture, "Device with one table element")
{
    using StaticModels::SignalPaths::Input;
    using StaticModels::SignalPaths::Output;
    using StaticModels::SignalPaths::Selector;

    StaticModels::SignalPaths::ApplianceBuilder builder("MyDevice");

    builder.add_element(StaticModels::SignalPaths::StaticElement("audio_source_a"));
    builder.add_element(StaticModels::SignalPaths::StaticElement("audio_source_b"));
    builder.add_element(StaticModels::SignalPaths::StaticElement("sink_a"));
    builder.add_element(StaticModels::SignalPaths::StaticElement("sink_b"));
    builder.add_element(StaticModels::SignalPaths::StaticElement("sink_c"));
    builder.add_element(StaticModels::SignalPaths::StaticElement("sink_d"));
    builder.add_element(StaticModels::SignalPaths::SwitchingElement::mk_table(
            "switch", "sel",
            {
                /* sel 0 */
                {
                    { Input(0), Output(0) },
                },
                /* sel 1 */
                {
                    { Input(1), Output(1) },
                },
                /* sel 2 */
                {
                    { Input(0), Output(0) },
                    { Input(1), Output(1) },
                },
                /* sel 3 */
                {
                    { Input(0), Output(1) },
                    { Input(1), Output(0) },
                },
                /* sel 4 */
                {
                    { Input(0), Output(1) },
                    { Input(1), Output(2) },
                },
                /* sel 5 */
                {
                    { Input(1), Output(2) },
                },
                /* sel 6 */
                {
                    { Input(0), Output(0) },
                    { Input(0), Output(1) },
                    { Input(1), Output(2) },
                    { Input(1), Output(3) },
                },
                /* sel 7 */
                {},
            }));
    builder.no_more_elements();

    auto &sel(builder.lookup_element("switch"));

    builder.lookup_element("audio_source_a").connect(Output(0), sel, Input(0));
    builder.lookup_element("audio_source_b").connect(Output(0), sel, Input(1));
    sel.connect(Output(0), builder.lookup_element("sink_a"), Input(0));
    sel.connect(Output(1), builder.lookup_element("sink_b"), Input(0));
    sel.connect(Output(2), builder.lookup_element("sink_c"), Input(0));
    sel.connect(Output(3), builder.lookup_element("sink_d"), Input(0));

    const auto dev(builder.build());

    ModelCompliant::SignalPathTracker tracker(dev);

    CHECK(tracker.select("switch", Selector(0)));
    std::vector<const StaticModels::SignalPaths::PathElement *> expected
    {
        dev.lookup_element("audio_source_a"),
        dev.lookup_element("switch"),
        dev.lookup_element("sink_a"),
    };
    expect_audio_path(tracker, expected);
}

/*
 *          +------------------+
 *          | input A/B        |
 *          +------------------+
 *          | in | [sel] | out |      +------------------+
 * source A | 0  |   0   |   0 |--.   | input selector   |
 * source B | 1  |   1   |     |  |   +------------------+
 *          +------------------+  |   | in | [sel] | out |
 *                                `-->| 0  |   0   |   0 |--.
 * source C ------------------------->| 1  |   1   |     |  |
 *        * ------------------------->| -  |   2   |     |  |
 *                                    +------------------+  |
 *                                                          |
 *        ,-------------------------------------------------'
 *        |
 *        |   +------------------+
 *        |   | output selector  |
 *        |   +------------------+
 *        |   | in | [sel] | out |
 *        `-->| 0  |   0   |   0 |---> sink A
 *            |    |   1   |   1 |---> sink B
 *            +------------------+
 */
TEST_CASE_FIXTURE(Fixture, "Device with two mux and one demux elements")
{
    using StaticModels::SignalPaths::Input;
    using StaticModels::SignalPaths::Output;
    using StaticModels::SignalPaths::Selector;

    StaticModels::SignalPaths::ApplianceBuilder builder("MyDevice");

    builder.add_element(StaticModels::SignalPaths::StaticElement("source_A"));
    builder.add_element(StaticModels::SignalPaths::StaticElement("source_B"));
    builder.add_element(StaticModels::SignalPaths::StaticElement("source_C"));
    builder.add_element(StaticModels::SignalPaths::StaticElement("sink_A"));
    builder.add_element(StaticModels::SignalPaths::StaticElement("sink_B"));
    builder.add_element(StaticModels::SignalPaths::SwitchingElement::mk_mux(
            "input_ab", "sel_ab", { Input(0), Input(1) }));
    builder.add_element(StaticModels::SignalPaths::SwitchingElement::mk_mux(
            "input_sel", "sel_2nd", { Input(0), Input(1), Input::mk_unconnected() }));
    builder.add_element(StaticModels::SignalPaths::SwitchingElement::mk_demux(
            "output", "sel_out", { Output(0), Output(1) }));
    builder.no_more_elements();

    auto &input_ab(builder.lookup_element("input_ab"));
    auto &input_sel(builder.lookup_element("input_sel"));
    auto &output_sel(builder.lookup_element("output"));

    builder.lookup_element("source_A").connect(Output(0), input_ab, Input(0));
    builder.lookup_element("source_B").connect(Output(0), input_ab, Input(1));
    builder.lookup_element("source_C").connect(Output(0), input_sel, Input(1));
    input_ab.connect(Output(0), input_sel, Input(0));
    input_sel.connect(Output(0), output_sel, Input(0));
    output_sel.connect(Output(0), builder.lookup_element("sink_A"), Input(0));
    output_sel.connect(Output(1), builder.lookup_element("sink_B"), Input(0));

    const auto dev(builder.build());

    ModelCompliant::SignalPathTracker tracker(dev);

    CHECK(tracker.enumerate_active_signal_paths(
            [] (const auto &p) { FAIL("unexpected"); return false; }));

    CHECK(tracker.select("input_ab", Selector(0)));
    CHECK(tracker.select("input_sel", Selector(0)));
    CHECK(tracker.select("output", Selector(0)));
    std::vector<const StaticModels::SignalPaths::PathElement *> expected
    {
        dev.lookup_element("source_A"),
        dev.lookup_element("input_ab"),
        dev.lookup_element("input_sel"),
        dev.lookup_element("output"),
        dev.lookup_element("sink_A"),
    };
    expect_audio_path(tracker, expected);

    CHECK(tracker.select("input_sel", Selector(1)));
    expected =
    {
        dev.lookup_element("source_C"),
        dev.lookup_element("input_sel"),
        dev.lookup_element("output"),
        dev.lookup_element("sink_A"),
    };
    expect_audio_path(tracker, expected);

    CHECK(tracker.select("output", Selector(1)));
    expected =
    {
        dev.lookup_element("source_C"),
        dev.lookup_element("input_sel"),
        dev.lookup_element("output"),
        dev.lookup_element("sink_B"),
    };
    expect_audio_path(tracker, expected);

    CHECK(tracker.select("input_ab", Selector(1)));
    CHECK(tracker.select("input_sel", Selector(0)));
    expected =
    {
        dev.lookup_element("source_B"),
        dev.lookup_element("input_ab"),
        dev.lookup_element("input_sel"),
        dev.lookup_element("output"),
        dev.lookup_element("sink_B"),
    };
    expect_audio_path(tracker, expected);

    CHECK(tracker.floating("input_ab"));
    CHECK(tracker.enumerate_active_signal_paths(
            [] (const auto &p) { FAIL("unexpected"); return false; }));

    CHECK(tracker.select("input_sel", Selector(1)));
    expected =
    {
        dev.lookup_element("source_C"),
        dev.lookup_element("input_sel"),
        dev.lookup_element("output"),
        dev.lookup_element("sink_B"),
    };
    expect_audio_path(tracker, expected);
}

/*
 * +--------------+      +------------------+
 * | source       |--.   | output enable    |
 * +--------------+  |   +------------------+
 * | sub-source A |  |   | in | [sel] | out |
 * | sub-source B |  `-->| 0  |   1   |   0 |---> sink
 * | sub-source C |      |    |   0   |   - |---> *
 * +--------------+      +------------------+
 */
TEST_CASE_FIXTURE(Fixture, "Device with source comprising of three sub-sources")
{
    using StaticModels::SignalPaths::Input;
    using StaticModels::SignalPaths::Output;
    using StaticModels::SignalPaths::Selector;

    StaticModels::SignalPaths::ApplianceBuilder builder("MyDevice");

    builder.add_element(StaticModels::SignalPaths::StaticElement("source"));
    builder.add_element(StaticModels::SignalPaths::StaticElement("sub_A"));
    builder.add_element(StaticModels::SignalPaths::StaticElement("sub_B"));
    builder.add_element(StaticModels::SignalPaths::StaticElement("sub_C"));
    builder.add_element(StaticModels::SignalPaths::StaticElement("sink"));
    builder.add_element(StaticModels::SignalPaths::SwitchingElement::mk_demux(
            "output", "sel", { Output::mk_unconnected(), Output(0) }));
    builder.no_more_elements();

    auto &source(builder.lookup_element("source"));
    auto &output(builder.lookup_element("output"));

    builder.lookup_element("sub_A").connect_to_parent(Output(0), source);
    builder.lookup_element("sub_B").connect_to_parent(Output(0), source);
    builder.lookup_element("sub_C").connect_to_parent(Output(0), source);
    source.connect(Output(0), output, Input(0));
    output.connect(Output(0), builder.lookup_element("sink"), Input(0));

    const auto dev(builder.build());

    ModelCompliant::SignalPathTracker tracker(dev);

    CHECK(tracker.enumerate_active_signal_paths(
            [] (const auto &p) { FAIL("unexpected"); return false; }));

    CHECK(tracker.select("output", Selector(0)));
    CHECK(tracker.enumerate_active_signal_paths(
            [] (const auto &p) { FAIL("unexpected"); return false; }));

    CHECK(tracker.select("output", Selector(1)));
    std::vector<const StaticModels::SignalPaths::PathElement *> expected
    {
        dev.lookup_element("source"),
        dev.lookup_element("output"),
        dev.lookup_element("sink"),
    };
    expect_audio_path(tracker, expected);
}

/*!
 * source ---+--> sink A
 *           `--> sink B
 */
TEST_CASE_FIXTURE(Fixture, "Two sinks connected to the same source output")
{
    using StaticModels::SignalPaths::Input;
    using StaticModels::SignalPaths::Output;

    StaticModels::SignalPaths::ApplianceBuilder builder("MyDevice");

    builder.add_element(StaticModels::SignalPaths::StaticElement("source"));
    builder.add_element(StaticModels::SignalPaths::StaticElement("sink_A"));
    builder.add_element(StaticModels::SignalPaths::StaticElement("sink_B"));
    builder.no_more_elements();

    auto &source(builder.lookup_element("source"));
    source.connect(Output(0), builder.lookup_element("sink_A"), Input(0));
    source.connect(Output(0), builder.lookup_element("sink_B"), Input(0));

    const auto dev(builder.build());

    ModelCompliant::SignalPathTracker tracker(dev);

    std::vector<std::vector<const StaticModels::SignalPaths::PathElement *>> expected
    {
        {
            dev.lookup_element("source"),
            dev.lookup_element("sink_A"),
        },
        {
            dev.lookup_element("source"),
            dev.lookup_element("sink_B"),
        }
    };
    std::sort(expected.begin(), expected.end());
    expect_audio_paths(tracker, expected);
}

/*
 *            +------------------+
 *            | output_select    |
 *            +------------------+
 *            | in | [sel] | out |
 * source --->| 0  |   0   |   0 |---+---> sink
 *            |    |   1   |   1 |---'
 *            |    |   2   |   - |-------> *
 *            +------------------+
 */
TEST_CASE_FIXTURE(Fixture, "Two outputs connected to the same sink")
{
    using StaticModels::SignalPaths::Input;
    using StaticModels::SignalPaths::Output;
    using StaticModels::SignalPaths::Selector;

    StaticModels::SignalPaths::ApplianceBuilder builder("MyDevice");

    builder.add_element(StaticModels::SignalPaths::StaticElement("source"));
    builder.add_element(StaticModels::SignalPaths::StaticElement("sink"));
    builder.add_element(StaticModels::SignalPaths::SwitchingElement::mk_demux(
            "output_select", "sel",
            {
                Output(0), Output(1), Output::mk_unconnected()
            }));
    builder.no_more_elements();

    auto &sel(builder.lookup_element("output_select"));
    auto &sink(builder.lookup_element("sink"));
    builder.lookup_element("source").connect(Output(0), sel, Input(0));
    sel.connect(Output(0), sink, Input(0));
    sel.connect(Output(1), sink, Input(0));

    const auto dev(builder.build());

    ModelCompliant::SignalPathTracker tracker(dev);

    CHECK(tracker.select("output_select", Selector(0)));
    const std::vector<const StaticModels::SignalPaths::PathElement *> expected
    {
        dev.lookup_element("source"),
        dev.lookup_element("output_select"),
        dev.lookup_element("sink"),
    };
    expect_audio_path(tracker, expected);

    CHECK(tracker.select("output_select", Selector(1)));
    expect_audio_path(tracker, expected);

    CHECK(tracker.select("output_select", Selector(2)));
    CHECK(tracker.enumerate_active_signal_paths(
            [] (const auto &p) { FAIL("unexpected"); return false; }));
}

TEST_SUITE_END();
