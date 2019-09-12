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

#include "report_roon.hh"

#include "client_plugin_manager.hh"
#include "configstore.hh"
#include "configstore_json.hh"
#include "configstore_changes.hh"
#include "device_models.hh"

#include "mock_messages.hh"

TEST_SUITE_BEGIN("Roon output plugin");

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
    const std::string input = R"(
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
    const std::string init_with_tone_control_enabled = R"(
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
    roon_update.check();

    /* switching off tone control suppresses equalizer settings */
    const std::string disable_tone_control = R"(
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
    roon_update.check();

    /* switching tone control back on emits path including equalizer settings
     * with previously set values */
    const std::string enable_tone_control = R"(
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

TEST_SUITE_END();
