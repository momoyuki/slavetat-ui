#include "runtime/HotkeyBinding.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace {

void expect(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

class TemporaryDirectory {
public:
    TemporaryDirectory()
        : path_(std::filesystem::temp_directory_path() / "SlaveTatsUIHotkeyBindingTests") {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

void missingConfigurationDefaultsToNoHotkey() {
    TemporaryDirectory directory;
    const auto configPath = directory.path() / "SlaveTatsUI.json";
    stui::runtime::HotkeyBinding binding(configPath);

    expect(binding.load(), "expected missing configuration to be created");
    expect(!binding.key(), "expected no default hotkey");
    expect(binding.label() == "None", "expected None label");

    std::ifstream input(configPath);
    const auto json = nlohmann::json::parse(input);
    expect(json.at("hotkey").is_null(), "expected persisted null hotkey");
}

void capturedKeyIsSavedAndRestored() {
    TemporaryDirectory directory;
    const auto configPath = directory.path() / "SlaveTatsUI.json";
    stui::runtime::HotkeyBinding binding(configPath);
    expect(binding.load(), "expected initial configuration");

    binding.beginCapture();
    expect(binding.isCapturing(), "expected key capture mode");
    expect(binding.capture(0x43), "expected F9 capture to save");
    expect(!binding.isCapturing(), "expected capture mode to finish");
    expect(binding.key() == 0x43, "expected captured F9 DIK code");
    expect(binding.label() == "F9", "expected captured key label");

    stui::runtime::HotkeyBinding restored(configPath);
    expect(restored.load(), "expected saved configuration to load");
    expect(restored.key() == 0x43, "expected persisted F9 DIK code");
}

void clearDisablesAndPersistsNoHotkey() {
    TemporaryDirectory directory;
    const auto configPath = directory.path() / "SlaveTatsUI.json";
    stui::runtime::HotkeyBinding binding(configPath);
    expect(binding.load(), "expected initial configuration");
    binding.beginCapture();
    expect(binding.capture(0x42), "expected F8 capture to save");
    expect(binding.matches(0x42), "expected configured F8 to match");

    expect(binding.clear(), "expected cleared binding to save");
    expect(!binding.key(), "expected cleared hotkey");
    expect(!binding.matches(0x42), "expected F8 to be disabled after clear");

    std::ifstream input(configPath);
    const auto json = nlohmann::json::parse(input);
    expect(json.at("hotkey").is_null(), "expected clear to persist null hotkey");
}

void canceledCapturePreservesExistingHotkey() {
    TemporaryDirectory directory;
    const auto configPath = directory.path() / "SlaveTatsUI.json";
    stui::runtime::HotkeyBinding binding(configPath);
    expect(binding.load(), "expected initial configuration");
    binding.beginCapture();
    expect(binding.capture(0x43), "expected initial F9 capture");

    binding.beginCapture();
    binding.cancelCapture();

    expect(!binding.isCapturing(), "expected canceled capture mode to finish");
    expect(binding.key() == 0x43, "expected canceled capture to preserve F9");
}

void legacyNamedHotkeyLoadsAndUnrelatedSettingsSurviveClear() {
    TemporaryDirectory directory;
    const auto configPath = directory.path() / "SlaveTatsUI.json";
    {
        std::ofstream output(configPath);
        output << R"({"hotkey":"F8","anotherSetting":true})";
    }

    stui::runtime::HotkeyBinding binding(configPath);
    expect(binding.load(), "expected legacy named hotkey to load");
    expect(binding.key() == 0x42, "expected legacy F8 DIK code");
    expect(binding.clear(), "expected legacy hotkey clear to save");

    std::ifstream input(configPath);
    const auto json = nlohmann::json::parse(input);
    expect(json.at("hotkey").is_null(), "expected legacy hotkey to clear");
    expect(json.at("anotherSetting") == true, "expected unrelated setting to survive clear");
}

}  // namespace

int main() {
    try {
        missingConfigurationDefaultsToNoHotkey();
        std::cout << "PASS missing configuration defaults to no hotkey\n";
        capturedKeyIsSavedAndRestored();
        std::cout << "PASS captured key is saved and restored\n";
        clearDisablesAndPersistsNoHotkey();
        std::cout << "PASS clear disables and persists no hotkey\n";
        canceledCapturePreservesExistingHotkey();
        std::cout << "PASS canceled capture preserves existing hotkey\n";
        legacyNamedHotkeyLoadsAndUnrelatedSettingsSurviveClear();
        std::cout << "PASS legacy named hotkey loads and unrelated settings survive clear\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
    return 0;
}
