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

#include <doctest.h>

#include "configstore.hh"
#include "configstore_json.hh"
#include "configstore_changes.hh"

#include "mock_messages.hh"

TEST_SUITE_BEGIN("Configuration store");

class Fixture
{
  protected:
    ConfigStore::Settings settings;
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

  protected:
    auto diff(const nlohmann::json &expected)
    {
        return nlohmann::json::diff(nlohmann::json::parse(settings.json_string()),
                                    expected);
    }

    void expect_equal(const nlohmann::json &expected,
                      ConfigStore::SettingsJSON *js = nullptr)
    {
        const auto d(diff(expected));

        if(d.empty())
            return;

        MESSAGE("Diff: " << d);

        if(js == nullptr)
            CHECK(settings.json_string() == expected);
        else
            CHECK(js->json() == expected);
    }

    void bunch_of_connected_instances(bool clear_change_log = false);

    template <size_t N>
    void check_disconnected_connections(
        const std::array<const std::pair<std::string, std::string>, N> &expected_connections)
    {
        ConfigStore::Changes changes;
        {
        ConfigStore::SettingsJSON js(settings);
        CHECK(js.extract_changes(changes));
        }

        std::vector<std::pair<std::string, std::string>> reported_connections;

        changes.for_each_changed_connection(
            [&reported_connections]
            (const auto &from, const auto &to, bool was_added)
            {
                CHECK_FALSE(was_added);
                reported_connections.push_back({from, to});
            });

        std::sort(reported_connections.begin(), reported_connections.end());

        std::vector<std::pair<const std::string, const std::string>> diff;
        std::set_symmetric_difference(
            expected_connections.begin(), expected_connections.end(),
            reported_connections.begin(), reported_connections.end(),
            std::back_inserter(diff));

        CHECK(reported_connections.size() == expected_connections.size());
        CHECK(diff.empty());
    }
};

TEST_CASE_FIXTURE(Fixture, "Newly created configuration store is empty")
{
    expect_equal(nlohmann::json({}));

    ConfigStore::Changes changes;
    {
    ConfigStore::SettingsJSON js(settings);
    CHECK_FALSE(js.extract_changes(changes));
    }

    bool called = false;
    changes.for_each_changed_device([&called] (const auto &, bool) { called = true; });
    REQUIRE_FALSE(called);
    changes.for_each_changed_connection(
        [&called] (const auto &, const auto &, bool) { called = true; });
    REQUIRE_FALSE(called);
    changes.for_each_changed_value(
        [&called] (const auto &, const auto &, const auto &) { called = true; });
    REQUIRE_FALSE(called);
}

TEST_CASE_FIXTURE(Fixture, "Single unconfigured instance")
{
    const std::string input = R"(
        {
            "audio_path_changes": [
                { "op": "add_instance", "name": "self", "id": "MP3100HV" }
            ]
        })";
    settings.update(input);

    const auto expected_json = R"({ "devices": { "self": "MP3100HV" }})"_json;
    expect_equal(expected_json);
}

TEST_CASE_FIXTURE(Fixture, "Single unconfigured instance through JSON settings")
{
    ConfigStore::SettingsJSON js(settings);

    nlohmann::json input;
    input["audio_path_changes"].push_back(
        {
            { "op", "add_instance" },
            { "name", "self" },
            { "id", "MP3100HV" }
        });
    js.update(input);

    const nlohmann::json expected_json = { { "devices", { { "self", "MP3100HV" } } } };
    expect_equal(expected_json, &js);
}

TEST_CASE_FIXTURE(Fixture, "Two unconfigured instances")
{
    const std::string input = R"(
        {
            "audio_path_changes": [
                { "op": "add_instance", "name": "self", "id": "MP3100HV" },
                { "op": "add_instance", "name": "pa", "id": "PA3000HV" }
            ]
        })";
    settings.update(input);

    const auto expected_json = R"(
        {
            "devices": {
                "self": "MP3100HV",
                "pa": "PA3000HV"
            }
        })"_json;
    expect_equal(expected_json);
}

TEST_CASE_FIXTURE(Fixture, "Three unconfigured instances")
{
    const std::string input = R"(
        {
            "audio_path_changes": [
                { "op": "add_instance", "name": "self", "id": "MP3100HV" },
                { "op": "add_instance", "name": "a", "id": "PA3000HV" },
                { "op": "add_instance", "name": "b", "id": "PA3000HV" }
            ]
        })";
    settings.update(input);

    const auto expected_json = R"(
        {
            "devices": {
                "self": "MP3100HV",
                "a": "PA3000HV",
                "b": "PA3000HV"
            }
        })"_json;
    expect_equal(expected_json);
}

TEST_CASE_FIXTURE(Fixture, "Remove one out of three unconfigured instances")
{
    const std::string input1 = R"(
        {
            "audio_path_changes": [
                { "op": "add_instance", "name": "self", "id": "MP3100HV" },
                { "op": "add_instance", "name": "a", "id": "PA3000HV" },
                { "op": "add_instance", "name": "b", "id": "PA2000R" }
            ]
        })";
    settings.update(input1);

    const std::string input2 = R"(
        {
            "audio_path_changes": [
                { "op": "rm_instance", "name": "a" }
            ]
        })";
    settings.update(input2);

    const auto expected_json = R"(
        {
            "devices": {
                "self": "MP3100HV",
                "b": "PA2000R"
            }
        })"_json;
    expect_equal(expected_json);
}

TEST_CASE_FIXTURE(Fixture, "Clear all instances")
{
    const std::string input1 = R"(
        {
            "audio_path_changes": [
                { "op": "add_instance", "name": "self", "id": "R1000E" }
            ]
        })";
    settings.update(input1);

    const std::string input2 = R"(
        { "audio_path_changes": [ { "op": "clear_instances" } ] })";
    settings.update(input2);

    expect_equal(nlohmann::json({}));
}

TEST_CASE_FIXTURE(Fixture, "Full initial audio path information")
{
    const std::string input = R"(
        {
            "audio_path_changes": [
                { "op": "clear_instances" },
                { "op": "add_instance", "name": "self", "id": "MP3100HV" },
                {
                    "op": "set", "element": "self.dsp",
                    "kv": {
                        "filter": { "type": "s", "value": "iir_bezier" },
                        "phase_invert": { "type": "b", "value": true }
                    }
                },
                {
                    "op": "set", "element": "self.dsd_out_filter",
                    "kv": {
                        "mode": { "type": "s", "value": "normal" }
                    }
                },
                {
                    "op": "set", "element": "self.whatever",
                    "kv": {
                        "my_param": { "type": "n", "value": -6000 }
                    }
                }
            ]
        })";
    settings.update(input);

    const auto expected_json = R"(
        {
            "devices": { "self": "MP3100HV" },
            "settings": {
                "self": {
                    "dsp": {
                        "filter": { "type": "s", "value": "iir_bezier" },
                        "phase_invert": { "type": "b", "value": true }
                    },
                    "dsd_out_filter": {
                        "mode": { "type": "s", "value": "normal" }
                    },
                    "whatever": {
                        "my_param": { "type": "n", "value": -6000 }
                    }
                }
            }
        })"_json;
    expect_equal(expected_json);
}

TEST_CASE_FIXTURE(Fixture, "Update single value after initial audio path information")
{
    const std::string input1 = R"(
        {
            "audio_path_changes": [
                { "op": "clear_instances" },
                { "op": "add_instance", "name": "self", "id": "MP3100HV" },
                {
                    "op": "set", "element": "self.dsp",
                    "kv": {
                        "filter": { "type": "s", "value": "iir_bezier" },
                        "phase_invert": { "type": "b", "value": true }
                    }
                },
                {
                    "op": "set", "element": "self.dsd_out_filter",
                    "kv": {
                        "mode": { "type": "s", "value": "normal" }
                    }
                }
            ]
        })";
    settings.update(input1);

    const std::string input2 = R"(
        {
            "audio_path_changes": [
                {
                    "op": "update", "element": "self.dsp",
                    "kv": {
                        "filter": { "type": "s", "value": "fir_long" }
                    }
                }
            ]
        })";
    settings.update(input2);

    const auto expected_json = R"(
        {
            "devices": { "self": "MP3100HV" },
            "settings": {
                "self": {
                    "dsp": {
                        "filter": { "type": "s", "value": "fir_long" },
                        "phase_invert": { "type": "b", "value": true }
                    },
                    "dsd_out_filter": {
                        "mode": { "type": "s", "value": "normal" }
                    }
                }
            }
        })"_json;
    expect_equal(expected_json);
}

TEST_CASE_FIXTURE(Fixture, "Set single value, purge remaining settings")
{
    const std::string input1 = R"(
        {
            "audio_path_changes": [
                { "op": "add_instance", "name": "self", "id": "MP3100HV" },
                {
                    "op": "set", "element": "self.dsp",
                    "kv": {
                        "filter": { "type": "s", "value": "iir_bezier" },
                        "phase_invert": { "type": "b", "value": true }
                    }
                },
                {
                    "op": "set", "element": "self.dsd_out_filter",
                    "kv": {
                        "mode": { "type": "s", "value": "normal" }
                    }
                }
            ]
        })";
    settings.update(input1);

    const std::string input2 = R"(
        {
            "audio_path_changes": [
                {
                    "op": "set", "element": "self.dsp",
                    "kv": {
                        "filter": { "type": "s", "value": "fir_long" }
                    }
                }
            ]
        })";
    settings.update(input2);

    const auto expected_json = R"(
        {
            "devices": { "self": "MP3100HV" },
            "settings": {
                "self": {
                    "dsp": {
                        "filter": { "type": "s", "value": "fir_long" }
                    },
                    "dsd_out_filter": {
                        "mode": { "type": "s", "value": "normal" }
                    }
                }
            }
        })"_json;
    expect_equal(expected_json);
}

TEST_CASE_FIXTURE(Fixture, "Unset one element value, make it an unknown value")
{
    const std::string input1 = R"(
        {
            "audio_path_changes": [
                { "op": "add_instance", "name": "self", "id": "MP3100HV" },
                {
                    "op": "set", "element": "self.dsp",
                    "kv": {
                        "filter": { "type": "s", "value": "iir_bezier" },
                        "phase_invert": { "type": "b", "value": true }
                    }
                }
            ]
        })";
    settings.update(input1);

    const std::string input2 = R"(
        {
            "audio_path_changes": [
                { "op": "unset", "element": "self.dsp", "v": "phase_invert" }
            ]
        })";
    settings.update(input2);

    const auto expected_json = R"(
        {
            "devices": { "self": "MP3100HV" },
            "settings": {
                "self": {
                    "dsp": {
                        "filter": { "type": "s", "value": "iir_bezier" }
                    }
                }
            }
        })"_json;
    expect_equal(expected_json);
}

TEST_CASE_FIXTURE(Fixture, "Unset values of all controls in an element")
{
    const std::string input1 = R"(
        {
            "audio_path_changes": [
                { "op": "add_instance", "name": "self", "id": "MP3100HV" },
                {
                    "op": "set", "element": "self.dsp",
                    "kv": {
                        "filter": { "type": "s", "value": "iir_bezier" },
                        "phase_invert": { "type": "b", "value": true }
                    }
                }
            ]
        })";
    settings.update(input1);

    {
    ConfigStore::SettingsJSON js(settings);
    ConfigStore::Changes changes;
    CHECK(js.extract_changes(changes));
    }

    const std::string input2 = R"(
        { "audio_path_changes": [ { "op": "unset_all", "element": "self.dsp" } ] })";
    settings.update(input2);

    const auto expected_json = R"( { "devices": { "self": "MP3100HV" } })"_json;
    expect_equal(expected_json);

    /* change log */
    ConfigStore::Changes changes;
    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    std::vector<std::string> removed_params;

    changes.for_each_changed_value(
        [&removed_params]
        (const auto &name, const auto &old_value, const auto &new_value)
        {
            CHECK(new_value == ConfigStore::Value());
            removed_params.push_back(name);
        });

    std::sort(removed_params.begin(), removed_params.end());

    static const std::array<const std::string, 2> expected_params
    {
        "self.dsp.filter", "self.dsp.phase_invert",
    };

    std::vector<std::string> diff;
    std::set_symmetric_difference(
        expected_params.begin(), expected_params.end(),
        removed_params.begin(), removed_params.end(),
        std::back_inserter(diff));

    CHECK(removed_params.size() == expected_params.size());
    CHECK(diff.empty());
}

TEST_CASE_FIXTURE(Fixture, "Connect audio output of one instance to input of another instance")
{
    const std::string input = R"(
        {
            "audio_path_changes": [
                { "op": "clear_instances" },
                { "op": "add_instance", "name": "self", "id": "MP3100HV" },
                { "op": "add_instance", "name": "a", "id": "PA3100HV" },
                { "op": "add_instance", "name": "b", "id": "PA2000R" },
                { "op": "add_instance", "name": "c", "id": "PA3100HV" },
                {
                    "op": "connect",
                    "from": "self.analog_line_out_1", "to": "a.analog_in_4"
                },
                {
                    "op": "connect",
                    "from": "self.analog_line_out_1", "to": "c.analog_in_1"
                },
                {
                    "op": "connect",
                    "from": "self.analog_line_out_2", "to": "b.analog_in_3"
                }
            ]
        })";
    settings.update(input);

    const auto expected_json = R"(
        {
            "devices": {
                "self": "MP3100HV",
                "a": "PA3100HV", "b": "PA2000R", "c": "PA3100HV"
            },
            "connections": {
                "self": {
                    "analog_line_out_1": [ "a.analog_in_4", "c.analog_in_1" ],
                    "analog_line_out_2": [ "b.analog_in_3" ]
                }
            }
        })"_json;
    expect_equal(expected_json);
}

/*
 *                             +-------------+
 *                      ,----->| i1          |
 *                     |       | i2          |       +-------------+
 *                     |       | i3  "a"  o1 |>----->| i1  "d"     |
 *                     | ,---->| i4          |       +-------------+
 *                     | | ,-->| i5          |
 *   +-------------+   | | |   +-------------+
 *   |          o1 |>----+ |
 *   |             |   | | |   +-------------+
 *   |          o2 |>--+------>| i3  "b"  o1 |>--.
 *   |     "s"     |   | | |   +-------------+   |   +-------------+
 *   |          o3 |>------+                     `-->| i1          |
 *   |             |   | | |                         |     "e"  o1 |
 *   |          o4 |>------'                     ,-->| i2          |
 *   +-------------+   | |     +-------------+   |   +-------------+
 *                     | `---->| i1          |   |
 *                     |       |     "c"  o1 |>--'
 *                     `------>| i2          |
 *                             +-------------+
 */
void Fixture::bunch_of_connected_instances(bool clear_change_log)
{
    const std::string input = R"(
        {
            "audio_path_changes": [
                { "op": "clear_instances" },
                { "op": "add_instance", "name": "s", "id": "MP3100HV" },
                { "op": "add_instance", "name": "a", "id": "A" },
                { "op": "add_instance", "name": "b", "id": "B" },
                { "op": "add_instance", "name": "c", "id": "C" },
                { "op": "add_instance", "name": "d", "id": "D" },
                { "op": "add_instance", "name": "e", "id": "E" },
                { "op": "add_instance", "name": "f", "id": "F" },
                { "op": "connect", "from": "s.o1", "to": "a.i4" },
                { "op": "connect", "from": "s.o1", "to": "c.i1" },
                { "op": "connect", "from": "s.o2", "to": "a.i1" },
                { "op": "connect", "from": "s.o2", "to": "b.i3" },
                { "op": "connect", "from": "s.o2", "to": "c.i2" },
                { "op": "connect", "from": "s.o3", "to": "a.i5" },
                { "op": "connect", "from": "s.o4", "to": "a.i5" },
                { "op": "connect", "from": "a.o1", "to": "d.i1" },
                { "op": "connect", "from": "b.o1", "to": "e.i1" },
                { "op": "connect", "from": "c.o1", "to": "e.i2" }
            ]
        })";
    settings.update(input);

    const auto expected_json = R"(
        {
            "devices": {
                "s": "MP3100HV",
                "a": "A", "b": "B", "c": "C", "d": "D", "e": "E", "f": "F"
            },
            "connections": {
                "s": {
                    "o1": [ "a.i4", "c.i1" ],
                    "o2": [ "a.i1", "b.i3", "c.i2" ],
                    "o3": [ "a.i5" ],
                    "o4": [ "a.i5" ]
                },
                "a": { "o1": [ "d.i1" ] },
                "b": { "o1": [ "e.i1" ] },
                "c": { "o1": [ "e.i2" ] }
            }
        })"_json;
    expect_equal(expected_json);

    if(clear_change_log)
    {
        ConfigStore::Changes changes;
        ConfigStore::SettingsJSON js(settings);
        CHECK(js.extract_changes(changes));
    }
}

TEST_CASE_FIXTURE(Fixture, "Changed devices are logged and can be processed")
{
    bunch_of_connected_instances();

    ConfigStore::Changes changes;
    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    std::vector<std::string> reported_devices;

    changes.for_each_changed_device(
        [&reported_devices] (const auto &name, bool was_added)
        {
            CHECK(was_added);
            reported_devices.push_back(name);
        });

    std::sort(reported_devices.begin(), reported_devices.end());

    static const std::array<const std::string, 7> expected_devices
    { "a", "b", "c", "d", "e", "f", "s" };

    std::vector<std::string> diff;
    std::set_symmetric_difference(expected_devices.begin(), expected_devices.end(),
                                  reported_devices.begin(), reported_devices.end(),
                                  std::back_inserter(diff));

    CHECK(reported_devices.size() == expected_devices.size());
    CHECK(diff.empty());
}

TEST_CASE_FIXTURE(Fixture, "Changed connections are logged and can be processed")
{
    bunch_of_connected_instances();

    ConfigStore::Changes changes;
    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    std::vector<std::pair<std::string, std::string>> reported_connections;

    changes.for_each_changed_connection(
        [&reported_connections] (const auto &from, const auto &to, bool was_added)
        {
            CHECK(was_added);
            reported_connections.push_back({from, to});
        });

    std::sort(reported_connections.begin(), reported_connections.end());

    static const std::array<const std::pair<std::string, std::string>, 10> expected_connections
    {{
        { "a.o1", "d.i1" }, { "b.o1", "e.i1" }, { "c.o1", "e.i2" },
        { "s.o1", "a.i4" }, { "s.o1", "c.i1" }, { "s.o2", "a.i1" },
        { "s.o2", "b.i3" }, { "s.o2", "c.i2" }, { "s.o3", "a.i5" },
        { "s.o4", "a.i5" },
    }};

    std::vector<std::pair<const std::string, const std::string>> diff;
    std::set_symmetric_difference(
        expected_connections.begin(), expected_connections.end(),
        reported_connections.begin(), reported_connections.end(),
        std::back_inserter(diff));

    CHECK(reported_connections.size() == expected_connections.size());
    CHECK(diff.empty());
}

TEST_CASE_FIXTURE(Fixture, "Changed values are logged and can be processed")
{
    bunch_of_connected_instances();

    const std::string some_values = R"(
        {
            "audio_path_changes": [
                {
                    "op": "set", "element": "s.dsp",
                    "kv": {
                        "filter": { "type": "s", "value": "iir_bezier" },
                        "phase_invert": { "type": "b", "value": true }
                    }
                },
                {
                    "op": "set", "element": "b.x",
                    "kv": {
                        "hello": { "type": "s", "value": "world" },
                        "foo": { "type": "s", "value": "bar" }
                    }
                },
                {
                    "op": "set", "element": "b.y",
                    "kv": {
                        "answer": { "type": "i", "value": 42 }
                    }
                },
                {
                    "op": "set", "element": "e.z",
                    "kv": {
                        "v": { "type": "D", "value": -0.75 }
                    }
                }
            ]
        })";
    settings.update(some_values);

    /* change log */
    ConfigStore::Changes changes;
    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    std::vector<std::pair<std::string, ConfigStore::Value>> reported_values;

    changes.for_each_changed_value(
        [&reported_values]
        (const auto &name, const auto &old_value, const auto &new_value)
        {
            CHECK(old_value == ConfigStore::Value());
            reported_values.push_back({name, new_value});
        });

    std::sort(reported_values.begin(), reported_values.end());

    static const std::array<const std::pair<std::string, ConfigStore::Value>, 6> expected_values
    {{
        { "b.x.foo",            ConfigStore::Value("s", nlohmann::json("bar")) },
        { "b.x.hello",          ConfigStore::Value("s", nlohmann::json("world")) },
        { "b.y.answer",         ConfigStore::Value("i", nlohmann::json(42)) },
        { "e.z.v",              ConfigStore::Value("D", nlohmann::json(-0.75)) },
        { "s.dsp.filter",       ConfigStore::Value("s", nlohmann::json("iir_bezier")) },
        { "s.dsp.phase_invert", ConfigStore::Value("b", nlohmann::json(true)) },
    }};

    std::vector<std::pair<const std::string, const ConfigStore::Value>> diff;
    std::set_symmetric_difference(
        expected_values.begin(), expected_values.end(),
        reported_values.begin(), reported_values.end(),
        std::back_inserter(diff),
        [] (const auto &a, const auto &b) { return a.first < b.first && a.second < b.second; });

    CHECK(reported_values.size() == expected_values.size());
    CHECK(diff.empty());
}

TEST_CASE_FIXTURE(Fixture, "Disconnect single one-to-one audio connection")
{
    bunch_of_connected_instances(true);

    const std::string input = R"(
        {
            "audio_path_changes": [
                { "op": "disconnect", "from": "b.o1", "to": "e.i1" }
            ]
        })";
    settings.update(input);

    const auto expected_json = R"(
        {
            "devices": {
                "s": "MP3100HV",
                "a": "A", "b": "B", "c": "C", "d": "D", "e": "E", "f": "F"
            },
            "connections": {
                "s": {
                    "o1": [ "a.i4", "c.i1" ],
                    "o2": [ "a.i1", "b.i3", "c.i2" ],
                    "o3": [ "a.i5" ],
                    "o4": [ "a.i5" ]
                },
                "a": { "o1": [ "d.i1" ] },
                "c": { "o1": [ "e.i2" ] }
            }
        })"_json;
    expect_equal(expected_json);

    /* change log */
    static const std::array<const std::pair<std::string, std::string>, 1> expected_connections
    {{ { "b.o1", "e.i1" } }};

    check_disconnected_connections(expected_connections);
}

TEST_CASE_FIXTURE(Fixture, "Disconnect single one-to-many audio connection")
{
    bunch_of_connected_instances();

    const std::string input = R"(
        {
            "audio_path_changes": [
                { "op": "disconnect", "from": "s.o4", "to": "a.i5" }
            ]
        })";
    settings.update(input);

    const auto expected_json = R"(
        {
            "devices": {
                "s": "MP3100HV",
                "a": "A", "b": "B", "c": "C", "d": "D", "e": "E", "f": "F"
            },
            "connections": {
                "s": {
                    "o1": [ "a.i4", "c.i1" ],
                    "o2": [ "a.i1", "b.i3", "c.i2" ],
                    "o3": [ "a.i5" ]
                },
                "a": { "o1": [ "d.i1" ] },
                "b": { "o1": [ "e.i1" ] },
                "c": { "o1": [ "e.i2" ] }
            }
        })"_json;
    expect_equal(expected_json);
}

TEST_CASE_FIXTURE(Fixture, "Disconnect complete one-to-many audio connection")
{
    bunch_of_connected_instances(true);

    const std::string input = R"(
        {
            "audio_path_changes": [
                { "op": "disconnect", "to": "a.i5" }
            ]
        })";
    settings.update(input);

    const auto expected_json = R"(
        {
            "devices": {
                "s": "MP3100HV",
                "a": "A", "b": "B", "c": "C", "d": "D", "e": "E", "f": "F"
            },
            "connections": {
                "s": {
                    "o1": [ "a.i4", "c.i1" ],
                    "o2": [ "a.i1", "b.i3", "c.i2" ]
                },
                "a": { "o1": [ "d.i1" ] },
                "b": { "o1": [ "e.i1" ] },
                "c": { "o1": [ "e.i2" ] }
            }
        })"_json;
    expect_equal(expected_json);

    /* change log */
    static const std::array<const std::pair<std::string, std::string>, 2> expected_connections
    {{
        { "s.o3", "a.i5" }, { "s.o4", "a.i5" },
    }};

    check_disconnected_connections(expected_connections);
}

TEST_CASE_FIXTURE(Fixture, "Disconnect single many-to-one audio connection")
{
    bunch_of_connected_instances();

    const std::string input = R"(
        {
            "audio_path_changes": [
                { "op": "disconnect", "from": "s.o2", "to": "b.i3" }
            ]
        })";
    settings.update(input);

    const auto expected_json = R"(
        {
            "devices": {
                "s": "MP3100HV",
                "a": "A", "b": "B", "c": "C", "d": "D", "e": "E", "f": "F"
            },
            "connections": {
                "s": {
                    "o1": [ "a.i4", "c.i1" ],
                    "o2": [ "a.i1", "c.i2" ],
                    "o3": [ "a.i5" ],
                    "o4": [ "a.i5" ]
                },
                "a": { "o1": [ "d.i1" ] },
                "b": { "o1": [ "e.i1" ] },
                "c": { "o1": [ "e.i2" ] }
            }
        })"_json;
    expect_equal(expected_json);
}

TEST_CASE_FIXTURE(Fixture, "Disconnect complete many-to-one audio connection")
{
    bunch_of_connected_instances(true);

    const std::string input = R"(
        {
            "audio_path_changes": [
                { "op": "disconnect", "from": "s.o2" }
            ]
        })";
    settings.update(input);

    const auto expected_json = R"(
        {
            "devices": {
                "s": "MP3100HV",
                "a": "A", "b": "B", "c": "C", "d": "D", "e": "E", "f": "F"
            },
            "connections": {
                "s": {
                    "o1": [ "a.i4", "c.i1" ],
                    "o3": [ "a.i5" ],
                    "o4": [ "a.i5" ]
                },
                "a": { "o1": [ "d.i1" ] },
                "b": { "o1": [ "e.i1" ] },
                "c": { "o1": [ "e.i2" ] }
            }
        })"_json;
    expect_equal(expected_json);

    /* change log */
    static const std::array<const std::pair<std::string, std::string>, 3> expected_connections
    {{
        { "s.o2", "a.i1" }, { "s.o2", "b.i3" }, { "s.o2", "c.i2" },
    }};

    check_disconnected_connections(expected_connections);
}

TEST_CASE_FIXTURE(Fixture, "Removing device in the middle also removes its connections")
{
    bunch_of_connected_instances(true);

    const std::string input = R"(
        { "audio_path_changes": [ { "op": "rm_instance", "name": "a" } ] })";
    settings.update(input);

    const auto expected_json = R"(
        {
            "devices": {
                "s": "MP3100HV",
                "b": "B", "c": "C", "d": "D", "e": "E", "f": "F"
            },
            "connections": {
                "s": {
                    "o1": [ "c.i1" ],
                    "o2": [ "b.i3", "c.i2" ]
                },
                "b": { "o1": [ "e.i1" ] },
                "c": { "o1": [ "e.i2" ] }
            }
        })"_json;
    expect_equal(expected_json);

    /* change log */
    ConfigStore::Changes changes;
    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    std::vector<std::string> reported_devices;
    std::vector<std::pair<std::string, std::string>> reported_connections;

    changes.for_each_changed_device(
        [&reported_devices] (const auto &name, bool was_added)
        {
            CHECK_FALSE(was_added);
            reported_devices.push_back(name);
        });

    changes.for_each_changed_connection(
        [&reported_connections]
        (const auto &from, const auto &to, bool was_added)
        {
            CHECK_FALSE(was_added);
            reported_connections.push_back({from, to});
        });

    std::sort(reported_devices.begin(), reported_devices.end());
    std::sort(reported_connections.begin(), reported_connections.end());

    static const std::array<const std::string, 1> expected_devices { "a" };

    std::vector<std::string> diff_devices;
    std::set_symmetric_difference(expected_devices.begin(), expected_devices.end(),
                                  reported_devices.begin(), reported_devices.end(),
                                  std::back_inserter(diff_devices));

    CHECK(reported_devices.size() == expected_devices.size());
    CHECK(diff_devices.empty());

    static const std::array<const std::pair<std::string, std::string>, 5> expected_connections
    {{
        { "a.o1", "d.i1" },
        { "s.o1", "a.i4" },
        { "s.o2", "a.i1" },
        { "s.o3", "a.i5" },
        { "s.o4", "a.i5" },
    }};

    std::vector<std::pair<const std::string, const std::string>> diff_connections;
    std::set_symmetric_difference(
        expected_connections.begin(), expected_connections.end(),
        reported_connections.begin(), reported_connections.end(),
        std::back_inserter(diff_connections));

    CHECK(reported_connections.size() == expected_connections.size());
    CHECK(diff_connections.empty());
}

TEST_CASE_FIXTURE(Fixture, "Removing root device also removes its connections")
{
    bunch_of_connected_instances();

    const std::string input = R"(
        { "audio_path_changes": [ { "op": "rm_instance", "name": "s" } ] })";
    settings.update(input);

    const auto expected_json = R"(
        {
            "devices": {
                "a": "A", "b": "B", "c": "C", "d": "D", "e": "E", "f": "F"
            },
            "connections": {
                "a": { "o1": [ "d.i1" ] },
                "b": { "o1": [ "e.i1" ] },
                "c": { "o1": [ "e.i2" ] }
            }
        })"_json;
    expect_equal(expected_json);
}

TEST_CASE_FIXTURE(Fixture, "Removing sink device also removes its connections")
{
    bunch_of_connected_instances();

    const std::string input = R"(
        { "audio_path_changes": [ { "op": "rm_instance", "name": "e" } ] })";
    settings.update(input);

    const auto expected_json = R"(
        {
            "devices": {
                "s": "MP3100HV",
                "a": "A", "b": "B", "c": "C", "d": "D", "f": "F"
            },
            "connections": {
                "s": {
                    "o1": [ "a.i4", "c.i1" ],
                    "o2": [ "a.i1", "b.i3", "c.i2" ],
                    "o3": [ "a.i5" ],
                    "o4": [ "a.i5" ]
                },
                "a": { "o1": [ "d.i1" ] }
            }
        })"_json;
    expect_equal(expected_json);
}

TEST_CASE_FIXTURE(Fixture, "Disconnect all outgoing audio connections")
{
    bunch_of_connected_instances(true);

    const std::string input = R"(
        { "audio_path_changes": [ { "op": "disconnect", "from": "s" } ] })";
    settings.update(input);

    const auto expected_json = R"(
        {
            "devices": {
                "s": "MP3100HV",
                "a": "A", "b": "B", "c": "C", "d": "D", "e": "E", "f": "F"
            },
            "connections": {
                "a": { "o1": [ "d.i1" ] },
                "b": { "o1": [ "e.i1" ] },
                "c": { "o1": [ "e.i2" ] }
            }
        })"_json;
    expect_equal(expected_json);

    /* change log */
    static const std::array<const std::pair<std::string, std::string>, 7> expected_connections
    {{
        { "s.o1", "a.i4" }, { "s.o1", "c.i1" }, { "s.o2", "a.i1" },
        { "s.o2", "b.i3" }, { "s.o2", "c.i2" }, { "s.o3", "a.i5" },
        { "s.o4", "a.i5" },
    }};

    check_disconnected_connections(expected_connections);
}

TEST_CASE_FIXTURE(Fixture, "Disconnect all ingoing audio connections (single source)")
{
    bunch_of_connected_instances(true);

    const std::string input = R"(
        { "audio_path_changes": [ { "op": "disconnect", "to": "a" } ] })";
    settings.update(input);

    const auto expected_json = R"(
        {
            "devices": {
                "s": "MP3100HV",
                "a": "A", "b": "B", "c": "C", "d": "D", "e": "E", "f": "F"
            },
            "connections": {
                "s": {
                    "o1": [ "c.i1" ],
                    "o2": [ "b.i3", "c.i2" ]
                },
                "a": { "o1": [ "d.i1" ] },
                "b": { "o1": [ "e.i1" ] },
                "c": { "o1": [ "e.i2" ] }
            }
        })"_json;
    expect_equal(expected_json);

    /* change log */
    static const std::array<const std::pair<std::string, std::string>, 4> expected_connections
    {{
        { "s.o1", "a.i4" }, { "s.o2", "a.i1" }, { "s.o3", "a.i5" },
        { "s.o4", "a.i5" },
    }};

    check_disconnected_connections(expected_connections);
}

TEST_CASE_FIXTURE(Fixture, "Disconnect all ingoing audio connections (multiple sources)")
{
    bunch_of_connected_instances(true);

    const std::string input = R"(
        { "audio_path_changes": [ { "op": "disconnect", "to": "e" } ] })";
    settings.update(input);

    const auto expected_json = R"(
        {
            "devices": {
                "s": "MP3100HV",
                "a": "A", "b": "B", "c": "C", "d": "D", "e": "E", "f": "F"
            },
            "connections": {
                "s": {
                    "o1": [ "a.i4", "c.i1" ],
                    "o2": [ "a.i1", "b.i3", "c.i2" ],
                    "o3": [ "a.i5" ],
                    "o4": [ "a.i5" ]
                },
                "a": { "o1": [ "d.i1" ] }
            }
        })"_json;
    expect_equal(expected_json);

    /* change log */
    static const std::array<const std::pair<std::string, std::string>, 2> expected_connections
    {{
        { "b.o1", "e.i1" }, { "c.o1", "e.i2" },
    }};

    check_disconnected_connections(expected_connections);
}

TEST_CASE_FIXTURE(Fixture, "Disconnect audio connections from one sink to specific instance")
{
    bunch_of_connected_instances(true);

    const std::string input = R"(
        { "audio_path_changes": [ { "op": "disconnect", "from": "s.o2", "to": "b" } ] })";
    settings.update(input);

    const auto expected_json = R"(
        {
            "devices": {
                "s": "MP3100HV",
                "a": "A", "b": "B", "c": "C", "d": "D", "e": "E", "f": "F"
            },
            "connections": {
                "s": {
                    "o1": [ "a.i4", "c.i1" ],
                    "o2": [ "a.i1", "c.i2" ],
                    "o3": [ "a.i5" ],
                    "o4": [ "a.i5" ]
                },
                "a": { "o1": [ "d.i1" ] },
                "b": { "o1": [ "e.i1" ] },
                "c": { "o1": [ "e.i2" ] }
            }
        })"_json;
    expect_equal(expected_json);

    /* change log */
    static const std::array<const std::pair<std::string, std::string>, 1> expected_connections
    {{ { "s.o2", "b.i3" }, }};

    check_disconnected_connections(expected_connections);
}

TEST_CASE_FIXTURE(Fixture, "Disconnect audio connections from all sinks to specific source")
{
    bunch_of_connected_instances(true);

    const std::string input = R"(
        { "audio_path_changes": [ { "op": "disconnect", "from": "s", "to": "a.i5" } ] })";
    settings.update(input);

    const auto expected_json = R"(
        {
            "devices": {
                "s": "MP3100HV",
                "a": "A", "b": "B", "c": "C", "d": "D", "e": "E", "f": "F"
            },
            "connections": {
                "s": {
                    "o1": [ "a.i4", "c.i1" ],
                    "o2": [ "a.i1", "b.i3", "c.i2" ]
                },
                "a": { "o1": [ "d.i1" ] },
                "b": { "o1": [ "e.i1" ] },
                "c": { "o1": [ "e.i2" ] }
            }
        })"_json;
    expect_equal(expected_json);

    /* change log */
    static const std::array<const std::pair<std::string, std::string>, 2> expected_connections
    {{
        { "s.o3", "a.i5" }, { "s.o4", "a.i5" },
    }};

    check_disconnected_connections(expected_connections);
}

TEST_CASE_FIXTURE(Fixture, "Disconnect all audio connections between two instances")
{
    bunch_of_connected_instances(true);

    const std::string input = R"(
        { "audio_path_changes": [ { "op": "disconnect", "from": "b", "to": "e" } ] })";
    settings.update(input);

    const auto expected_json = R"(
        {
            "devices": {
                "s": "MP3100HV",
                "a": "A", "b": "B", "c": "C", "d": "D", "e": "E", "f": "F"
            },
            "connections": {
                "s": {
                    "o1": [ "a.i4", "c.i1" ],
                    "o2": [ "a.i1", "b.i3", "c.i2" ],
                    "o3": [ "a.i5" ],
                    "o4": [ "a.i5" ]
                },
                "a": { "o1": [ "d.i1" ] },
                "c": { "o1": [ "e.i2" ] }
            }
        })"_json;
    expect_equal(expected_json);

    /* change log */
    static const std::array<const std::pair<std::string, std::string>, 1> expected_connections
    {{ { "b.o1", "e.i1" }, }};

    check_disconnected_connections(expected_connections);
}

TEST_CASE_FIXTURE(Fixture, "Disconnect all audio connections")
{
    bunch_of_connected_instances(true);

    const std::string input = R"(
        { "audio_path_changes": [ { "op": "disconnect" } ] })";
    settings.update(input);

    const auto expected_json = R"(
        {
            "devices": {
                "s": "MP3100HV",
                "a": "A", "b": "B", "c": "C", "d": "D", "e": "E", "f": "F"
            }
        })"_json;
    expect_equal(expected_json);

    /* change log */
    static const std::array<const std::pair<std::string, std::string>, 10> expected_connections
    {{
        { "a.o1", "d.i1" }, { "b.o1", "e.i1" }, { "c.o1", "e.i2" },
        { "s.o1", "a.i4" }, { "s.o1", "c.i1" }, { "s.o2", "a.i1" },
        { "s.o2", "b.i3" }, { "s.o2", "c.i2" }, { "s.o3", "a.i5" },
        { "s.o4", "a.i5" },
    }};

    check_disconnected_connections(expected_connections);
}

TEST_CASE_FIXTURE(Fixture, "NOP reports are filtered out")
{
    bunch_of_connected_instances(true);

    const std::string input1 = R"(
        {
            "audio_path_changes": [
                { "op": "add_instance", "name": "mp", "id": "MP3100HV" },
                { "op": "connect", "from": "mp.o1", "to": "e.i2" },
                {
                    "op": "set", "element": "mp.dsp",
                    "kv": {
                        "filter": { "type": "s", "value": "iir_bezier" },
                        "phase_invert": { "type": "b", "value": true }
                    }
                }
            ]
        })";
    settings.update(input1);

    const std::string input2 = R"(
        { "audio_path_changes": [ { "op": "rm_instance", "name": "mp" } ] })";
    settings.update(input2);

    /* change log */
    ConfigStore::Changes changes;
    {
    ConfigStore::SettingsJSON js(settings);
    CHECK_FALSE(js.extract_changes(changes));
    }
}

TEST_SUITE_END();
