#include "native/BrowserCameraPreferenceStore.h"
#include "runtime/PluginConfigFile.h"
#include <nlohmann/json.hpp>
#include <utility>
namespace stui::native {
namespace {
constexpr auto key="browserCameraViews";
std::optional<BrowserCameraRelativePose> decode(const nlohmann::json& value) {
    if (!value.is_object() || value.size()!=4) return std::nullopt;
    for (const auto* name : {"cameraOffset","pivotOffset"}) {
        if (!value.contains(name) || !value[name].is_array() || value[name].size()!=3)
            return std::nullopt;
        for (const auto& item : value[name]) if (!item.is_number()) return std::nullopt;
    }
    for (const auto* name : {"yaw","pitch"})
        if (!value.contains(name) || !value[name].is_number()) return std::nullopt;
    BrowserCameraRelativePose pose;
    try {
        pose.cameraOffset=value["cameraOffset"].get<CameraVector>();
        pose.pivotOffset=value["pivotOffset"].get<CameraVector>();
        pose.yaw=value["yaw"].get<float>(); pose.pitch=value["pitch"].get<float>();
    } catch (const nlohmann::json::exception&) { return std::nullopt; }
    return validBrowserCameraRelativePose(pose) ? std::optional{pose} : std::nullopt;
}
nlohmann::json encode(const BrowserCameraRelativePose& pose) {
    return {{"cameraOffset",pose.cameraOffset},{"pivotOffset",pose.pivotOffset},
        {"yaw",pose.yaw},{"pitch",pose.pitch}};
}
}
BrowserCameraPreferenceStore::BrowserCameraPreferenceStore(
    std::shared_ptr<runtime::PluginConfigFile> config) : m_config(std::move(config)) {}
bool BrowserCameraPreferenceStore::load() {
    const std::scoped_lock lock(m_mutex);
    m_sides={}; ++m_generation;
    if (!m_config) return false;
    const auto config=m_config->read();
    if (!config) return false;
    if (!config->contains(key)) return true;
    const auto& value=config->at(key);
    if (!value.is_object() || value.size()>3 || !value.contains("version") ||
        !value["version"].is_number_integer() || value["version"]!=1) return false;
    bool valid=true;
    for (int i=0; i<2; ++i) {
        const auto* side=i==0 ? "left" : "right";
        if (!value.contains(side)) continue;
        m_sides[i]=decode(value[side]);
        if (!m_sides[i]) valid=false;
    }
    return valid;
}
std::optional<BrowserCameraRelativePose> BrowserCameraPreferenceStore::pose(bool right) const {
    const std::scoped_lock lock(m_mutex); return m_sides[right ? 1 : 0];
}
std::uint64_t BrowserCameraPreferenceStore::generation() const {
    const std::scoped_lock lock(m_mutex); return m_generation;
}
bool BrowserCameraPreferenceStore::remember(bool right,
    const BrowserCameraRelativePose& pose, std::uint64_t generation) {
    if (!validBrowserCameraRelativePose(pose)) return false;
    const std::scoped_lock lock(m_mutex);
    if (generation!=m_generation) return false;
    // A disk failure must not lose the valid last view during this process.
    m_sides[right ? 1 : 0]=pose;
    if (!m_config) return false;
    return m_config->update([&](nlohmann::json& config) -> runtime::ConfigUpdateResult {
        auto& value=config[key];
        // Reconstruct only this bounded preference subtree, retaining the valid other side.
        value={{"version",1}};
        for (int i=0; i<2; ++i)
            if (m_sides[i]) value[i==0 ? "left" : "right"]=encode(*m_sides[i]);
        return {};
    }).has_value();
}
bool BrowserCameraPreferenceStore::clear() {
    const std::scoped_lock lock(m_mutex);
    if (!m_config || !m_config->update([](nlohmann::json& config) -> runtime::ConfigUpdateResult {
        config.erase(key); return {};
    })) return false;
    m_sides={}; ++m_generation; return true;
}
}
