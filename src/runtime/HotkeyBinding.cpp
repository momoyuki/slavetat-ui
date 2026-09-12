#include "runtime/HotkeyBinding.h"

#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace stui::runtime {
namespace {

const std::unordered_map<std::string, std::uint32_t> kNamedKeys = {
    {"F1", 0x3B}, {"F2", 0x3C}, {"F3", 0x3D}, {"F4", 0x3E},
    {"F5", 0x3F}, {"F6", 0x40}, {"F7", 0x41}, {"F8", 0x42},
    {"F9", 0x43}, {"F10", 0x44}, {"F11", 0x57}, {"F12", 0x58},
    {"INSERT", 0xD2}, {"DELETE", 0xD3}, {"HOME", 0xC7}, {"END", 0xCF},
    {"PAGEUP", 0xC9}, {"PAGEDOWN", 0xD1}, {"TILDE", 0x29},
    {"BACKSLASH", 0x2B}, {"NUMPAD0", 0x52}, {"NUMPAD1", 0x4F},
    {"NUMPAD2", 0x50}, {"NUMPAD3", 0x51}, {"NUMPAD4", 0x4B},
    {"NUMPAD5", 0x4C}, {"NUMPAD6", 0x4D}, {"NUMPAD7", 0x47},
    {"NUMPAD8", 0x48}, {"NUMPAD9", 0x49},
};

}  // namespace

HotkeyBinding::HotkeyBinding(std::filesystem::path configPath)
    : configPath_(std::move(configPath)) {}

bool HotkeyBinding::load() {
    const std::scoped_lock lock(mutex_);
    if (!std::filesystem::exists(configPath_)) {
        key_.reset();
        return save();
    }
    std::ifstream input(configPath_);
    if (!input) {
        return false;
    }
    const auto config = nlohmann::json::parse(input, nullptr, false);
    if (config.is_discarded() || !config.is_object() || !config.contains("hotkey")) {
        return false;
    }
    const auto& hotkey = config.at("hotkey");
    if (hotkey.is_null()) {
        key_.reset();
        return true;
    }
    if (hotkey.is_number_unsigned()) {
        key_ = hotkey.get<std::uint32_t>();
        return true;
    }
    if (hotkey.is_number_integer()) {
        const auto value = hotkey.get<std::int64_t>();
        if (value >= 0 && value <= UINT32_MAX) {
            key_ = static_cast<std::uint32_t>(value);
            return true;
        }
        return false;
    }
    if (hotkey.is_string()) {
        auto name = hotkey.get<std::string>();
        for (auto& character : name) {
            character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
        }
        if (const auto found = kNamedKeys.find(name); found != kNamedKeys.end()) {
            key_ = found->second;
            return true;
        }
    }
    return false;
}

void HotkeyBinding::beginCapture() noexcept {
    const std::scoped_lock lock(mutex_);
    capturing_ = true;
}

void HotkeyBinding::cancelCapture() noexcept {
    const std::scoped_lock lock(mutex_);
    capturing_ = false;
}

bool HotkeyBinding::isCapturing() const noexcept {
    const std::scoped_lock lock(mutex_);
    return capturing_;
}

bool HotkeyBinding::capture(std::uint32_t key) {
    const std::scoped_lock lock(mutex_);
    const auto previous = key_;
    key_ = key;
    capturing_ = false;
    if (save()) {
        return true;
    }
    key_ = previous;
    return false;
}

bool HotkeyBinding::clear() {
    const std::scoped_lock lock(mutex_);
    const auto previous = key_;
    key_.reset();
    capturing_ = false;
    if (save()) {
        return true;
    }
    key_ = previous;
    return false;
}

bool HotkeyBinding::matches(std::uint32_t key) const noexcept {
    const std::scoped_lock lock(mutex_);
    return key_ && *key_ == key;
}

std::optional<std::uint32_t> HotkeyBinding::key() const noexcept {
    const std::scoped_lock lock(mutex_);
    return key_;
}

std::string HotkeyBinding::label() const {
    const std::scoped_lock lock(mutex_);
    if (!key_) {
        return "None";
    }
    for (const auto& [name, value] : kNamedKeys) {
        if (value == *key_) {
            return name;
        }
    }
    std::ostringstream label;
    label << "DIK 0x" << std::uppercase << std::hex << *key_;
    return label.str();
}

bool HotkeyBinding::save() const {
    nlohmann::json config = nlohmann::json::object();
    std::ifstream existing(configPath_);
    if (existing) {
        auto parsed = nlohmann::json::parse(existing, nullptr, false);
        if (!parsed.is_discarded() && parsed.is_object()) {
            config = std::move(parsed);
        }
    }
    config["hotkey"] = key_ ? nlohmann::json(*key_) : nlohmann::json(nullptr);
    std::ofstream output(configPath_);
    if (!output) {
        return false;
    }
    output << config.dump(2);
    return output.good();
}

}  // namespace stui::runtime
