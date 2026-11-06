#include "runtime/ThumbnailCacheSetting.h"
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
void settingsValidateAndPreserveOtherKeys() {
    const auto path = std::filesystem::temp_directory_path() / "SlaveTatsUICacheLimitTests.json";
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    auto config = std::make_shared<stui::runtime::PluginConfigFile>(path);
    stui::runtime::ThumbnailCacheSetting setting(config);
    expect(setting.load() && setting.limitMiB() == 256, "missing limit defaults to 256 MiB");
    expect(config->update([](nlohmann::json& value) -> stui::runtime::ConfigUpdateResult {
        value["hotkey"] = 67;
        value["menuSide"] = "right";
        value["favorites"] = {"keep"};
        return {};
    }).has_value(), "fixture should save");
    for (const auto limit : {128U, 256U, 512U, 1024U}) {
        expect(setting.select(limit), "approved limit should save");
        stui::runtime::ThumbnailCacheSetting restored(config);
        expect(restored.load() && restored.limitMiB() == limit, "saved limit should restore");
    }
    expect(setting.select(256), "reset to default should save");
    const auto saved = config->read();
    expect(saved && saved->at("hotkey") == 67 && saved->at("menuSide") == "right" &&
        saved->at("favorites") == nlohmann::json({"keep"}), "save preserves other settings");
    for (const auto invalid : {0U, 127U, 255U, 2048U, 4294967295U}) {
        expect(!setting.select(invalid) && setting.limitMiB() == 256,
            "invalid selections must preserve applied value");
    }
    for (const auto* invalid : {"true", "null", "\"512\"", "512.0", "-128", "0", "257",
             "4294967552", "18446744073709551615", "{}", "[]"}) {
        expect(setting.select(512), "fixture should select nondefault");
        { std::ofstream output(path); output << "{\"thumbnailDiskCacheLimitMiB\":" << invalid << "}"; }
        expect(!setting.load() && setting.limitMiB() == 256,
            "invalid config must fall back to default without conversion overflow");
    }
    { std::ofstream output(path); output << "broken"; }
    expect(!setting.select(512) && setting.limitMiB() == 256,
        "failed save must preserve the previous value");
    expect(!setting.load() && setting.limitMiB() == 256, "broken config defaults safely");
    stui::runtime::ThumbnailCacheSetting unavailable(nullptr);
    expect(!unavailable.load() && !unavailable.select(128) && unavailable.limitMiB() == 256,
        "unavailable config stays safely at default");
    std::filesystem::remove(path, ignored);
}
}
int main() {
    try { settingsValidateAndPreserveOtherKeys(); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    std::cout << "PASS thumbnail cache setting\n";
}
