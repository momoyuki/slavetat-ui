#include "adapters/PrismaTattooSerializer.h"

#include <iostream>
#include <string>

int main() {
    const stui::core::TattooEntry tattoo{
        .runtimeHandle = 42,
        .domain = "default",
        .section = "Rose\nGarden",
        .name = "Red \"Rose\"",
        .texturePath = "roses\\red.dds",
        .area = "BODY",
        .slot = -1,
        .color = 0xFFFFFF,
        .locked = true,
        .alpha = 0.75F,
    };

    const std::string expected =
        R"({"handle":42,"name":"Red \"Rose\"","section":"Rose\nGarden","area":"BODY","texture":"roses\\red.dds","slot":-1,"color":16777215,"locked":1,"alpha":0.75})";
    const auto actual = stui::adapters::toPrismaTattooJSON(tattoo);

    if (actual != expected) {
        std::cerr << "Expected: " << expected << '\n';
        std::cerr << "Actual:   " << actual << '\n';
        return 1;
    }

    return 0;
}
