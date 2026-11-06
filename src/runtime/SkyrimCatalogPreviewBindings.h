#pragma once

#include "runtime/CatalogPreviewVisualSession.h"
#include "runtime/SkeeCapabilityProbe.h"

namespace stui::runtime {

void collectExternalTextures(core::TattooSlots& slots, skee::OverrideInterface* interface);

CatalogPreviewVisualBindings makeSkyrimCatalogPreviewBindings(
    skee::OverrideInterface* interface);

}  // namespace stui::runtime
