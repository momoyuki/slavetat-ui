#include "pch.h"
#include "Bridge.h"
#include "SlaveTatsNG_Interface.h"

using namespace stui;

// ── Input handler — F8 toggles the UI ────────────────────────────────────────

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

            // F8 = 0x42 (DIK scancode) — change here or expose via INI
            if (btn->GetIDCode() == 0x42) {
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
        Bridge::get()->onDataLoaded();
        break;
    }
}

static void onSlaveTatsMessage(SKSE::MessagingInterface::Message* msg) {
    if (msg->type != slavetats::interface::Interface) return;

    auto* api = slavetats::interface::Addresses::from_void(msg->data);
    if (!api) {
        logger::error("SlaveTatsUI: SlaveTatsNG interface version mismatch");
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

    try {
        auto logPath = setupLog();
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath.string(), true);
        auto log  = std::make_shared<spdlog::logger>("SlaveTatsUI", std::move(sink));
        log->set_level(spdlog::level::debug);
        log->flush_on(spdlog::level::debug);
        spdlog::set_default_logger(std::move(log));
    } catch (...) {}

    auto* msg = SKSE::GetMessagingInterface();
    if (!msg) {
        logger::critical("SlaveTatsUI: messaging interface unavailable");
        return false;
    }

    msg->RegisterListener("SKSE",          onSKSEMessage);
    msg->RegisterListener("SlaveTatsNG",   onSlaveTatsMessage);
    msg->RegisterListener("JContainers64", onJContainersMessage);

    logger::info("SlaveTatsUI: loaded — press F8 in-game to toggle UI");
    return true;
}
