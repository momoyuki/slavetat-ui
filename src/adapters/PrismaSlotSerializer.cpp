#include "adapters/PrismaSlotSerializer.h"

#include <format>
#include <string_view>

namespace stui::adapters {
namespace {

std::string_view areaName(core::TattooArea area) noexcept {
    switch (area) {
    case core::TattooArea::body:
        return "BODY";
    case core::TattooArea::face:
        return "FACE";
    case core::TattooArea::hands:
        return "HANDS";
    case core::TattooArea::feet:
        return "FEET";
    }

    return "BODY";
}

std::string escapeJSON(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());

    for (const char character : value) {
        switch (character) {
        case '"':
            escaped += "\\\"";
            break;
        case '\\':
            escaped += "\\\\";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += character;
            break;
        }
    }

    return escaped;
}

std::string toPrismaSlotJSON(const core::TattooSlot& slot) {
    if (slot.occupancy == core::SlotOccupancy::external) {
        return std::format(
            R"({{"slot":{},"occupied":true,"external":true,"name":"[External]"}})",
            slot.index);
    }

    if (slot.occupancy == core::SlotOccupancy::slaveTats && slot.tattoo) {
        const auto& tattoo = *slot.tattoo;
        return std::format(
            R"({{"slot":{},"occupied":true,"name":"{}","section":"{}","texture":"{}","color":{},"alpha":{:.2f},"handle":{}}})",
            slot.index,
            escapeJSON(tattoo.name),
            escapeJSON(tattoo.section),
            escapeJSON(tattoo.texturePath),
            tattoo.color,
            tattoo.alpha,
            tattoo.runtimeHandle);
    }

    return std::format(R"({{"slot":{},"occupied":false}})", slot.index);
}

}  // namespace

std::string toPrismaSlotsJSON(const core::TattooSlots& slots) {
    std::string serializedSlots;
    for (std::size_t index = 0; index < slots.slots.size(); ++index) {
        if (index > 0) {
            serializedSlots += ',';
        }
        serializedSlots += toPrismaSlotJSON(slots.slots[index]);
    }

    return std::format(
        R"({{"type":"slots","area":"{}","maxSlots":{},"slots":[{}]}})",
        areaName(slots.area),
        slots.configuredCount,
        serializedSlots);
}

std::string toPrismaApplySuccessJSON(const core::ApplyTattooSuccess& success) {
    return std::format(
        R"({{"type":"success","action":"applyToSlot","slot":{},"section":"{}","name":"{}"}})",
        success.slot,
        escapeJSON(success.section),
        escapeJSON(success.name));
}

}  // namespace stui::adapters
