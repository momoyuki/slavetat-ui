#include "native/BrowserCameraPreferenceStore.h"
#include "runtime/PluginConfigFile.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <nlohmann/json.hpp>
using namespace stui;
void expect(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
int main() {
 try {
    const auto path = std::filesystem::temp_directory_path() / "SlaveTatsUICameraPreferenceTests.json";
    std::error_code ignored; std::filesystem::remove(path, ignored);
    auto config = std::make_shared<runtime::PluginConfigFile>(path);
    native::BrowserCameraPreferenceStore store(config);
    expect(store.load() && !store.pose(false), "missing config uses initial framing");
    expect(config->update([](nlohmann::json& j) -> runtime::ConfigUpdateResult {
        j["thumbnailBackground"]="white"; j["thumbnailDiskCacheLimitMiB"]=512;
        j["hotkey"]=67; j["favorites"]={"keep"}; return {};
    }).has_value(), "fixture saves");
    const native::BrowserCameraRelativePose left{{10,-200,80},{0,0,80},0.4F,-0.2F};
    const native::BrowserCameraRelativePose right{{-20,-300,70},{0,10,90},-0.6F,0.3F};
    const auto generation=store.generation();
    expect(store.remember(false,left,generation) && store.remember(true,right,generation), "both sides save");
    native::BrowserCameraPreferenceStore restarted(config);
    expect(restarted.load() && restarted.pose(false)->cameraOffset==left.cameraOffset &&
        restarted.pose(true)->cameraOffset==right.cameraOffset, "restart retains independent sides");
    const auto saved=config->read();
    expect(saved && saved->at("thumbnailBackground")=="white" && saved->at("hotkey")==67 &&
        saved->at("thumbnailDiskCacheLimitMiB")==512 && saved->at("favorites")==nlohmann::json({"keep"}), "other settings survive");
    expect(store.clear() && !store.pose(false) && !store.pose(true), "clear both sides");
    expect(!store.remember(false,left,generation) && !store.pose(false), "old session cannot repopulate clear");
    expect(restarted.load() && !restarted.pose(false), "clear persists across restart");
    expect(store.remember(false,left,store.generation()), "fresh session can remember after clear");
    { std::ofstream out(path); out<<"broken"; }
    expect(!store.clear() && store.pose(false).has_value(), "failed clear retains current view");
    expect(!store.remember(true,right,store.generation()) && store.pose(true).has_value(), "failed save keeps safe session recall");
    expect(!restarted.load() && !restarted.pose(false), "malformed root falls back safely");
    std::filesystem::remove(path,ignored);
    expect(store.remember(false,left,store.generation()) && store.remember(true,right,store.generation()), "restore fixture");
    expect(config->update([](nlohmann::json& j) -> runtime::ConfigUpdateResult {
        j["browserCameraViews"]["left"]["cameraOffset"]={1,2,5000}; return {};
    }).has_value(), "invalid side fixture saves");
    expect(!restarted.load() && !restarted.pose(false) && restarted.pose(true).has_value(), "bad side does not discard valid opposite side");
    for (const auto* malformed : {"null", "[]", "{}", "{\"version\":2}",
         "{\"version\":1,\"left\":{\"cameraOffset\":[1,2],\"pivotOffset\":[0,0,0],\"yaw\":0,\"pitch\":0}}"}) {
        { std::ofstream out(path); out<<"{\"browserCameraViews\":"<<malformed<<"}"; }
        expect(!restarted.load() && !restarted.pose(false), "invalid preference falls back");
    }
    auto invalid=left; invalid.pitch=2;
    expect(!store.remember(false,invalid,store.generation()), "invalid pitch rejects capture");
    invalid=left; invalid.cameraOffset[0]=std::numeric_limits<float>::infinity();
    expect(!store.remember(false,invalid,store.generation()), "nonfinite capture rejected");
    std::filesystem::remove(path,ignored);
 } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
 std::cout<<"PASS browser camera preference persistence\n";
}
