#include "runtime/ThumbnailBackgroundSetting.h"
#include "runtime/PluginConfigFile.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace {
void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void backgroundPersistsAndRejectsInvalidConfiguration() {
    const auto path = std::filesystem::temp_directory_path() / "SlaveTatsUIBackgroundTests.json";
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    auto config = std::make_shared<stui::runtime::PluginConfigFile>(path);
    stui::runtime::ThumbnailBackgroundSetting setting(config);
    expect(setting.load() && !setting.white(), "missing preference defaults to black");
    expect(config->update([](nlohmann::json& value) -> stui::runtime::ConfigUpdateResult {
        value["hotkey"] = 67;
        value["menuSide"] = "right";
        value["favorites"] = {"keep"};
        return {};
    }).has_value(), "fixture should save");
    for (const bool white : {true, false}) {
        expect(setting.select(white), "selection should save");
        stui::runtime::ThumbnailBackgroundSetting restored(config);
        expect(restored.load() && restored.white() == white, "background restores after restart");
    }
    const auto saved = config->read();
    expect(saved && saved->at("hotkey") == 67 && saved->at("menuSide") == "right" &&
        saved->at("favorites") == nlohmann::json({"keep"}), "save preserves unrelated keys");
    for (const auto* invalid : {"true", "null", "1", "\"White\"", "\"checkerboard\"", "{}", "[]"}) {
        expect(setting.select(true), "fixture selects white");
        { std::ofstream output(path); output << "{\"thumbnailBackground\":" << invalid << "}"; }
        expect(!setting.load() && !setting.white(), "invalid value falls back to black");
    }
    { std::ofstream output(path); output << "{}"; }
    expect(setting.select(true), "white should save");
    { std::ofstream output(path); output << "broken"; }
    expect(!setting.select(false) && setting.white(), "failed save preserves displayed value");
    expect(!setting.load() && !setting.white(), "malformed configuration defaults to black");
    const auto missingParent = path / "missing-parent" / "settings.json";
    stui::runtime::ThumbnailBackgroundSetting unwritable(
        std::make_shared<stui::runtime::PluginConfigFile>(missingParent));
    expect(unwritable.load() && !unwritable.select(true) && !unwritable.white(),
        "failure creating the configuration file preserves black");
    stui::runtime::ThumbnailBackgroundSetting unavailable(nullptr);
    expect(!unavailable.load() && !unavailable.select(true) && !unavailable.white(),
        "unavailable store stays at black");
    std::filesystem::remove(path, ignored);
}
}
int main() {
    try { backgroundPersistsAndRejectsInvalidConfiguration(); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    std::cout << "PASS thumbnail background setting\n";
}
