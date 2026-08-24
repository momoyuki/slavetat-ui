#include "pch.h"
#include "Bridge.h"
#include "SlaveTatsNG_Interface.h"
#include "repository/TattooCatalogStore.h"

#include <array>

using namespace stui;

// ── Config (SlaveTatsUI.json) ─────────────────────────────────────────────────
namespace {

repository::TattooCatalogStore g_tattooCatalogStore;

}  // namespace


namespace Config {

inline uint32_t hotkeyDIK = 0x42;  // default: F8

static const std::unordered_map<std::string, uint32_t> kKeyNames = {
    {"F1",0x3B},  {"F2",0x3C},  {"F3",0x3D},  {"F4",0x3E},
    {"F5",0x3F},  {"F6",0x40},  {"F7",0x41},  {"F8",0x42},
    {"F9",0x43},  {"F10",0x44}, {"F11",0x57}, {"F12",0x58},
    {"INSERT",0xD2}, {"DELETE",0xD3}, {"HOME",0xC7}, {"END",0xCF},
    {"PAGEUP",0xC9}, {"PAGEDOWN",0xD1},
    {"TILDE",0x29},  {"BACKSLASH",0x2B},
    {"NUMPAD0",0x52},{"NUMPAD1",0x4F},{"NUMPAD2",0x50},{"NUMPAD3",0x51},
    {"NUMPAD4",0x4B},{"NUMPAD5",0x4C},{"NUMPAD6",0x4D},
    {"NUMPAD7",0x47},{"NUMPAD8",0x48},{"NUMPAD9",0x49},
};

inline void load(const std::filesystem::path& dir) {
    auto path = dir / "SlaveTatsUI.json";

    if (!std::filesystem::exists(path)) {
        // Write defaults on first run
        nlohmann::json def;
        def["hotkey"]   = "F8";
        def["_comment"] = "hotkey: F1-F12, INSERT, DELETE, HOME, END, PAGEUP, PAGEDOWN, TILDE, BACKSLASH, NUMPAD0-9, or DIK scancode integer";
        std::ofstream f(path);
        if (f) f << def.dump(2);
        logger::info("SlaveTatsUI: created default config at {}", path.string());
        return;
    }

    std::ifstream f(path);
    if (!f) { logger::warn("SlaveTatsUI: cannot open config {}", path.string()); return; }

    auto j = nlohmann::json::parse(f, nullptr, false);
    if (j.is_discarded()) { logger::warn("SlaveTatsUI: config JSON parse error"); return; }

    if (j.contains("hotkey")) {
        auto& hk = j["hotkey"];
        if (hk.is_number_integer()) {
            hotkeyDIK = hk.get<uint32_t>();
        } else if (hk.is_string()) {
            std::string name = hk.get<std::string>();
            for (char& c : name) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            auto it = kKeyNames.find(name);
            if (it != kKeyNames.end()) hotkeyDIK = it->second;
            else logger::warn("SlaveTatsUI: unknown key name '{}' — using F8", name);
        }
    }

    logger::info("SlaveTatsUI: hotkey DIK=0x{:X}", hotkeyDIK);
}

}  // namespace Config

// ── Input handler ─────────────────────────────────────────────────────────────

class InputSink : public RE::BSTEventSink<RE::InputEvent*> {
public:
    static InputSink* get() {
        static InputSink inst;
        return &inst;
    }

    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event,
                                          RE::BSTEventSource<RE::InputEvent*>*) override {
        if (!a_event) return RE::BSEventNotifyControl::kContinue;

        for (auto* e = *a_event; e; e = e->next) {
            auto* btn = e->AsButtonEvent();
            if (!btn || !btn->IsDown()) continue;

            if (btn->GetIDCode() == Config::hotkeyDIK) {
                Bridge::get()->toggleUI();
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};

// ── SKSE message handlers ─────────────────────────────────────────────────────

static void onSKSEMessage(SKSE::MessagingInterface::Message* msg) {
    switch (msg->type) {
    case SKSE::MessagingInterface::kDataLoaded:
        RE::BSInputDeviceManager::GetSingleton()->AddEventSink(InputSink::get());
        {
            std::array<wchar_t, 32768> executablePath{};
            const DWORD pathLength = GetModuleFileNameW(
                nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));

            if (pathLength == 0 || pathLength >= executablePath.size()) {
                logger::warn(
                    "SlaveTatsUI: cannot locate Skyrim executable for effective JSON scan (error={})",
                    GetLastError());
            } else try {
                const auto sourceDirectory = std::filesystem::path(
                    executablePath.data(), executablePath.data() + pathLength).parent_path()
                    / L"Data" / repository::kTattooSourceRelativeDirectory;
                auto catalog = g_tattooCatalogStore.refresh(sourceDirectory);

                if (!catalog) {
                    logger::warn(
                        "SlaveTatsUI: effective JSON scan failed for '{}': {}",
                        sourceDirectory.string(),
                        catalog.error().message());
                } else {
                    logger::info(
                        "SlaveTatsUI: loaded {} tattoo definitions from {} sources ({} issues)",
                        (*catalog)->repository.query().totalEntries,
                        (*catalog)->sourceCount,
                        (*catalog)->issues.size());
                    for (const auto& issue : (*catalog)->issues) {
                        if (issue.entryIndex) {
                            logger::warn(
                                "SlaveTatsUI: source '{}' entry {} skipped: {}",
                                issue.sourceId, *issue.entryIndex, issue.message);
                        } else {
                            logger::warn(
                                "SlaveTatsUI: source '{}' skipped: {}",
                                issue.sourceId, issue.message);
                        }
                    }
                }
            } catch (const std::exception& error) {
                logger::warn(
                    "SlaveTatsUI: effective JSON scan aborted without blocking UI: {}",
                    error.what());
            }
        }
        Bridge::get()->onDataLoaded();
        break;
    }
}

static void onSlaveTatsMessage(SKSE::MessagingInterface::Message* msg) {
    if (msg->type != slavetats::interface::Interface) return;

    // Peek version before the strict check so the UI can surface a named error
    uint32_t gotVersion = (msg->data && msg->dataLen >= 4)
        ? *reinterpret_cast<const uint32_t*>(msg->data)
        : 0u;

    auto* api = slavetats::interface::Addresses::from_void(msg->data);
    if (!api) {
        logger::error("SlaveTatsUI: SlaveTatsNG API version mismatch (got v{}, need v{})",
            gotVersion, static_cast<uint32_t>(slavetats::interface::Addresses::version));
        // Store peeked version so the UI can display a meaningful error on connect
        Bridge::get()->onSlaveTatsVersionMismatch(gotVersion);
        return;
    }
    Bridge::get()->onSlaveTatsInterface(api);
}

static void onJContainersMessage(SKSE::MessagingInterface::Message* msg) {
    logger::info("SlaveTatsUI: JContainers message received, type={}", msg->type);

    if (msg->type != jc::message_root_interface) return;

    auto* root = jc::root_interface::from_void(msg->data);
    if (!root) {
        logger::error("SlaveTatsUI: JContainers root_interface version mismatch (got {})",
            msg->data ? static_cast<jc::root_interface*>(msg->data)->current_version : 0u);
        return;
    }
    Bridge::get()->onJContainersReady(root);
}

// ── Plugin entry ──────────────────────────────────────────────────────────────

SKSEPluginLoad(const SKSE::LoadInterface* a_skse) {
    SKSE::Init(a_skse);

    // Try Documents\My Games\Skyrim Special Edition\SKSE\ first,
    // then fall back to Data\SKSE\Plugins\ beside Skyrim.exe.
    auto setupLog = [&]() -> std::filesystem::path {
        if (auto p = SKSE::log::log_directory()) {
            std::error_code ec;
            std::filesystem::create_directories(*p, ec);
            if (!ec) return *p / "SlaveTatsUI.log";
        }
        wchar_t exe[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        auto dir = std::filesystem::path(exe).parent_path() / L"Data" / L"SKSE" / L"Plugins";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return dir / "SlaveTatsUI.log";
    };

    std::filesystem::path pluginDir;
    try {
        auto logPath = setupLog();
        pluginDir = logPath.parent_path();
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
        auto log  = std::make_shared<spdlog::logger>("SlaveTatsUI", std::move(sink));
        log->set_level(spdlog::level::debug);
        log->flush_on(spdlog::level::debug);
        spdlog::set_default_logger(std::move(log));
    } catch (...) {}

    Config::load(pluginDir);

    auto* msg = SKSE::GetMessagingInterface();
    if (!msg) {
        logger::critical("SlaveTatsUI: messaging interface unavailable");
        return false;
    }

    msg->RegisterListener("SKSE",          onSKSEMessage);
    msg->RegisterListener("SlaveTatsNG",   onSlaveTatsMessage);
    msg->RegisterListener("JContainers64", onJContainersMessage);

    logger::info("SlaveTatsUI: loaded — toggle hotkey DIK=0x{:X} (edit SlaveTatsUI.json to change)", Config::hotkeyDIK);
    return true;
}
