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
    bool expecting_full_path_;
    bool update_was_sent_;

  public:
    RoonUpdate(const RoonUpdate &) = delete;
    RoonUpdate(RoonUpdate &&) = default;
    RoonUpdate &operator=(const RoonUpdate &) = delete;
    RoonUpdate &operator=(RoonUpdate &&) = default;

    explicit RoonUpdate():
        expecting_update_(false),
        expecting_full_path_(false),
        update_was_sent_(false)
    {}

    void expect(const char *expected, bool is_full_signal_path)
    {
        expect(nlohmann::json::parse(expected), is_full_signal_path);
    }

    void expect(nlohmann::json &&expected, bool is_full_signal_path)
    {
        expected_update_ = std::move(expected);
        expecting_update_ = true;
        expecting_full_path_ = is_full_signal_path;

        REQUIRE_FALSE(update_was_sent_);
        sent_update_ = nullptr;
        update_was_sent_ = false;
    }

    void send(const std::string &asp, bool is_full_signal_path)
    {
        CHECK(expecting_update_);
        CHECK(is_full_signal_path == expecting_full_path_);
        CHECK_FALSE(update_was_sent_);
        CHECK_FALSE(asp.empty());

        if(update_was_sent_)
        {
            MESSAGE("Multiple updates sent, keeping first");
            return;
        }

        sent_update_ = nlohmann::json::parse(asp);
        update_was_sent_ = true;

        // FIXME: Temporary workaround for random signal path ordering
        std::sort(expected_update_.begin(), expected_update_.end());
        std::sort(sent_update_.begin(), sent_update_.end());

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

    RoonUpdate roon_update;

  public:
    explicit Fixture():
        settings(models),
        mock_messages(std::make_unique<MockMessages::Mock>())
    {
        MockMessages::singleton = mock_messages.get();

        expect<MockMessages::MsgInfo>(mock_messages,
                                      "Registered plugin \"Roon\"", false);

        auto roon(std::make_unique<ClientPlugin::Roon>(
            [this] (const auto &asp, bool full) { roon_update.send(asp, full); }));
        roon->add_client();
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
};

TEST_CASE_FIXTURE(Fixture, "Passing empty changes has no side effects")
{
    ConfigStore::Changes changes;
    ConfigStore::SettingsJSON js(settings);
    CHECK_FALSE(js.extract_changes(changes));
    pm.report_changes(settings, changes);
}

TEST_CASE_FIXTURE(Fixture, "Settings for CALA CDR")
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
            { "type": "t+a", "sub_type": "virtual_surround"   , "quality": "enhanced" },
            { "type": "balance",                                "quality": "lossless", "value": 0.11764705882352944 },
            { "type": "t+a", "sub_type": "contour_fundamental", "quality": "enhanced", "gain": -1.0 },
            { "type": "eq",  "sub_type": "bass",                "quality": "enhanced", "gain": 1.0 },
            { "type": "t+a", "sub_type": "contour_presence",    "quality": "enhanced", "gain": 2.0 },
            { "type": "eq",  "sub_type": "mid",                 "quality": "enhanced", "gain": 0.5 }
        ]
    )";

    roon_update.expect(expected_update, false);
    pm.report_changes(settings, changes);
}

TEST_SUITE_END();
