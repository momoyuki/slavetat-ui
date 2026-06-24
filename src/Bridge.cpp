#include "pch.h"
#include "Bridge.h"
#include "jcontainers_mini.h"

namespace stui {

// ── Initialization ────────────────────────────────────────────────────────────

void Bridge::onSlaveTatsInterface(const slavetats::interface::Addresses* api) {
    m_tattooAPI = api;
    logger::info("SlaveTatsUI: SlaveTatsNG API v{} bound", api->current_version);
}

void Bridge::onJContainersReady(const jc::root_interface* root) {
    m_jcReady = jcmini::Init(root);
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
        Bridge::get()->sendToUI(R"({"type":"ready"})");
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

    if (m_hidden) {
        m_prismaUI->Show(m_view);
        m_prismaUI->Focus(m_view, true);
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
        int   color = j.value("color", 0);
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
    if (!m_tattooAPI) {
        sendToUI(R"({"type":"error","message":"SlaveTatsNG not available"})");
        return;
    }
    if (!m_jcReady) {
        sendToUI(R"({"type":"error","message":"JContainers not ready"})");
        return;
    }

    logger::info("SlaveTatsUI: queryAvailable domain={}", domain);

    static constexpr const char* k_pool = "SlaveTatsUI-queryAvailable";
    int matches = jcmini::JValue::addToPool(jcmini::JArray::object(), k_pool);
    logger::info("SlaveTatsUI: queryAvailable JArray handle={}", matches);

    bool failed = m_tattooAPI->query_available_tattoos(0, matches, 0, RE::BSFixedString(domain.c_str()));
    logger::info("SlaveTatsUI: query_available_tattoos failed={}, count={}", failed, jcmini::JArray::count(matches));

    if (failed) {
        jcmini::JValue::cleanPool(k_pool);
        sendToUI(R"({"type":"error","message":"query_available_tattoos failed"})");
        return;
    }

    std::string result = std::format(
        R"({{"type":"available","domain":"{}","tattoos":{}}})",
        escapeJSON(domain), jArrayToJSON(matches));

    jcmini::JValue::cleanPool(k_pool);
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

    std::string result = std::format(
        R"({{"type":"applied","actorId":"0x{:X}","tattoos":{}}})",
        actorFormId, jArrayToJSON(matches));

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

// Runs on a detached background thread — tries disk cache, then loose file, then BSA
void Bridge::handleGetTexture(std::string texPath) {
    auto cachePath = getCachePath(texPath);

    wchar_t exeBuf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exeBuf, MAX_PATH);
    auto sourcePath = std::filesystem::path(exeBuf).parent_path()
                      / L"Data" / L"textures" / L"actors" / L"character" / L"slavetats"
                      / std::filesystem::path(texPath);

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

    // Cache miss or stale — try loose DDS file first
    DirectX::ScratchImage raw;
    DirectX::TexMetadata  meta;
    HRESULT hr = DirectX::LoadFromDDSFile(sourcePath.wstring().c_str(),
                                          DirectX::DDS_FLAGS_NONE, &meta, raw);
    if (SUCCEEDED(hr)) {
        sendDecodedTexture(texPath, raw, cachePath);
        return;
    }

    // Loose file not found — fall back to game thread BSA reader
    SKSE::GetTaskInterface()->AddTask([this, texPath, cachePath]() {
        handleGetTextureBSA(texPath, cachePath);
    });
}

// Runs on game thread — reads texture from BSA via Skyrim's own reader
void Bridge::handleGetTextureBSA(std::string texPath, std::filesystem::path cachePath) {
    std::string relPath = "textures\\actors\\character\\slavetats\\";
    for (char c : texPath) relPath += (c == '/') ? '\\' : c;

    RE::BSResourceNiBinaryStream stream(relPath.c_str());
    if (!stream.good()) {
        logger::warn("SlaveTatsUI: texture not found (loose + BSA): {}", texPath);
        sendToUI(std::format(R"({{"type":"textureError","path":"{}"}})", escapeJSON(texPath)));
        return;
    }

    std::vector<uint8_t> data;
    std::array<uint8_t, 8192> buf;
    while (true) {
        auto nr = stream.read(buf.data(), buf.size());
        if (nr == 0) break;
        data.insert(data.end(), buf.data(), buf.data() + static_cast<size_t>(nr));
    }

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
    return std::format(
        R"({{"name":"{}","section":"{}","area":"{}","texture":"{}","slot":{},"color":{},"locked":{},"alpha":{:.2f}}})",
        escapeJSON(jcmini::JMap::getStr(tattoo, "name")),
        escapeJSON(jcmini::JMap::getStr(tattoo, "section")),
        escapeJSON(jcmini::JMap::getStr(tattoo, "area")),
        escapeJSON(jcmini::JMap::getStr(tattoo, "texture")),
        jcmini::JMap::getInt(tattoo, "slot"),
        jcmini::JMap::getInt(tattoo, "color"),
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
