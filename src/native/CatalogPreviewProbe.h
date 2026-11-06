#pragma once
#include "core/TattooModels.h"
#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <vector>
namespace stui::native {
enum class CatalogPreviewMaterialSupport {
    missingGeometry, unsupportedShader, tintConversionRequired,
    missingEmissionStorage, scalarCandidate
};
struct CatalogPreviewMaterialFacts {
    bool geometry{}, lightingMaterial{}, compatibleTint{}, emissionStorage{};
};
inline CatalogPreviewMaterialSupport catalogPreviewMaterialSupport(
    CatalogPreviewMaterialFacts facts) noexcept {
    if (!facts.geometry) return CatalogPreviewMaterialSupport::missingGeometry;
    if (!facts.lightingMaterial) return CatalogPreviewMaterialSupport::unsupportedShader;
    if (!facts.compatibleTint) return CatalogPreviewMaterialSupport::tintConversionRequired;
    if (!facts.emissionStorage) return CatalogPreviewMaterialSupport::missingEmissionStorage;
    return CatalogPreviewMaterialSupport::scalarCandidate;
}
inline std::optional<std::string> catalogPreviewNodeName(core::TattooArea area,int slot) {
    if (slot < 0) return std::nullopt;
    const char* prefix{};
    switch (area) {
    case core::TattooArea::body: prefix="Body"; break;
    case core::TattooArea::face: prefix="Face"; break;
    case core::TattooArea::hands: prefix="Hands"; break;
    case core::TattooArea::feet: prefix="Feet"; break;
    default: return std::nullopt;
    }
    return std::string(prefix)+" [Ovl"+std::to_string(slot)+"]";
}
struct ExternalProbeTarget { std::uint32_t actorFormId; std::string nodeName; std::int32_t slotIndex; };
inline std::vector<ExternalProbeTarget> externalProbeTargets(const core::TattooSlots& slots) {
    std::vector<ExternalProbeTarget> targets;
    if (slots.actorFormId == 0) return targets;
    for (const auto& slot : slots.slots) {
        if (slot.occupancy != core::SlotOccupancy::external) continue;
        const auto name = catalogPreviewNodeName(slots.area, slot.index);
        if (name) targets.push_back({slots.actorFormId, *name, slot.index});
    }
    return targets;
}
enum class ExternalProbeComparison { unavailable, match, mismatch };
inline ExternalProbeComparison compareExternalPaths(const std::optional<std::string>& property, const std::optional<std::string>& material) {
    if (!property || !material || property->empty() || material->empty()) return ExternalProbeComparison::unavailable;
    const auto canonical = [](unsigned char c) { return c == '\\' ? '/' : static_cast<char>(std::tolower(c)); };
    return property->size() == material->size() && std::equal(property->begin(), property->end(), material->begin(), [&](char a, char b) { return canonical(a) == canonical(b); })
        ? ExternalProbeComparison::match : ExternalProbeComparison::mismatch;
}
// Scheduled game-thread diagnostics only. Does not call SKEE property setters/getters.
void logCatalogPreviewNodeCapabilities(const core::TattooSlots& slots);
} // namespace stui::native
