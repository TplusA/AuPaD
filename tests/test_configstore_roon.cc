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

#include <doctest.h>

#include "report_roon.hh"

#include "client_plugin_manager.hh"
#include "configstore.hh"
#include "configstore_json.hh"
#include "configstore_changes.hh"
#include "device_models.hh"

#include "mock_messages.hh"

class RoonUpdate
{
  private:
    nlohmann::json expected_update_;
    nlohmann::json sent_update_;
    bool expecting_update_;
    bool update_was_sent_;

  public:
    RoonUpdate(const RoonUpdate &) = delete;
    RoonUpdate(RoonUpdate &&) = default;
    RoonUpdate &operator=(const RoonUpdate &) = delete;
    RoonUpdate &operator=(RoonUpdate &&) = default;

    explicit RoonUpdate():
        expecting_update_(false),
        update_was_sent_(false)
    {}

    void expect(const char *expected)
    {
        expect(nlohmann::json::parse(expected));
    }

    void expect(nlohmann::json &&expected)
    {
        expected_update_ = std::move(expected);
        expecting_update_ = true;

        REQUIRE_FALSE(update_was_sent_);
        sent_update_ = nullptr;
        update_was_sent_ = false;
    }

    void send(const std::string &asp, const std::vector<std::string> &extra)
    {
        CHECK(expecting_update_);
        CHECK_FALSE(update_was_sent_);
        CHECK_FALSE(asp.empty());
        CHECK(extra.empty());

        if(update_was_sent_)
        {
            MESSAGE("Multiple updates sent, keeping first");
            return;
        }

        sent_update_ = nlohmann::json::parse(asp);
        update_was_sent_ = true;

        const auto d(nlohmann::json::diff(sent_update_, expected_update_));
        if(d.empty())
            return;

        MESSAGE("Diff: " << d);
        CHECK(sent_update_ == expected_update_);
    }

    void check()
    {
        CHECK(expecting_update_ == update_was_sent_);
        expected_update_ = nullptr;
        expecting_update_ = false;
        sent_update_ = nullptr;
        update_was_sent_ = false;
    }
};

TEST_SUITE_BEGIN("Roon output plugin, single appliance");

class Fixture
{
  protected:
    ClientPlugin::PluginManager pm;
    StaticModels::DeviceModelsDatabase models;
    ConfigStore::Settings settings;
    std::unique_ptr<MockMessages::Mock> mock_messages;

    ClientPlugin::Roon *roon_plugin;
    RoonUpdate roon_update;

  public:
    explicit Fixture():
        settings(models),
        mock_messages(std::make_unique<MockMessages::Mock>()),
        roon_plugin(nullptr)
    {
        MockMessages::singleton = mock_messages.get();

        expect<MockMessages::MsgInfo>(mock_messages,
                                      "Registered plugin \"Roon\"", false);

        auto roon(std::make_unique<ClientPlugin::Roon>(
            [this] (const auto &asp, const auto &extra) { roon_update.send(asp, extra); }));
        roon->add_client();
        roon_plugin = roon.get();
        pm.register_plugin(std::move(roon));

        if(!models.load("test_models.json", true))
            models.load("tests/test_models.json");
    }

    ~Fixture()
    {
        expect<MockMessages::MsgInfo>(mock_messages,
                                      "Unregistered plugin \"Roon\"", false);
        pm.shutdown();

        try
        {
            roon_update.check();
            mock_messages->done();
        }
        catch(...)
        {
            /* no throwing from dtors */
        }

        MockMessages::singleton = nullptr;
    }

  protected:
    void expect_equal(const nlohmann::json &expected, const nlohmann::json &have)
    {
        const auto d(nlohmann::json::diff(have, expected));

        if(d.empty())
            return;

        MESSAGE("Diff: " << d);
        CHECK(have == expected);
    }
};

TEST_CASE_FIXTURE(Fixture, "Passing empty changes has no side effects")
{
    ConfigStore::Changes changes;
    ConfigStore::SettingsJSON js(settings);
    CHECK_FALSE(js.extract_changes(changes));
    pm.report_changes(settings, changes);
}

TEST_CASE_FIXTURE(Fixture, "Settings update for CALA CDR")
{
    const auto input = R"(
        {
            "audio_path_changes": [
                {
                    "op": "add_instance", "name": "self", "id": "CalaCDR"
                },
                {
                    "op": "set", "element": "self.dsp",
                    "kv": {
                        "volume":                  { "type": "y", "value": 60 },
                        "balance":                 { "type": "Y", "value": 10 },
                        "loudness_enable":         { "type": "b", "value": false },
                        "tone_control_enable":     { "type": "b", "value": true },
                        "treble":                  { "type": "Y", "value": 0 },
                        "mid":                     { "type": "D", "value": 0.5 },
                        "bass":                    { "type": "Y", "value": 1 },
                        "subwoofer_volume_offset": { "type": "Y", "value": -3 },
                        "contour_presence":        { "type": "Y", "value": 2 },
                        "contour_ft":              { "type": "Y", "value": -1 },
                        "virtual_surround":        { "type": "b", "value": true },
                        "speaker_lf_shape":        { "type": "s", "value": "full_range" },
                        "speaker_stand":           { "type": "s", "value": "corner" },
                        "room_correction_level":   { "type": "s", "value": "r2" },
                        "subwoofer_freq":          { "type": "s", "value": "60hz" },
                        "room_headphone_enable":   { "type": "b", "value": false },
                        "analog_1_pass_through":   { "type": "b", "value": false },
                        "analog_2_phono_mode":     { "type": "b", "value": false },
                        "analog_1_in_level":       { "type": "Y", "value": 1 },
                        "analog_2_in_level":       { "type": "Y", "value": 2 },
                        "analog_2_phono_in_level": { "type": "Y", "value": 0 }
                    }
                },
                {
                    "op": "set", "element": "self.input_select",
                    "kv": { "sel": { "type": "s", "value": "strbo" } }
                },
                {
                    "op": "set", "element": "self.analog_or_digital",
                    "kv": { "is_digital": { "type": "b", "value": true } }
                },
                {
                    "op": "set", "element": "self.amp",
                    "kv": { "enable": { "type": "b", "value": true } }
                }
            ]
        })";
    settings.update(input);

    ConfigStore::Changes changes;
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));

    const auto expected_update = R"(
        [
            { "type": "digital_volume",                         "quality": "high",     "gain": 60.0 },
            { "type": "balance",                                "quality": "lossless", "value": 0.11764705882352944 },
            { "type": "eq",  "sub_type": "bass",                "quality": "enhanced", "gain": 1.0 },
            { "type": "eq",  "sub_type": "mid",                 "quality": "enhanced", "gain": 0.5 },
            { "type": "t+a", "sub_type": "contour_presence",    "quality": "enhanced", "gain": 2.0 },
            { "type": "t+a", "sub_type": "contour_fundamental", "quality": "enhanced", "gain": -1.0 },
            { "type": "t+a", "sub_type": "virtual_surround",    "quality": "enhanced" },
            { "type": "output", "method": "speakers",           "quality": "lossless" }
        ]
    )";

    roon_update.expect(expected_update);
    pm.report_changes(settings, changes);
}

TEST_CASE_FIXTURE(Fixture, "Tone control override in CALA CDR")
{
    const auto init_with_tone_control_enabled = R"(
        {
            "audio_path_changes": [
                {
                    "op": "add_instance", "name": "self", "id": "CalaCDR"
                },
                {
                    "op": "set", "element": "self.dsp",
                    "kv": {
                        "volume":                  { "type": "y", "value": 60 },
                        "balance":                 { "type": "Y", "value": 10 },
                        "loudness_enable":         { "type": "b", "value": true },
                        "tone_control_enable":     { "type": "b", "value": true },
                        "treble":                  { "type": "Y", "value": 1 },
                        "mid":                     { "type": "D", "value": 0.5 },
                        "bass":                    { "type": "Y", "value": 1 },
                        "contour_presence":        { "type": "Y", "value": 2 },
                        "contour_ft":              { "type": "Y", "value": -1 },
                        "virtual_surround":        { "type": "b", "value": true }
                    }
                },
                {
                    "op": "set", "element": "self.input_select",
                    "kv": { "sel": { "type": "s", "value": "strbo" } }
                },
                {
                    "op": "set", "element": "self.analog_or_digital",
                    "kv": { "is_digital": { "type": "b", "value": true } }
                },
                {
                    "op": "set", "element": "self.amp",
                    "kv": { "enable": { "type": "b", "value": true } }
                }
            ]
        })";
    settings.update(init_with_tone_control_enabled);

    ConfigStore::Changes changes;
    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    const auto expected_path_after_init = R"(
        [
            { "type": "digital_volume",                         "quality": "high",     "gain": 60.0 },
            { "type": "balance",                                "quality": "lossless", "value": 0.11764705882352944 },
            { "type": "t+a", "sub_type": "loudness",            "quality": "enhanced" },
            { "type": "eq",  "sub_type": "bass",                "quality": "enhanced", "gain": 1.0 },
            { "type": "eq",  "sub_type": "mid",                 "quality": "enhanced", "gain": 0.5 },
            { "type": "eq",  "sub_type": "treble",              "quality": "enhanced", "gain": 1.0 },
            { "type": "t+a", "sub_type": "contour_presence",    "quality": "enhanced", "gain": 2.0 },
            { "type": "t+a", "sub_type": "contour_fundamental", "quality": "enhanced", "gain": -1.0 },
            { "type": "t+a", "sub_type": "virtual_surround",    "quality": "enhanced" },
            { "type": "output", "method": "speakers",           "quality": "lossless" }
        ]
    )";

    roon_update.expect(expected_path_after_init);
    pm.report_changes(settings, changes);
    changes.reset();
    roon_update.check();

    /* switching off tone control suppresses equalizer settings */
    const auto disable_tone_control = R"(
        {
            "audio_path_changes": [{
                "op": "update", "element": "self.dsp",
                "kv": { "tone_control_enable": { "type": "b", "value": false } }
            }]
        })";
    settings.update(disable_tone_control);

    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    const auto expected_path_after_tone_control_disable = R"(
        [
            { "type": "digital_volume",                         "quality": "high",     "gain": 60.0 },
            { "type": "balance",                                "quality": "lossless", "value": 0.11764705882352944 },
            { "type": "t+a", "sub_type": "loudness",            "quality": "enhanced" },
            { "type": "t+a", "sub_type": "contour_presence",    "quality": "enhanced", "gain": 2.0 },
            { "type": "t+a", "sub_type": "contour_fundamental", "quality": "enhanced", "gain": -1.0 },
            { "type": "t+a", "sub_type": "virtual_surround",    "quality": "enhanced" },
            { "type": "output", "method": "speakers",           "quality": "lossless" }
        ]
    )";

    roon_update.expect(expected_path_after_tone_control_disable);
    pm.report_changes(settings, changes);
    changes.reset();
    roon_update.check();

    /* switching tone control back on emits path including equalizer settings
     * with previously set values */
    const auto enable_tone_control = R"(
        {
            "audio_path_changes": [{
                "op": "update", "element": "self.dsp",
                "kv": { "tone_control_enable": { "type": "b", "value": true } }
            }]
        })";
    settings.update(enable_tone_control);

    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    roon_update.expect(expected_path_after_init);
    pm.report_changes(settings, changes);
}

TEST_CASE_FIXTURE(Fixture, "Set values of fake MP200 compound")
{
    /* initialization: no update for Roon expected */
    const auto init_device = R"(
        {
            "audio_path_changes": [
                { "op": "clear_instances" },
                { "op": "add_instance", "name": "self", "id": "MP200" }
            ]
        })";
    settings.update(init_device);

    ConfigStore::Changes changes;
    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    pm.report_changes(settings, changes);
    roon_update.check();

    /* first audio path update: update for Roon expected */
    const auto first_selection = R"(
        {
            "audio_path_changes": [
                {
                    "op": "update", "element": "self.input_select",
                    "kv": { "sel": { "type": "s", "value": "strbo" }}
                },
                {
                    "op": "update", "element": "self.hp1_out_enable",
                    "kv": { "enable": { "type": "b", "value": true }}
                },
                {
                    "op": "update", "element": "self.amp",
                    "kv": { "enable": { "type": "b", "value": true }}
                },
                {
                    "op": "update", "element": "self.bw_filter",
                    "kv": { "mode": { "type": "s", "value": "wide" }}
                },
                {
                    "op": "update", "element": "self.syslink",
                    "kv": { "mode": { "type": "s", "value": "HA 200" }}
                }
            ]
        })";
    settings.update(first_selection);

    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    const auto expected_path_after_init = R"(
        [
            { "type": "t+a", "sub_type": "sys_link", "mode": "HA 200", "quality": "lossless" },
            { "type": "output", "method": "headphones", "quality": "lossless" }
        ]
    )";

    roon_update.expect(expected_path_after_init);
    pm.report_changes(settings, changes);
    changes.reset();
    roon_update.check();

    /* second audio path update with tone control: update for Roon expected */
    const auto tone_control = R"(
        {
            "audio_path_changes": [
                {
                    "op": "update", "element": "self.syslink",
                    "kv": { "mode": { "type": "s", "value": "DAC 200"} }
                },
                {
                    "op": "update", "element": "self.tone_ctrl",
                    "kv": {
                        "bass": { "type": "Y", "value": 1 },
                        "treble": { "type": "Y", "value": 2 },
                        "loudness_enable": { "type": "b", "value": true },
                        "tone_ctrl_enable": { "type": "b", "value": true }
                    }
                },
                {
                    "op": "update", "element": "self.crossfeed",
                    "kv": { "enable": { "type": "b", "value": true }}
                }
            ]
        })";
    settings.update(tone_control);

    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    const auto expected_path_after_tone_control = R"(
        [
            { "type": "t+a", "sub_type": "sys_link", "mode": "DAC 200", "quality": "lossless" },
            { "type": "eq", "sub_type": "treble", "gain": 2.0, "quality": "enhanced" },
            { "type": "eq", "sub_type": "bass", "gain": 1.0, "quality": "enhanced" },
            { "type": "t+a", "sub_type": "loudness", "quality": "enhanced" },
            { "type": "t+a", "sub_type": "crossfeed", "quality": "enhanced" },
            { "type": "output", "method": "headphones", "quality": "lossless" }
        ]
    )";

    roon_update.expect(expected_path_after_tone_control);
    pm.report_changes(settings, changes);
    changes.reset();
    roon_update.check();

    /* third audio path update, volume control and crossfeed: balance update
     * and crossfeed update for Roon expected, analog volume update not
     * expected */
    const auto volume_control = R"(
        {
            "audio_path_changes": [
                {
                    "op": "update", "element": "self.volume_ctrl",
                    "kv": {
                        "volume": { "type": "Y", "value": 65 },
                        "balance": { "type": "Y", "value": 15 }
                    }
                },
                {
                    "op": "update", "element": "self.crossfeed",
                    "kv": { "enable": { "type": "b", "value": false }}
                }
            ]
        })";
    settings.update(volume_control);

    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    const auto expected_path_after_volume_control = R"(
        [
            { "type": "t+a", "sub_type": "sys_link", "mode": "DAC 200", "quality": "lossless" },
            { "type": "balance", "value": "R15", "quality": "lossless" },
            { "type": "eq", "sub_type": "treble", "gain": 2.0, "quality": "enhanced" },
            { "type": "eq", "sub_type": "bass", "gain": 1.0, "quality": "enhanced" },
            { "type": "t+a", "sub_type": "loudness", "quality": "enhanced" },
            { "type": "output", "method": "headphones", "quality": "lossless" }
        ]
    )";

    roon_update.expect(expected_path_after_volume_control);
    pm.report_changes(settings, changes);
    changes.reset();
    roon_update.check();

    /* forth audio path update: balance update to the left expected */
    const auto balance_control = R"(
        {
            "audio_path_changes": [
                {
                    "op": "update", "element": "self.volume_ctrl",
                    "kv": { "balance": { "type": "Y", "value": -27 }}
                }
            ]
        })";
    settings.update(balance_control);

    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    const auto expected_path_after_balance_control = R"(
        [
            { "type": "t+a", "sub_type": "sys_link", "mode": "DAC 200", "quality": "lossless" },
            { "type": "balance", "value": "L27", "quality": "lossless" },
            { "type": "eq", "sub_type": "treble", "gain": 2.0, "quality": "enhanced" },
            { "type": "eq", "sub_type": "bass", "gain": 1.0, "quality": "enhanced" },
            { "type": "t+a", "sub_type": "loudness", "quality": "enhanced" },
            { "type": "output", "method": "headphones", "quality": "lossless" }
        ]
    )";

    roon_update.expect(expected_path_after_balance_control);
    pm.report_changes(settings, changes);
}

TEST_CASE_FIXTURE(Fixture,
                  "Roon report is sent if a switching option makes otherwise "
                  "unchanged values visible or invisible")
{
    /* initialization: tone control disabled, update for Roon expected */
    const auto init_device = R"(
        {
            "audio_path_changes": [
                { "op": "clear_instances" },
                { "op": "add_instance", "name": "self", "id": "MP200" },
                {
                    "op": "update", "element": "self.input_select",
                    "kv": { "sel": { "type": "s", "value": "strbo" }}
                },
                {
                    "op": "update", "element": "self.hp1_out_enable",
                    "kv": { "enable": { "type": "b", "value": true }}
                },
                {
                    "op": "update", "element": "self.amp",
                    "kv": { "enable": { "type": "b", "value": true }}
                },
                {
                    "op": "update", "element": "self.bw_filter",
                    "kv": { "mode": { "type": "s", "value": "wide" }}
                },
                {
                    "op": "update", "element": "self.tone_ctrl",
                    "kv": {
                        "bass": { "type": "Y", "value": 1 },
                        "treble": { "type": "Y", "value": 2 },
                        "loudness_enable": { "type": "b", "value": true }
                    }
                }
            ]
        })";
    settings.update(init_device);

    ConfigStore::Changes changes;
    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    const auto expected_path_after_init = R"(
        [
            { "type": "t+a", "sub_type": "loudness", "quality": "enhanced" },
            { "type": "output", "method": "headphones", "quality": "lossless" }
        ]
    )";

    roon_update.expect(expected_path_after_init);
    pm.report_changes(settings, changes);
    changes.reset();
    roon_update.check();

    /* enable tone control: update with tone controls for Roon expected */
    const auto tone_control_enable = R"(
        {
            "audio_path_changes": [
                {
                    "op": "update", "element": "self.tone_ctrl",
                    "kv": { "tone_ctrl_enable": { "type": "b", "value": true } }
                }
            ]
        })";
    settings.update(tone_control_enable);

    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    const auto expected_path_after_tone_control_enable = R"(
        [
            { "type": "eq", "sub_type": "treble", "gain": 2.0, "quality": "enhanced" },
            { "type": "eq", "sub_type": "bass", "gain": 1.0, "quality": "enhanced" },
            { "type": "t+a", "sub_type": "loudness", "quality": "enhanced" },
            { "type": "output", "method": "headphones", "quality": "lossless" }
        ]
    )";

    roon_update.expect(expected_path_after_tone_control_enable);
    pm.report_changes(settings, changes);
    changes.reset();
    roon_update.check();

    /* disable tone control: update without bass and treble values for Roon
     * expected */
    const auto tone_control_disable = R"(
        {
            "audio_path_changes": [
                {
                    "op": "update", "element": "self.tone_ctrl",
                    "kv": { "tone_ctrl_enable": { "type": "b", "value": false } }
                }
            ]
        })";
    settings.update(tone_control_disable);

    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    roon_update.expect(expected_path_after_init);
    pm.report_changes(settings, changes);
    changes.reset();
    roon_update.check();

    /* changing treble and bass does not trigger another Roon report */
    const auto tone_values_change = R"(
        {
            "audio_path_changes": [
                {
                    "op": "update", "element": "self.tone_ctrl",
                    "kv": {
                        "bass": { "type": "Y", "value": 4 },
                        "treble": { "type": "Y", "value": 5 }
                    }
                }
            ]
        })";
    settings.update(tone_values_change);

    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    pm.report_changes(settings, changes);
}

class CustomModels
{
  protected:
    ClientPlugin::PluginManager pm;
    StaticModels::DeviceModelsDatabase models;
    ConfigStore::Settings settings;
    std::unique_ptr<MockMessages::Mock> mock_messages;

    ClientPlugin::Roon *roon_plugin;
    RoonUpdate roon_update;

    explicit CustomModels():
        settings(models),
        mock_messages(std::make_unique<MockMessages::Mock>()),
        roon_plugin(nullptr)
    {
        MockMessages::singleton = mock_messages.get();

        expect<MockMessages::MsgInfo>(mock_messages,
                                      "Registered plugin \"Roon\"", false);

        auto roon(std::make_unique<ClientPlugin::Roon>(
            [this] (const auto &asp, const auto &extra) { roon_update.send(asp, extra); }));
        roon->add_client();
        roon_plugin = roon.get();
        pm.register_plugin(std::move(roon));
    }

    ~CustomModels()
    {
        expect<MockMessages::MsgInfo>(mock_messages,
                                      "Unregistered plugin \"Roon\"", false);
        pm.shutdown();

        try
        {
            roon_update.check();
            mock_messages->done();
        }
        catch(...)
        {
            /* no throwing from dtors */
        }

        MockMessages::singleton = nullptr;
    }

  protected:
    void expect_equal(const nlohmann::json &expected, const nlohmann::json &have)
    {
        const auto d(nlohmann::json::diff(have, expected));

        if(d.empty())
            return;

        MESSAGE("Diff: " << d);
        CHECK(have == expected);
    }
};

TEST_CASE_FIXTURE(CustomModels,
                  "Settings for simplest possible model with source, DSP, and sink")
{
    const auto model_definition = R"(
        {
          "all_devices": {
            "MyDevice": {
              "audio_sources": [{ "id": "bluetooth" }],
              "audio_sinks": [
                {
                  "id": "analog_line_out",
                  "roon": { "rank": 0, "method": "analog" }
                }
              ],
              "elements": [
                {
                  "id": "dsp",
                  "element": {
                    "controls": {
                      "volume": {
                        "type": "range", "value_type": "y",
                        "min": 0, "max": 99, "step": 1, "scale": "steps",
                        "neutral_setting": 0,
                        "roon": {
                          "rank": 0,
                          "template": { "type": "digital_volume", "quality": "high" },
                          "value_name": "gain",
                          "value_mapping": { "type": "direct", "value_type": "d" }
                        }
                      },
                      "balance": {
                        "type": "range", "value_type": "Y",
                        "min": -16, "max": 16, "step": 1, "scale": "steps",
                        "neutral_setting": 0,
                        "roon": {
                          "rank": 1,
                          "template": { "type": "balance", "quality": "lossless" },
                          "value_name": "value",
                          "value_mapping": {
                            "type": "to_range", "value_type": "d",
                            "from": -1.0, "to": 1.0
                          }
                        }
                      }
                    }
                  }
                }
              ],
              "audio_signal_paths": [
                {
                  "connections": {
                    "bluetooth": "dsp",
                    "dsp": "analog_line_out"
                  }
                }
              ]
            }
          }
        })";

    CHECK(models.loads(model_definition));

    const auto input = R"(
        {
          "audio_path_changes": [
            { "op": "add_instance", "name": "self", "id": "MyDevice" },
            {
              "op": "set", "element": "self.dsp",
              "kv": {
                "volume":  { "type": "y", "value": 42 },
                "balance": { "type": "Y", "value": -4 }
              }
            }
          ]
        })";
    settings.update(input);

    ConfigStore::Changes changes;
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));

    const auto expected_update = R"(
        [
          { "type": "digital_volume", "gain": 42,         "quality": "high" },
          { "type": "balance",        "value": -0.25,     "quality": "lossless" },
          { "type": "output",         "method": "analog", "quality": "lossless" }
        ]
    )";

    roon_update.expect(expected_update);
    pm.report_changes(settings, changes);
}

TEST_CASE_FIXTURE(CustomModels, "Settings for linear model with NOP elements")
{
    const auto model_definition = R"(
        {
          "all_devices": {
            "MyDevice": {
              "audio_sources": [{ "id": "bluetooth" }],
              "audio_sinks": [
                {
                  "id": "analog_line_out",
                  "roon": { "rank": 0, "method": "analog" }
                }
              ],
              "elements": [
                {
                  "id": "dsp",
                  "element": {
                    "controls": {
                      "volume": {
                        "type": "range", "value_type": "y",
                        "min": 0, "max": 99, "step": 1, "scale": "steps",
                        "neutral_setting": 0,
                        "roon": {
                          "rank": 0,
                          "template": { "type": "digital_volume", "quality": "high" },
                          "value_name": "gain",
                          "value_mapping": { "type": "direct", "value_type": "d" }
                        }
                      },
                      "balance": {
                        "type": "range", "value_type": "Y",
                        "min": -16, "max": 16, "step": 1, "scale": "steps",
                        "neutral_setting": 0,
                        "roon": {
                          "rank": 1,
                          "template": { "type": "balance", "quality": "lossless" },
                          "value_name": "value",
                          "value_mapping": {
                            "type": "to_range", "value_type": "d",
                            "from": -1.0, "to": 1.0
                          }
                        }
                      }
                    }
                  }
                },
                { "id": "codec", "element": null },
                { "id": "dac", "element": null }
              ],
              "audio_signal_paths": [
                {
                  "connections": {
                    "bluetooth": "codec",
                    "codec": "dsp",
                    "dsp": "dac",
                    "dac": "analog_line_out"
                  }
                }
              ]
            }
          }
        })";

    CHECK(models.loads(model_definition));

    const auto input = R"(
        {
          "audio_path_changes": [
            { "op": "add_instance", "name": "self", "id": "MyDevice" },
            {
              "op": "set", "element": "self.dsp",
              "kv": {
                "volume":  { "type": "y", "value": 42 },
                "balance": { "type": "Y", "value": -4 }
              }
            }
          ]
        })";
    settings.update(input);

    ConfigStore::Changes changes;
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));

    const auto expected_update = R"(
        [
          { "type": "digital_volume", "gain": 42,         "quality": "high" },
          { "type": "balance",        "value": -0.25,     "quality": "lossless" },
          { "type": "output",         "method": "analog", "quality": "lossless" }
        ]
    )";

    roon_update.expect(expected_update);
    pm.report_changes(settings, changes);
}

TEST_CASE_FIXTURE(CustomModels, "Subsequent changes of Roon-related settings")
{
    const auto model_definition = R"(
        {
          "all_devices": {
            "MyDevice": {
              "audio_sources": [{ "id": "bluetooth" }],
              "audio_sinks": [
                {
                  "id": "analog_line_out",
                  "roon": { "rank": 0, "method": "analog" }
                }
              ],
              "elements": [
                {
                  "id": "dsp",
                  "element": {
                    "controls": {
                      "volume": {
                        "type": "range", "value_type": "y",
                        "min": 0, "max": 99, "step": 1, "scale": "steps",
                        "neutral_setting": 0,
                        "roon": {
                          "rank": 0,
                          "template": { "type": "digital_volume", "quality": "high" },
                          "value_name": "gain",
                          "value_mapping": { "type": "direct", "value_type": "d" }
                        }
                      },
                      "balance": {
                        "type": "range", "value_type": "Y",
                        "min": -16, "max": 16, "step": 1, "scale": "steps",
                        "neutral_setting": 0,
                        "roon": {
                          "rank": 1,
                          "template": { "type": "balance", "quality": "lossless" },
                          "value_name": "value",
                          "value_mapping": {
                            "type": "to_range", "value_type": "d",
                            "from": -1.0, "to": 1.0
                          }
                        }
                      }
                    }
                  }
                },
                { "id": "codec", "element": null },
                { "id": "dac", "element": null }
              ],
              "audio_signal_paths": [
                {
                  "connections": {
                    "bluetooth": "codec",
                    "codec": "dsp",
                    "dsp": "dac",
                    "dac": "analog_line_out"
                  }
                }
              ]
            }
          }
        })";

    CHECK(models.loads(model_definition));

    const auto init_self = R"(
        {
          "audio_path_changes": [
            { "op": "add_instance", "name": "self", "id": "MyDevice" }
          ]
        })";
    settings.update(init_self);

    ConfigStore::Changes changes;
    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    const auto expected_update_after_init = R"(
        [ { "type": "output", "method": "analog", "quality": "lossless" } ]
    )";
    roon_update.expect(expected_update_after_init);
    pm.report_changes(settings, changes);
    changes.reset();
    roon_update.check();

    /* balance changed */
    const auto balance_changed = R"(
        {
          "audio_path_changes": [
            {
              "op": "update", "element": "self.dsp",
              "kv": { "balance": { "type": "Y", "value": -4 } }
            }
          ]
        }
    )";
    settings.update(balance_changed);

    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    const auto expected_balance_update = R"(
        [
          { "type": "balance", "value": -0.25,     "quality": "lossless" },
          { "type": "output",  "method": "analog", "quality": "lossless" }
        ]
    )";
    roon_update.expect(expected_balance_update);
    pm.report_changes(settings, changes);
    changes.reset();
    roon_update.check();

    /* volume changed */
    const auto volume_changed = R"(
        {
          "audio_path_changes": [
            {
              "op": "update", "element": "self.dsp",
              "kv": { "volume": { "type": "y", "value": 42 } }
            }
          ]
        }
    )";
    settings.update(volume_changed);

    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    const auto expected_volume_update = R"(
        [
          { "type": "digital_volume", "gain": 42,         "quality": "high" },
          { "type": "balance",        "value": -0.25,     "quality": "lossless" },
          { "type": "output",         "method": "analog", "quality": "lossless" }
        ]
    )";
    roon_update.expect(expected_volume_update);
    pm.report_changes(settings, changes);
    changes.reset();
    roon_update.check();

    /* balance back to neutral, volume unchanged */
    const auto balance_neutral = R"(
        {
          "audio_path_changes": [
            {
              "op": "update", "element": "self.dsp",
              "kv": { "balance": { "type": "Y", "value": 0 } }
            }
          ]
        }
    )";
    settings.update(balance_neutral);

    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    const auto expected_balance_neutral_update = R"(
        [
          { "type": "digital_volume", "gain": 42,         "quality": "high" },
          { "type": "output",         "method": "analog", "quality": "lossless" }
        ]
    )";

    roon_update.expect(expected_balance_neutral_update);
    pm.report_changes(settings, changes);
}

TEST_SUITE_END();

TEST_SUITE_BEGIN("Roon output plugin, connected appliances");

class ConnFixture
{
  protected:
    ClientPlugin::PluginManager pm;
    StaticModels::DeviceModelsDatabase models;
    ConfigStore::Settings settings;
    std::unique_ptr<MockMessages::Mock> mock_messages;

    ClientPlugin::Roon *roon_plugin;
    RoonUpdate roon_update;

  public:
    explicit ConnFixture():
        settings(models),
        mock_messages(std::make_unique<MockMessages::Mock>()),
        roon_plugin(nullptr)
    {
        MockMessages::singleton = mock_messages.get();

        expect<MockMessages::MsgInfo>(mock_messages,
                                      "Registered plugin \"Roon\"", false);

        auto roon(std::make_unique<ClientPlugin::Roon>(
            [this] (const auto &asp, const auto &extra) { roon_update.send(asp, extra); }));
        roon->add_client();
        roon_plugin = roon.get();
        pm.register_plugin(std::move(roon));

        if(!models.load("test_player_and_amplifier.json", true))
            models.load("tests/test_player_and_amplifier.json");
    }

    ~ConnFixture()
    {
        expect<MockMessages::MsgInfo>(mock_messages,
                                      "Unregistered plugin \"Roon\"", false);
        pm.shutdown();

        try
        {
            roon_update.check();
            mock_messages->done();
        }
        catch(...)
        {
            /* no throwing from dtors */
        }

        MockMessages::singleton = nullptr;
    }

  protected:
    void expect_equal(const nlohmann::json &expected, const nlohmann::json &have)
    {
        const auto d(nlohmann::json::diff(have, expected));

        if(d.empty())
            return;

        MESSAGE("Diff: " << d);
        CHECK(have == expected);
    }
};

TEST_CASE_FIXTURE(ConnFixture, "Player is connected to one amplifier, headphones are plugged here and there")
{
    /* the appliance we are running in introduces itself and tells us it is
     * configured to play from Bluetooth to the analog line output */
    const auto init_self = R"(
        {
          "audio_path_changes": [
            { "op": "add_instance", "name": "self", "id": "Player" },
            {
              "op": "set", "element": "self.input_select",
              "kv": { "src": { "type": "s", "value": "bt" } }
            },
            {
              "op": "set", "element": "self.output_select",
              "kv": { "hp_plugged": { "type": "b", "value": false } }
            },
            {
              "op": "set", "element": "self.dsp",
              "kv": {
                "balance": { "type": "Y", "value": -1 },
                "volume": { "type": "y", "value": 30 }
              }
            }
          ]
        })";
    settings.update(init_self);

    ConfigStore::Changes changes;
    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    const auto expected_for_init_self = R"(
        [
          { "type": "digital_volume", "quality": "high", "gain": 30.0 },
          { "type": "balance", "quality": "lossless", "gain": -0.0625 },
          { "type": "output", "method": "analog", "quality": "lossless" }
        ]
    )";
    roon_update.expect(expected_for_init_self);
    pm.report_changes(settings, changes);
    changes.reset();
    roon_update.check();

    /* an amplifier has been connected/detected to the analog line output so
     * that its first analog input is routed to the speakers */
    const auto amp_connected = R"(
        {
          "audio_path_changes": [
            { "op": "add_instance", "name": "amp", "id": "Amplifier" },
            {
              "op": "set", "element": "amp.input_select",
              "kv": { "src": { "type": "s", "value": "in_1" } }
            },
            {
              "op": "set", "element": "amp.output_select",
              "kv": { "hp_plugged": { "type": "b", "value": false } }
            },
            {
              "op": "set", "element": "amp.bass",
              "kv": { "level": { "type": "Y", "value": 2 } }
            },
            {
              "op": "set", "element": "amp.amp",
              "kv": { "enable": { "type": "b", "value": true } }
            },
            {
              "op": "connect",
              "from": "self.analog_line_out", "to": "amp.analog_in_1"
            }
          ]
        })";
    settings.update(amp_connected);

    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    const auto expected_for_amp_connected = R"(
        [
          { "type": "digital_volume", "quality": "high", "gain": 30.0 },
          { "type": "balance", "quality": "lossless", "gain": -0.0625 },
          { "type": "eq", "sub_type": "bass_management", "quality": "enhanced", "gain": 2.0 },
          { "type": "output", "method": "speakers", "quality": "lossless" }
        ]
    )";
    roon_update.expect(expected_for_amp_connected);
    pm.report_changes(settings, changes);
    changes.reset();
    roon_update.check();

    /* plug headphones into the player */
    const auto headphones_plugged_into_player = R"(
        {
          "audio_path_changes": [
            {
              "op": "set", "element": "self.output_select",
              "kv": { "hp_plugged": { "type": "b", "value": true } }
            }
          ]
        })";
    settings.update(headphones_plugged_into_player);

    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    const auto expected_for_headphones_plugged_into_player = R"(
        [
          { "type": "digital_volume", "quality": "high", "gain": 30.0 },
          { "type": "balance", "quality": "lossless", "gain": -0.0625 },
          { "type": "output", "method": "headphones", "quality": "lossless" }
        ]
    )";
    roon_update.expect(expected_for_headphones_plugged_into_player);
    pm.report_changes(settings, changes);
    changes.reset();
    roon_update.check();

    /* unplug headphones from the player, audio path is back to the previous
     * path */
    const auto headphones_pulled_from_player = R"(
        {
          "audio_path_changes": [
            {
              "op": "set", "element": "self.output_select",
              "kv": { "hp_plugged": { "type": "b", "value": false } }
            }
          ]
        })";
    settings.update(headphones_pulled_from_player);

    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    roon_update.expect(expected_for_amp_connected);
    pm.report_changes(settings, changes);
    changes.reset();
    roon_update.check();

    /* now plug the headphones into the amp */
    const auto headphones_plugged_into_amp = R"(
        {
          "audio_path_changes": [
            {
              "op": "set", "element": "amp.output_select",
              "kv": { "hp_plugged": { "type": "b", "value": true } }
            }
          ]
        })";
    settings.update(headphones_plugged_into_amp);

    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    const auto expected_for_headphones_plugged_into_amp = R"(
        [
          { "type": "digital_volume", "quality": "high", "gain": 30.0 },
          { "type": "balance", "quality": "lossless", "gain": -0.0625 },
          { "type": "eq", "sub_type": "bass_management", "quality": "enhanced", "gain": 2.0 },
          { "type": "output", "method": "headphones", "quality": "lossless" }
        ]
    )";
    roon_update.expect(expected_for_headphones_plugged_into_amp);
    pm.report_changes(settings, changes);
    changes.reset();
    roon_update.check();

    /* late report from amplifier that its internal output stage has been
     * disabled to conserve power and avoid noise from the otherwise silent
     * speakers: no report for Roon is expected */
    const auto amp_output_power_down = R"(
        {
          "audio_path_changes": [
            {
              "op": "set", "element": "amp.amp",
              "kv": { "enable": { "type": "b", "value": false } }
            }
          ]
        })";
    settings.update(amp_output_power_down);

    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    pm.report_changes(settings, changes);
    roon_update.check();
}

TEST_CASE_FIXTURE(ConnFixture, "Player is connected to one amplifier, single configuration")
{
    const auto init_compound = R"(
        {
          "audio_path_changes": [
            { "op": "add_instance", "name": "self", "id": "Player" },
            { "op": "add_instance", "name": "amp", "id": "Amplifier" },
            {
              "op": "connect",
              "from": "self.analog_line_out", "to": "amp.analog_in_1"
            },
            {
              "op": "set", "element": "self.input_select",
              "kv": { "src": { "type": "s", "value": "bt" } }
            },
            {
              "op": "set", "element": "self.output_select",
              "kv": { "hp_plugged": { "type": "b", "value": false } }
            },
            {
              "op": "set", "element": "self.dsp",
              "kv": {
                "balance": { "type": "Y", "value": 2 },
                "volume": { "type": "y", "value": 13 }
              }
            },
            {
              "op": "set", "element": "amp.input_select",
              "kv": { "src": { "type": "s", "value": "in_1" } }
            },
            {
              "op": "set", "element": "amp.output_select",
              "kv": { "hp_plugged": { "type": "b", "value": true } }
            },
            {
              "op": "set", "element": "amp.amp",
              "kv": { "enable": { "type": "b", "value": true } }
            },
            {
              "op": "set", "element": "amp.bass",
              "kv": { "level": { "type": "Y", "value": -3 } }
            }
          ]
        })";
    settings.update(init_compound);

    ConfigStore::Changes changes;
    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    const auto expected_for_headphones_plugged_into_amp = R"(
        [
          { "type": "digital_volume", "quality": "high", "gain": 13.0 },
          { "type": "balance", "quality": "lossless", "gain": 0.125 },
          { "type": "eq", "sub_type": "bass_management", "quality": "enhanced", "gain": -3.0 },
          { "type": "output", "method": "headphones", "quality": "lossless" }
        ]
    )";
    roon_update.expect(expected_for_headphones_plugged_into_amp);
    pm.report_changes(settings, changes);
}

TEST_CASE_FIXTURE(ConnFixture, "Player is connected to two amplifiers, switching amplifier inputs")
{
    const auto init_compound = R"(
        {
          "audio_path_changes": [
            { "op": "add_instance", "name": "self", "id": "Player" },
            { "op": "add_instance", "name": "amp_A", "id": "Amplifier" },
            { "op": "add_instance", "name": "amp_B", "id": "Amplifier" },
            {
              "op": "connect",
              "from": "self.analog_line_out", "to": "amp_A.analog_in_1"
            },
            {
              "op": "connect",
              "from": "self.analog_line_out", "to": "amp_B.analog_in_1"
            },
            {
              "op": "set", "element": "self.input_select",
              "kv": { "src": { "type": "s", "value": "bt" } }
            },
            {
              "op": "set", "element": "self.output_select",
              "kv": { "hp_plugged": { "type": "b", "value": false } }
            },
            {
              "op": "set", "element": "self.dsp",
              "kv": {
                "balance": { "type": "Y", "value": 1 },
                "volume": { "type": "y", "value": 24 }
              }
            },
            {
              "op": "set", "element": "amp_A.input_select",
              "kv": { "src": { "type": "s", "value": "in_1" } }
            },
            {
              "op": "set", "element": "amp_A.output_select",
              "kv": { "hp_plugged": { "type": "b", "value": false } }
            },
            {
              "op": "set", "element": "amp_A.amp",
              "kv": { "enable": { "type": "b", "value": true } }
            },
            {
              "op": "set", "element": "amp_A.bass",
              "kv": { "level": { "type": "Y", "value": -3 } }
            },
            {
              "op": "set", "element": "amp_B.input_select",
              "kv": { "src": { "type": "s", "value": "in_2" } }
            },
            {
              "op": "set", "element": "amp_B.output_select",
              "kv": { "hp_plugged": { "type": "b", "value": true } }
            },
            {
              "op": "set", "element": "amp_B.amp",
              "kv": { "enable": { "type": "b", "value": false } }
            },
            {
              "op": "set", "element": "amp_B.bass",
              "kv": { "level": { "type": "Y", "value": 5 } }
            }
          ]
        })";
    settings.update(init_compound);

    ConfigStore::Changes changes;
    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    const auto expected_for_init_compound = R"(
        [
          { "type": "digital_volume", "quality": "high", "gain": 24.0 },
          { "type": "balance", "quality": "lossless", "gain": 0.0625 },
          { "type": "eq", "sub_type": "bass_management", "quality": "enhanced", "gain": -3.0 },
          { "type": "output", "method": "speakers", "quality": "lossless" }
        ]
    )";
    roon_update.expect(expected_for_init_compound);
    pm.report_changes(settings, changes);
    changes.reset();
    roon_update.check();

    /* now switch the inputs on both amplifiere so that the audio path goes
     * into the headphones connected to amplifier B */
    const auto switched_amp_inputs = R"(
        {
          "audio_path_changes": [
            {
              "op": "set", "element": "amp_A.input_select",
              "kv": { "src": { "type": "s", "value": "in_2" } }
            },
            {
              "op": "set", "element": "amp_B.input_select",
              "kv": { "src": { "type": "s", "value": "in_1" } }
            }
          ]
        })";
    settings.update(switched_amp_inputs);

    {
    ConfigStore::SettingsJSON js(settings);
    CHECK(js.extract_changes(changes));
    }

    const auto expected_for_switched_amp_inputs = R"(
        [
          { "type": "digital_volume", "quality": "high", "gain": 24.0 },
          { "type": "balance", "quality": "lossless", "gain": 0.0625 },
          { "type": "eq", "sub_type": "bass_management", "quality": "enhanced", "gain": 5.0 },
          { "type": "output", "method": "headphones", "quality": "lossless" }
        ]
    )";
    roon_update.expect(expected_for_switched_amp_inputs);
    pm.report_changes(settings, changes);
}

TEST_SUITE_END();
