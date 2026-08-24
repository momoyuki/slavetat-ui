#include "adapters/PrismaTattooSerializer.h"

#include <format>
#include <string_view>

namespace stui::adapters {
namespace {

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

}  // namespace

std::string toPrismaTattooJSON(const core::TattooEntry& tattoo) {
    return std::format(
        R"({{"handle":{},"name":"{}","section":"{}","area":"{}","texture":"{}","slot":{},"color":{},"locked":{},"alpha":{:.2f}}})",
        tattoo.runtimeHandle,
        escapeJSON(tattoo.name),
        escapeJSON(tattoo.section),
        escapeJSON(tattoo.area),
        escapeJSON(tattoo.texturePath),
        tattoo.slot,
        tattoo.color,
        tattoo.locked ? 1 : 0,
        tattoo.alpha);
}

}  // namespace stui::adapters
