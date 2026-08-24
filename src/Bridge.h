#pragma once

#include "PrismaUI_API.h"
#include "core/SlaveTatsService.h"
#include "runtime/SlaveTatsRuntime.h"
#include "SlaveTatsNG_Interface.h"
#include "JContainers/jc_interface.h"
#include <DirectXTex.h>
#ifdef interface
#  undef interface
#endif

namespace stui {

class Bridge {
public:
    static Bridge* get() {
        static Bridge inst;
        return &inst;
    }

    // Call these from SKSE message handlers, in order
    void onSlaveTatsInterface(const slavetats::interface::Addresses* api);
    void onSlaveTatsVersionMismatch(uint32_t gotVersion);
    void onJContainersReady(const jc::root_interface* root);
    void onDataLoaded();  // init PrismaUI, create view

    void toggleUI();

private:
    Bridge() = default;

    // JS → C++ command dispatcher (called from PrismaUI callback thread)
    void onJSCommand(const char* jsonStr);

    // Game-thread handlers (dispatched via SKSE task interface)
    void handleQueryAvailable(const std::string& domain);
    void handleQueryApplied(uint32_t actorFormId);
    void handleAddTattoo(uint32_t actorId, std::string section, std::string name, int color, float alpha);
    void handleRemoveTattoo(uint32_t actorId, std::string section, std::string name);
    void handleSyncTattoos(uint32_t actorId);
    void handleQueryActors();
    void handleQuerySlots(uint32_t actorId, std::string area);
    void handleQueryAllSlots(uint32_t actorId);
    void handleApplyToSlot(uint32_t actorId, std::string section, std::string name, std::string domain, int slot, int color, float alpha);
    void handleRemoveFromSlot(uint32_t actorId, std::string area, int slot);
    void handleUpdateTattoo(uint32_t actorId, int tattooHandle, int color, float alpha);
    void handleGetTexture(std::string texPath);
    void handleGetTextureBSA(std::string texPath, std::filesystem::path cachePath);
    void sendDecodedTexture(std::string texPath, DirectX::ScratchImage& raw,
                             std::filesystem::path cachePath = {});

    // Helpers
    void sendToUI(const std::string& json);
    void sendRGBAToUI(const std::string& texPath, const uint8_t* rgba, size_t sz, uint32_t w, uint32_t h);
    std::string tattooToJSON(int tattoo);
    std::string jArrayToJSON(int jarray);
    static std::string escapeJSON(std::string_view s);
    static std::filesystem::path getCachePath(const std::string& texPath);
    static bool readCache(const std::filesystem::path& p, std::vector<uint8_t>& rgba, uint32_t& w, uint32_t& h);
    static void writeCache(const std::filesystem::path& p, const uint8_t* rgba, size_t sz, uint32_t w, uint32_t h);

    runtime::SlaveTatsRuntime m_runtime;
    core::SlaveTatsService m_service{m_runtime};
    const slavetats::interface::Addresses* m_tattooAPI = nullptr;
    PRISMA_UI_API::IVPrismaUI1* m_prismaUI = nullptr;
    PrismaView m_view = 0;
    bool m_jcReady = false;
    std::mutex m_toggleMutex;
    bool m_hidden = false;
};

}  // namespace stui
