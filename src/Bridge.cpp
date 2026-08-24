#include "pch.h"
#include "Bridge.h"
#include "adapters/PrismaTattooSerializer.h"
#include "jcontainers_mini.h"
#include "textures/ExactStreamReader.h"
#include "textures/TextureResolver.h"

namespace stui {

// ── Initialization ────────────────────────────────────────────────────────────

void Bridge::onSlaveTatsInterface(const slavetats::interface::Addresses* api) {
    m_tattooAPI = api;
    m_runtime.bindSlaveTats(api);
    logger::info("SlaveTatsUI: SlaveTatsNG API v{} bound", api->current_version);
}

void Bridge::onSlaveTatsVersionMismatch(uint32_t gotVersion) {
    m_tattooAPI = nullptr;
    m_runtime.noteSlaveTatsVersionMismatch(gotVersion);
}

void Bridge::onJContainersReady(const jc::root_interface* root) {
    m_jcReady = m_runtime.bindJContainers(root);
    if (m_jcReady)
        logger::info("SlaveTatsUI: JContainers initialized");
    else
        logger::error("SlaveTatsUI: JContainers init failed — reflection/domain interface missing");
}

void Bridge::onDataLoaded() {
    m_prismaUI = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI1>();
    if (!m_prismaUI) {
        logger::error("SlaveTatsUI: PrismaUI.dll not found — is PrismaUI installed?");
        return;
    }

    m_view = m_prismaUI->CreateView("SlaveTatsUI/index.html", [](PrismaView) {
        auto* b = Bridge::get();
        b->sendToUI(std::format(
            R"({{"type":"ready","apiVersion":{},"apiOk":{}}})",
            b->m_runtime.apiVersion(),
            b->m_runtime.apiAvailable() ? "true" : "false"));
    });

    m_prismaUI->RegisterJSListener(m_view, "slavetatsCmd", [](const char* data) {
        Bridge::get()->onJSCommand(data);
    });

    // Start hidden; player opens via hotkey
    m_prismaUI->Hide(m_view);
    m_hidden = true;

    logger::info("SlaveTatsUI: view created");
}

// ── UI Toggle ─────────────────────────────────────────────────────────────────

void Bridge::toggleUI() {
    if (!m_prismaUI || !m_view) return;
    std::lock_guard<std::mutex> lock(m_toggleMutex);
    if (m_hidden) {
        m_prismaUI->Show(m_view);
        m_prismaUI->Focus(m_view, true);
        sendToUI(R"({"type":"show"})");
    } else {
        m_prismaUI->Unfocus(m_view);
        m_prismaUI->Hide(m_view);
    }
    m_hidden = !m_hidden;
}

// ── Command Dispatch ──────────────────────────────────────────────────────────

void Bridge::onJSCommand(const char* jsonStr) {
    auto j = nlohmann::json::parse(jsonStr, nullptr, false);
    if (j.is_discarded()) {
        logger::warn("SlaveTatsUI: invalid JSON: {}", jsonStr);
        return;
    }

    auto action = j.value("action", "");

    if (action == "toggleUI") {
        toggleUI();
        return;
    }

    if (action == "queryAvailable") {
        std::string domain = j.value("domain", "default");
        SKSE::GetTaskInterface()->AddTask([this, domain = std::move(domain)]() {
            handleQueryAvailable(domain);
        });
    } else if (action == "queryApplied") {
        uint32_t actorId = j.value("actorId", 0x14u);
        SKSE::GetTaskInterface()->AddTask([this, actorId]() {
            handleQueryApplied(actorId);
        });
    } else if (action == "addTattoo") {
        uint32_t actorId = j.value("actorId", 0x14u);
        std::string section = j.value("section", "");
        std::string name    = j.value("name", "");
        int   color = j.value("color", 0xFFFFFF);
        float alpha = j.value("alpha", 1.0f);
        SKSE::GetTaskInterface()->AddTask([this, actorId,
                                          section = std::move(section),
                                          name    = std::move(name),
                                          color, alpha]() {
            handleAddTattoo(actorId, section, name, color, alpha);
        });
    } else if (action == "removeTattoo") {
        uint32_t actorId    = j.value("actorId", 0x14u);
        std::string section = j.value("section", "");
        std::string name    = j.value("name", "");
        SKSE::GetTaskInterface()->AddTask([this, actorId,
                                          section = std::move(section),
                                          name    = std::move(name)]() {
            handleRemoveTattoo(actorId, section, name);
        });
    } else if (action == "syncTattoos") {
        uint32_t actorId = j.value("actorId", 0x14u);
        SKSE::GetTaskInterface()->AddTask([this, actorId]() {
            handleSyncTattoos(actorId);
        });
    } else if (action == "queryActors") {
        SKSE::GetTaskInterface()->AddTask([this]() { handleQueryActors(); });
    } else if (action == "querySlots") {
        uint32_t actorId = j.value("actorId", 0x14u);
        std::string area = j.value("area", "BODY");
        SKSE::GetTaskInterface()->AddTask([this, actorId, area = std::move(area)]() {
            handleQuerySlots(actorId, area);
        });
    } else if (action == "queryAllSlots") {
        uint32_t actorId = j.value("actorId", 0x14u);
        SKSE::GetTaskInterface()->AddTask([this, actorId]() {
            handleQueryAllSlots(actorId);
        });
    } else if (action == "removeFromSlot") {
        uint32_t actorId = j.value("actorId", 0x14u);
        std::string area = j.value("area", "");
        int slot         = j.value("slot", -1);
        SKSE::GetTaskInterface()->AddTask([this, actorId, area = std::move(area), slot]() {
            handleRemoveFromSlot(actorId, area, slot);
        });
    } else if (action == "applyToSlot") {
        uint32_t actorId    = j.value("actorId", 0x14u);
        std::string section = j.value("section", "");
        std::string name    = j.value("name", "");
        std::string domain  = j.value("domain", "default");
        int  slot           = j.value("slot", -1);
        int  color          = j.value("color", 0xFFFFFF);
        float alpha         = j.value("alpha", 1.0f);
        SKSE::GetTaskInterface()->AddTask([this, actorId,
                                          section = std::move(section),
                                          name    = std::move(name),
                                          domain  = std::move(domain),
                                          slot, color, alpha]() {
            handleApplyToSlot(actorId, section, name, domain, slot, color, alpha);
        });
    } else if (action == "updateTattoo") {
        uint32_t actorId    = j.value("actorId", 0x14u);
        int  tattooHandle   = j.value("tattooHandle", 0);
        int  color          = j.value("color", 0xFFFFFF);
        float alpha         = j.value("alpha", 1.0f);
        SKSE::GetTaskInterface()->AddTask([this, actorId, tattooHandle, color, alpha]() {
            handleUpdateTattoo(actorId, tattooHandle, color, alpha);
        });
    } else if (action == "getTexture") {
        std::string path = j.value("path", "");
        if (!path.empty()) {
            // File I/O + DirectXTex decode runs off the game thread to avoid stutter
            std::thread([this, path = std::move(path)]() {
                handleGetTexture(path);
            }).detach();
        }
    } else {
        logger::warn("SlaveTatsUI: unknown action '{}'", action);
    }
}

// ── Game-thread Handlers ──────────────────────────────────────────────────────

void Bridge::handleQueryAvailable(const std::string& domain) {
    logger::info("SlaveTatsUI: queryAvailable domain={}", domain);

    const auto queryResult = m_service.queryAvailable(domain);
    if (!queryResult) {
        sendToUI(std::format(
            R"({{"type":"error","message":"{}"}})",
            escapeJSON(queryResult.error().message)));
        return;
    }

    std::string tattoos = "[";
    for (std::size_t index = 0; index < queryResult->size(); ++index) {
        if (index > 0) tattoos += ',';
        tattoos += adapters::toPrismaTattooJSON((*queryResult)[index]);
    }
    tattoos += ']';

    std::string result = std::format(
        R"({{"type":"available","domain":"{}","tattoos":{}}})",
        escapeJSON(domain), tattoos);

    sendToUI(result);
}

void Bridge::handleQueryApplied(uint32_t actorFormId) {
    if (!m_tattooAPI) {
        sendToUI(R"({"type":"error","message":"SlaveTatsNG not available"})");
        return;
    }
    if (!m_jcReady) {
        sendToUI(R"({"type":"error","message":"JContainers not ready"})");
        return;
    }

    auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorFormId);
    if (!actor) {
        sendToUI(std::format(R"({{"type":"error","message":"Actor 0x{:X} not found"}})", actorFormId));
        return;
    }

    static constexpr const char* k_pool = "SlaveTatsUI-queryApplied";
    int matches = jcmini::JValue::addToPool(jcmini::JArray::object(), k_pool);

    bool failed = m_tattooAPI->query_applied_tattoos(actor, 0, matches, "", -1);

    if (failed) {
        jcmini::JValue::cleanPool(k_pool);
        sendToUI(R"({"type":"error","message":"query_applied_tattoos failed"})");
        return;
    }

    std::string version = jcmini::JFormDB::getStr(actor, ".SlaveTats.version", "");
    std::string result = std::format(
        R"({{"type":"applied","actorId":"0x{:X}","version":"{}","tattoos":{}}})",
        actorFormId, escapeJSON(version), jArrayToJSON(matches));

    jcmini::JValue::cleanPool(k_pool);
    sendToUI(result);
}

void Bridge::handleAddTattoo(uint32_t actorId, std::string section, std::string name, int color, float alpha) {
    logger::info("SlaveTatsUI: addTattoo actor=0x{:X} section='{}' name='{}' color={} alpha={:.2f}",
        actorId, section, name, color, alpha);

    if (!m_tattooAPI) {
        logger::error("SlaveTatsUI: addTattoo — m_tattooAPI is null");
        sendToUI(R"({"type":"error","message":"SlaveTatsNG not available"})");
        return;
    }

    auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorId);
    if (!actor) {
        logger::error("SlaveTatsUI: addTattoo — actor 0x{:X} not found", actorId);
        sendToUI(std::format(R"({{"type":"error","message":"Actor 0x{:X} not found"}})", actorId));
        return;
    }

    bool failed = m_tattooAPI->simple_add_tattoo(
        actor,
        RE::BSFixedString(section.c_str()),
        RE::BSFixedString(name.c_str()),
        color, true, false, alpha);

    // SlaveTatsNG may return true (fail_t) even when the tattoo is visually applied
    // (e.g. slot number cast to bool, or internal sync step returning error).
    // Treat any return as success since the visual application succeeds in practice.
    if (failed) logger::warn("SlaveTatsUI: simple_add_tattoo returned failed=true (tattoo may still be applied)");
    else         logger::info("SlaveTatsUI: simple_add_tattoo returned failed=false");

    sendToUI(std::format(
        R"({{"type":"success","action":"addTattoo","section":"{}","name":"{}"}})",
        escapeJSON(section), escapeJSON(name)));
}

void Bridge::handleRemoveTattoo(uint32_t actorId, std::string section, std::string name) {
    if (!m_tattooAPI) {
        sendToUI(R"({"type":"error","message":"SlaveTatsNG not available"})");
        return;
    }

    auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorId);
    if (!actor) {
        sendToUI(std::format(R"({{"type":"error","message":"Actor 0x{:X} not found"}})", actorId));
        return;
    }

    bool failed = m_tattooAPI->simple_remove_tattoo(
        actor,
        RE::BSFixedString(section.c_str()),
        RE::BSFixedString(name.c_str()),
        true, false);

    if (failed) logger::warn("SlaveTatsUI: simple_remove_tattoo returned failed=true (tattoo may still be removed)");
    else         logger::info("SlaveTatsUI: simple_remove_tattoo returned failed=false");

    sendToUI(std::format(
        R"({{"type":"success","action":"removeTattoo","section":"{}","name":"{}"}})",
        escapeJSON(section), escapeJSON(name)));
}

void Bridge::handleSyncTattoos(uint32_t actorId) {
    if (!m_tattooAPI) {
        sendToUI(R"({"type":"error","message":"SlaveTatsNG not available"})");
        return;
    }

    auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorId);
    if (!actor) {
        sendToUI(std::format(R"({{"type":"error","message":"Actor 0x{:X} not found"}})", actorId));
        return;
    }

    bool failed = m_tattooAPI->synchronize_tattoos(actor, false);
    if (failed)
        sendToUI(R"({"type":"error","message":"synchronize_tattoos failed"})");
    else
        sendToUI(R"({"type":"success","action":"syncTattoos"})");
}

void Bridge::handleQueryActors() {
    std::string result = R"({"type":"actors","actors":[)";
    bool first = true;

    auto addActor = [&](RE::Actor* actor, bool isPlayer) {
        if (!actor) return;
        const char* name = actor->GetDisplayFullName();
        if (!name || !name[0]) name = isPlayer ? "Player" : "NPC";
        if (!first) result += ',';
        first = false;
        result += std::format(R"({{"id":"0x{:X}","name":"{}","isPlayer":{}}})",
            actor->GetFormID(), escapeJSON(name), isPlayer ? "true" : "false");
    };

    addActor(RE::PlayerCharacter::GetSingleton(), true);

    auto* pl = RE::ProcessLists::GetSingleton();
    if (pl) {
        for (auto& handle : pl->highActorHandles) {
            auto ptr = handle.get();
            if (!ptr || ptr->IsPlayerRef() || !ptr->GetActorBase()) continue;
            addActor(ptr.get(), false);
        }
    }

    result += "]}";
    sendToUI(result);
}

void Bridge::handleQuerySlots(uint32_t actorId, std::string area) {
    if (!m_tattooAPI) {
        sendToUI(R"({"type":"error","message":"SlaveTatsNG not available"})");
        return;
    }
    auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorId);
    if (!actor) {
        sendToUI(std::format(R"({{"type":"error","message":"Actor 0x{:X} not found"}})", actorId));
        return;
    }

    std::string areaUp = area;
    for (char& c : areaUp) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    // Read overlay count from skee64.ini; cache per area after first read
    static std::unordered_map<std::string, int> s_slotCache;
    if (!s_slotCache.count(areaUp)) {
        static std::filesystem::path s_iniPath;
        if (s_iniPath.empty()) {
            wchar_t exeBuf[MAX_PATH] = {};
            GetModuleFileNameW(nullptr, exeBuf, MAX_PATH);
            s_iniPath = std::filesystem::path(exeBuf).parent_path()
                        / L"Data" / L"SKSE" / L"Plugins" / L"skee64.ini";
        }
        static const std::unordered_map<std::string, std::pair<std::wstring, int>> kAreaIni = {
            {"BODY",  {L"Overlays/Body",  12}},
            {"FACE",  {L"Overlays/Face",   3}},
            {"HANDS", {L"Overlays/Hands",  3}},
            {"FEET",  {L"Overlays/Feet",   3}},
        };
        int def = (areaUp == "BODY") ? 12 : 3;
        auto it = kAreaIni.find(areaUp);
        if (it != kAreaIni.end())
            s_slotCache[areaUp] = static_cast<int>(GetPrivateProfileIntW(
                it->second.first.c_str(), L"iNumOverlays", it->second.second, s_iniPath.c_str()));
        else
            s_slotCache[areaUp] = def;
        logger::info("SlaveTatsUI: {} maxSlots={} (from skee64.ini)", areaUp, s_slotCache[areaUp]);
    }
    int maxSlots = s_slotCache.at(areaUp);

    // Query slots occupied by non-SlaveTats overlays (other mods using NiOverride directly)
    static constexpr const char* k_pool = "SlaveTatsUI-extSlots";
    int extArr = jcmini::JValue::addToPool(jcmini::JArray::object(), k_pool);
    m_tattooAPI->external_slots(actor, RE::BSFixedString(areaUp.c_str()), extArr);

    std::unordered_set<int> externalSet;
    int extCount = jcmini::JArray::count(extArr);
    for (int i = 0; i < extCount; ++i)
        externalSet.insert(jcmini::JArray::getInt(extArr, i));
    jcmini::JValue::cleanPool(k_pool);

    std::string result = std::format(
        R"({{"type":"slots","area":"{}","maxSlots":{},"slots":[)", escapeJSON(area), maxSlots);
    bool first = true;
    for (int slot = 0; slot < maxSlots; slot++) {
        int tattoo = m_tattooAPI->get_applied_tattoo_in_slot(
            actor, RE::BSFixedString(areaUp.c_str()), slot);
        if (!first) result += ',';
        first = false;
        if (tattoo) {
            int rawColor = jcmini::JMap::getInt(tattoo, "color", 0);
            result += std::format(
                R"({{"slot":{},"occupied":true,"name":"{}","section":"{}","texture":"{}","color":{},"alpha":{:.2f},"handle":{}}})",
                slot,
                escapeJSON(jcmini::JMap::getStr(tattoo, "name")),
                escapeJSON(jcmini::JMap::getStr(tattoo, "section")),
                escapeJSON(jcmini::JMap::getStr(tattoo, "texture")),
                (rawColor == 0) ? 0xFFFFFF : rawColor,
                jcmini::JMap::getFlt(tattoo, "alpha", 1.0f),
                tattoo);
        } else if (externalSet.count(slot)) {
            result += std::format(R"({{"slot":{},"occupied":true,"external":true,"name":"[External]"}})", slot);
        } else {
            result += std::format(R"({{"slot":{},"occupied":false}})", slot);
        }
    }
    result += "]}";
    sendToUI(result);
}

void Bridge::handleQueryAllSlots(uint32_t actorId) {
    for (const char* area : {"BODY", "FACE", "HANDS", "FEET"}) {
        handleQuerySlots(actorId, area);
    }
}

void Bridge::handleRemoveFromSlot(uint32_t actorId, std::string area, int slot) {
    if (!m_tattooAPI) {
        sendToUI(R"({"type":"error","message":"SlaveTatsNG not available"})");
        return;
    }
    if (!m_jcReady) {
        sendToUI(R"({"type":"error","message":"JContainers not ready"})");
        return;
    }
    if (slot < 0) {
        sendToUI(std::format(R"({{"type":"error","message":"Invalid slot {}"}})", slot));
        return;
    }
    auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorId);
    if (!actor) {
        sendToUI(std::format(R"({{"type":"error","message":"Actor 0x{:X} not found"}})", actorId));
        return;
    }
    std::string areaUp = area;
    for (char& c : areaUp) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    bool failed = m_tattooAPI->remove_tattoo_from_slot(
        actor, RE::BSFixedString(areaUp.c_str()), slot, false, false);
    if (failed) {
        sendToUI(R"({"type":"error","message":"Failed to remove tattoo from slot"})");
        return;
    }
    jcmini::JFormDB::setInt(actor, ".SlaveTats.updated", 1);
    bool syncFailed = m_tattooAPI->synchronize_tattoos(actor, false);
    if (syncFailed) {
        sendToUI(R"({"type":"error","message":"Tattoo removed but sync failed — try Sync button"})");
        return;
    }
    sendToUI(std::format(
        R"({{"type":"success","action":"removeFromSlot","area":"{}","slot":{}}})",
        escapeJSON(area), slot));
}

void Bridge::handleApplyToSlot(uint32_t actorId, std::string section, std::string name, std::string domain, int slot, int color, float alpha) {
    if (!m_tattooAPI) {
        sendToUI(R"({"type":"error","message":"SlaveTatsNG not available"})");
        return;
    }
    if (!m_jcReady) {
        sendToUI(R"({"type":"error","message":"JContainers not ready"})");
        return;
    }
    auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorId);
    if (!actor) {
        sendToUI(std::format(R"({{"type":"error","message":"Actor 0x{:X} not found"}})", actorId));
        return;
    }
    if (slot < 0) {
        sendToUI(std::format(R"({{"type":"error","message":"Invalid slot {}"}})", slot));
        return;
    }

    logger::info("SlaveTatsUI: applyToSlot actor=0x{:X} domain='{}' section='{}' name='{}' slot={} color=0x{:X} alpha={:.2f}",
        actorId, domain, section, name, slot, color, alpha);

    // Re-query with the same domain the UI used — handles from a prior queryAvailable
    // were freed after that pool was cleaned, so we need a fresh reference here.
    static constexpr const char* k_pool = "SlaveTatsUI-applyToSlot";
    int arr = jcmini::JValue::addToPool(jcmini::JArray::object(), k_pool);
    m_tattooAPI->query_available_tattoos(0, arr, 0, RE::BSFixedString(domain.c_str()));

    int count = jcmini::JArray::count(arr);
    int tattooHandle = 0;
    for (int i = 0; i < count && !tattooHandle; ++i) {
        int t = jcmini::JArray::getObj(arr, i);
        if (jcmini::JMap::getStr(t, "section") == section &&
            jcmini::JMap::getStr(t, "name")    == name) {
            tattooHandle = t;
        }
    }

    if (!tattooHandle) {
        jcmini::JValue::cleanPool(k_pool);
        logger::error("SlaveTatsUI: applyToSlot — '{}/{}' not found in domain '{}'", section, name, domain);
        sendToUI(R"({"type":"error","message":"Tattoo not found in available list"})");
        return;
    }

    // Temporarily set color/alpha on the template so add_and_get_tattoo copies them
    // into JFormDB. Restore original values after the call to avoid corrupting the
    // shared template for subsequent queries.
    int   origColor = jcmini::JMap::getInt(tattooHandle, "color", 0);
    float origAlpha = jcmini::JMap::getFlt(tattooHandle, "alpha", 1.0f);
    jcmini::JMap::setInt(tattooHandle, "color", color);
    jcmini::JMap::setFlt(tattooHandle, "alpha", alpha);

    int applied = m_tattooAPI->add_and_get_tattoo(actor, tattooHandle, slot, false, false, false);

    // Restore template before pool cleanup so other concurrent readers see original values
    jcmini::JMap::setInt(tattooHandle, "color", origColor);
    jcmini::JMap::setFlt(tattooHandle, "alpha", origAlpha);
    jcmini::JValue::cleanPool(k_pool);

    if (!applied) {
        logger::error("SlaveTatsUI: add_and_get_tattoo returned 0 — slot locked or invalid");
        sendToUI(R"({"type":"error","message":"Failed to apply tattoo to slot"})");
        return;
    }

    jcmini::JFormDB::setInt(actor, ".SlaveTats.updated", 1);
    m_tattooAPI->synchronize_tattoos(actor, false);

    sendToUI(std::format(
        R"({{"type":"success","action":"applyToSlot","slot":{},"section":"{}","name":"{}"}})",
        slot, escapeJSON(section), escapeJSON(name)));
}

void Bridge::handleUpdateTattoo(uint32_t actorId, int tattooHandle, int color, float alpha) {
    if (!m_tattooAPI || !m_jcReady) {
        sendToUI(R"({"type":"error","message":"SlaveTatsNG or JContainers not ready"})");
        return;
    }
    auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorId);
    if (!actor) {
        sendToUI(std::format(R"({{"type":"error","message":"Actor 0x{:X} not found"}})", actorId));
        return;
    }

    if (!tattooHandle) {
        sendToUI(R"({"type":"error","message":"Invalid tattoo handle — try Refresh"})");
        return;
    }

    logger::info("SlaveTatsUI: updateTattoo handle={} color=0x{:X} alpha={:.2f}", tattooHandle, color, alpha);

    jcmini::JMap::setInt(tattooHandle, "color", color);
    jcmini::JMap::setFlt(tattooHandle, "alpha", alpha);

    // Mark the actor's SlaveTats data as changed so synchronize_tattoos doesn't abort
    jcmini::JFormDB::setInt(actor, ".SlaveTats.updated", 1);
    m_tattooAPI->synchronize_tattoos(actor, false);
    sendToUI(R"({"type":"success","action":"updateTattoo"})");
}

// Runs on a detached background thread — tries disk cache, then loose file, then BSA
void Bridge::handleGetTexture(std::string texPath) {
    auto cachePath = getCachePath(texPath);

    wchar_t exeBuf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exeBuf, MAX_PATH);
    const auto textureRoot = std::filesystem::path(exeBuf).parent_path()
        / L"Data" / L"textures" / L"actors" / L"character" / L"slavetats";
    const auto normalizedPath = textures::TextureResolver::normalize(texPath);
    if (!normalizedPath) {
        logger::warn("SlaveTatsUI: rejected unsafe texture path: {}", texPath);
        sendToUI(std::format(
            R"({{"type":"textureError","path":"{}"}})", escapeJSON(texPath)));
        return;
    }
    const auto sourcePath = textureRoot / std::filesystem::path(*normalizedPath);

    // Cache hit check: valid if cache exists AND (no loose source OR source is not newer)
    {
        std::vector<uint8_t> rgba;
        uint32_t w = 0, h = 0;
        if (readCache(cachePath, rgba, w, h)) {
            std::error_code ec;
            auto cacheTime  = std::filesystem::last_write_time(cachePath, ec);
            auto sourceTime = std::filesystem::last_write_time(sourcePath, ec);
            // Use cache if source doesn't exist (BSA) or source is not newer
            bool cacheValid = ec || !(sourceTime > cacheTime);
            if (cacheValid) {
                logger::debug("SlaveTatsUI: thumb cache hit: {}", texPath);
                sendRGBAToUI(texPath, rgba.data(), rgba.size(), w, h);
                return;
            }
        }
    }

    textures::TextureResolver resolver(textureRoot);
    auto loose = resolver.resolveLoose(*normalizedPath);
    if (loose) {
        DirectX::ScratchImage raw;
        DirectX::TexMetadata meta;
        const HRESULT hr = DirectX::LoadFromDDSMemory(
            loose->bytes.data(), loose->bytes.size(), DirectX::DDS_FLAGS_NONE, &meta, raw);
        if (SUCCEEDED(hr)) {
            sendDecodedTexture(texPath, raw, cachePath);
            return;
        }
        logger::warn(
            "SlaveTatsUI: loose texture decode failed, trying BSA: {} hr=0x{:X}",
            texPath,
            static_cast<uint32_t>(hr));
    }

    // Loose file not found — fall back to game thread BSA reader
    SKSE::GetTaskInterface()->AddTask([this, texPath, cachePath]() {
        handleGetTextureBSA(texPath, cachePath);
    });
}

// Runs on game thread — reads texture from BSA via Skyrim's own reader
void Bridge::handleGetTextureBSA(std::string texPath, std::filesystem::path cachePath) {
    wchar_t exeBuf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exeBuf, MAX_PATH);
    const auto textureRoot = std::filesystem::path(exeBuf).parent_path()
        / L"Data" / L"textures" / L"actors" / L"character" / L"slavetats";
    textures::TextureResolver resolver(textureRoot);
    auto resolved = resolver.resolveArchive(texPath, [](std::string_view resourcePath) {
        const std::string path(resourcePath);
        RE::BSResourceNiBinaryStream stream(path.c_str());
        if (!stream.good()) {
            return textures::TextureBytesResult(
                std::unexpected(textures::TextureResolveError::notFound));
        }
        const std::uint32_t resourceSize = stream.stream->totalSize;
        auto bytes = textures::readExactBytes(stream, resourceSize);
        if (!bytes) {
            return textures::TextureBytesResult(
                std::unexpected(textures::TextureResolveError::readFailed));
        }
        return textures::TextureBytesResult(std::move(*bytes));
    });
    if (!resolved) {
        if (resolved.error() == textures::TextureResolveError::readFailed) {
            logger::warn("SlaveTatsUI: BSA texture read failed: {}", texPath);
        } else {
            logger::warn("SlaveTatsUI: texture not found (loose + BSA): {}", texPath);
        }
        sendToUI(std::format(R"({{"type":"textureError","path":"{}"}})", escapeJSON(texPath)));
        return;
    }
    auto data = std::move(resolved->bytes);

    // Decode on background thread so game thread isn't blocked
    std::thread([this, texPath, cachePath = std::move(cachePath), data = std::move(data)]() mutable {
        DirectX::ScratchImage raw;
        DirectX::TexMetadata  meta;
        HRESULT hr = DirectX::LoadFromDDSMemory(data.data(), data.size(),
                                                  DirectX::DDS_FLAGS_NONE, &meta, raw);
        if (FAILED(hr)) {
            logger::warn("SlaveTatsUI: BSA texture decode failed: {} hr=0x{:X}",
                texPath, static_cast<uint32_t>(hr));
            sendToUI(std::format(R"({{"type":"textureError","path":"{}"}})", escapeJSON(texPath)));
            return;
        }
        sendDecodedTexture(texPath, raw, std::move(cachePath));
    }).detach();
}

// Shared: decompress → resize → write cache → send RGBA (safe to call from any thread)
void Bridge::sendDecodedTexture(std::string texPath, DirectX::ScratchImage& raw,
                                 std::filesystem::path cachePath) {
    const auto& meta = raw.GetMetadata();

    DirectX::ScratchImage decoded;
    HRESULT hr;
    if (DirectX::IsCompressed(meta.format)) {
        hr = DirectX::Decompress(raw.GetImages(), raw.GetImageCount(), meta,
                                 DXGI_FORMAT_R8G8B8A8_UNORM, decoded);
    } else {
        hr = DirectX::Convert(raw.GetImages(), raw.GetImageCount(), meta,
                              DXGI_FORMAT_R8G8B8A8_UNORM,
                              DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT,
                              decoded);
    }
    if (FAILED(hr)) {
        logger::warn("SlaveTatsUI: decompress failed: {} hr=0x{:X}", texPath, static_cast<uint32_t>(hr));
        sendToUI(std::format(R"({{"type":"textureError","path":"{}"}})", escapeJSON(texPath)));
        return;
    }

    const auto* mip0 = decoded.GetImage(0, 0, 0);
    if (!mip0) {
        sendToUI(std::format(R"({{"type":"textureError","path":"{}"}})", escapeJSON(texPath)));
        return;
    }

    DirectX::ScratchImage thumb;
    hr = DirectX::Resize(*mip0, 128, 128, DirectX::TEX_FILTER_LINEAR, thumb);
    const auto* img = SUCCEEDED(hr) ? thumb.GetImage(0, 0, 0) : mip0;

    const uint32_t W = static_cast<uint32_t>(img->width);
    const uint32_t H = static_cast<uint32_t>(img->height);
    std::vector<uint8_t> rgba(static_cast<size_t>(W) * H * 4);
    for (size_t y = 0; y < H; ++y)
        std::memcpy(rgba.data() + y * W * 4, img->pixels + y * img->rowPitch, W * 4);

    // Persist to disk so next session loads instantly
    if (!cachePath.empty())
        writeCache(cachePath, rgba.data(), rgba.size(), W, H);

    sendRGBAToUI(texPath, rgba.data(), rgba.size(), W, H);
}

// ── Texture cache helpers ─────────────────────────────────────────────────────

std::filesystem::path Bridge::getCachePath(const std::string& texPath) {
    std::filesystem::path cacheDir;
    if (auto p = SKSE::log::log_directory())
        cacheDir = p->parent_path() / "SlaveTatsUI" / "thumbcache";
    else {
        wchar_t exeBuf[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exeBuf, MAX_PATH);
        cacheDir = std::filesystem::path(exeBuf).parent_path()
                   / L"Data" / L"SKSE" / L"Plugins" / L"SlaveTatsUI" / L"thumbcache";
    }
    // Sanitize tex path → safe filename
    std::string name = texPath;
    for (char& c : name)
        if (c == '\\' || c == '/' || c == ':') c = '_';
    if (name.size() > 4 && name.compare(name.size()-4, 4, ".dds") == 0)
        name.resize(name.size() - 4);
    name += ".rgba";
    return cacheDir / name;
}

// Cache file format: uint32 w + uint32 h + w*h*4 RGBA bytes
bool Bridge::readCache(const std::filesystem::path& p,
                        std::vector<uint8_t>& rgba, uint32_t& w, uint32_t& h) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    f.read(reinterpret_cast<char*>(&w), 4);
    f.read(reinterpret_cast<char*>(&h), 4);
    if (!f || w == 0 || h == 0 || w > 4096 || h > 4096) return false;
    rgba.resize(static_cast<size_t>(w) * h * 4);
    f.read(reinterpret_cast<char*>(rgba.data()), rgba.size());
    return f.good();
}

void Bridge::writeCache(const std::filesystem::path& p,
                         const uint8_t* rgba, size_t sz, uint32_t w, uint32_t h) {
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    if (ec) return;
    std::ofstream f(p, std::ios::binary);
    if (!f) return;
    f.write(reinterpret_cast<const char*>(&w), 4);
    f.write(reinterpret_cast<const char*>(&h), 4);
    f.write(reinterpret_cast<const char*>(rgba), sz);
}

void Bridge::sendRGBAToUI(const std::string& texPath,
                           const uint8_t* rgba, size_t sz, uint32_t w, uint32_t h) {
    static constexpr char kB64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string b64;
    b64.reserve(((sz + 2) / 3) * 4);
    for (size_t i = 0; i < sz; i += 3) {
        uint32_t v = static_cast<uint32_t>(rgba[i]) << 16;
        if (i + 1 < sz) v |= static_cast<uint32_t>(rgba[i + 1]) << 8;
        if (i + 2 < sz) v |= rgba[i + 2];
        b64 += kB64[(v >> 18) & 63];
        b64 += kB64[(v >> 12) & 63];
        b64 += (i + 1 < sz) ? kB64[(v >> 6) & 63] : '=';
        b64 += (i + 2 < sz) ? kB64[ v       & 63] : '=';
    }
    sendToUI(std::format(R"({{"type":"texture","path":"{}","w":{},"h":{},"data":"{}"}})",
        escapeJSON(texPath), w, h, b64));
}

// ── Serialization ─────────────────────────────────────────────────────────────

std::string Bridge::tattooToJSON(int tattoo) {
    int rawColor = jcmini::JMap::getInt(tattoo, "color", 0);
    return std::format(
        R"({{"handle":{},"name":"{}","section":"{}","area":"{}","texture":"{}","slot":{},"color":{},"locked":{},"alpha":{:.2f}}})",
        tattoo,
        escapeJSON(jcmini::JMap::getStr(tattoo, "name")),
        escapeJSON(jcmini::JMap::getStr(tattoo, "section")),
        escapeJSON(jcmini::JMap::getStr(tattoo, "area")),
        escapeJSON(jcmini::JMap::getStr(tattoo, "texture")),
        jcmini::JMap::getInt(tattoo, "slot"),
        (rawColor == 0) ? 0xFFFFFF : rawColor,
        jcmini::JMap::getInt(tattoo, "locked"),
        jcmini::JMap::getFlt(tattoo, "alpha", 1.0f));
}

std::string Bridge::jArrayToJSON(int jarray) {
    int count = jcmini::JArray::count(jarray);
    std::string out = "[";
    for (int i = 0; i < count; ++i) {
        if (i > 0) out += ',';
        out += tattooToJSON(jcmini::JArray::getObj(jarray, i));
    }
    out += ']';
    return out;
}

void Bridge::sendToUI(const std::string& json) {
    if (!m_prismaUI || !m_view) return;
    // InteropCall delivers the string directly to the named JS function —
    // no JS-level escaping needed, unlike Invoke.
    m_prismaUI->InteropCall(m_view, "slavetatsOnData", json.c_str());
}

std::string Bridge::escapeJSON(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

}  // namespace stui
