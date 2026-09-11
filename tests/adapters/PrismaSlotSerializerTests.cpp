#include "adapters/PrismaSlotSerializer.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

using stui::adapters::toPrismaApplySuccessJSON;
using stui::adapters::toPrismaSlotsJSON;
using stui::core::ApplyTattooSuccess;
using stui::core::SlotOccupancy;
using stui::core::TattooArea;
using stui::core::TattooEntry;
using stui::core::TattooSlot;
using stui::core::TattooSlots;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

void completeSlotPayloadPreservesLegacyShape() {
    const TattooSlots slots{
        .actorFormId = 0x14,
        .area = TattooArea::body,
        .configuredCount = 3,
        .slots = {
            TattooSlot{.index = 0, .occupancy = SlotOccupancy::empty},
            TattooSlot{.index = 1, .occupancy = SlotOccupancy::external},
            TattooSlot{
                .index = 2,
                .occupancy = SlotOccupancy::slaveTats,
                .tattoo = TattooEntry{
                    .runtimeHandle = 42,
                    .section = "Pack",
                    .name = "Rose",
                    .texturePath = "rose.dds",
                    .area = "BODY",
                    .slot = 2,
                    .color = 0xFFFFFF,
                    .alpha = 1.0F,
                },
            },
        },
    };

    const std::string expected =
        R"({"type":"slots","area":"BODY","maxSlots":3,"slots":[{"slot":0,"occupied":false},{"slot":1,"occupied":true,"external":true,"name":"[External]"},{"slot":2,"occupied":true,"name":"Rose","section":"Pack","texture":"rose.dds","color":16777215,"alpha":1.00,"handle":42}]})";

    expect(toPrismaSlotsJSON(slots) == expected, "expected unchanged Prisma slots payload");
}

void typedAreasUseLegacyUppercaseNames() {
    const TattooSlots body{.area = TattooArea::body};
    const TattooSlots face{.area = TattooArea::face};
    const TattooSlots hands{.area = TattooArea::hands};
    const TattooSlots feet{.area = TattooArea::feet};

    expect(toPrismaSlotsJSON(body) == R"({"type":"slots","area":"BODY","maxSlots":0,"slots":[]})",
        "expected BODY serializer name");
    expect(toPrismaSlotsJSON(face) == R"({"type":"slots","area":"FACE","maxSlots":0,"slots":[]})",
        "expected FACE serializer name");
    expect(toPrismaSlotsJSON(hands) == R"({"type":"slots","area":"HANDS","maxSlots":0,"slots":[]})",
        "expected HANDS serializer name");
    expect(toPrismaSlotsJSON(feet) == R"({"type":"slots","area":"FEET","maxSlots":0,"slots":[]})",
        "expected FEET serializer name");
}

void slotPayloadEscapesTattooFields() {
    const TattooSlots slots{
        .area = TattooArea::body,
        .configuredCount = 1,
        .slots = {
            TattooSlot{
                .index = 0,
                .occupancy = SlotOccupancy::slaveTats,
                .tattoo = TattooEntry{
                    .runtimeHandle = 7,
                    .section = "Rose\nPack",
                    .name = "Red \"Rose\"",
                    .texturePath = "roses\\red.dds",
                    .area = "BODY",
                    .slot = 0,
                    .color = 0xFFFFFF,
                    .alpha = 0.5F,
                },
            },
        },
    };

    const std::string expected =
        R"({"type":"slots","area":"BODY","maxSlots":1,"slots":[{"slot":0,"occupied":true,"name":"Red \"Rose\"","section":"Rose\nPack","texture":"roses\\red.dds","color":16777215,"alpha":0.50,"handle":7}]})";

    expect(toPrismaSlotsJSON(slots) == expected, "expected escaped Prisma slot tattoo fields");
}

void applySuccessPayloadEscapesTattooIdentity() {
    const ApplyTattooSuccess success{
        .actorFormId = 0x14,
        .area = TattooArea::body,
        .slot = 2,
        .section = "Rose\nPack",
        .name = "Red \"Rose\"",
    };

    const std::string expected =
        R"({"type":"success","action":"applyToSlot","slot":2,"section":"Rose\nPack","name":"Red \"Rose\""})";

    expect(toPrismaApplySuccessJSON(success) == expected,
        "expected unchanged escaped Prisma apply success payload");
}

template <class Test>
int run(std::string_view name, Test&& test) {
    try {
        std::forward<Test>(test)();
        std::cout << "PASS " << name << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << name << ": " << error.what() << '\n';
        return 1;
    }
}

}  // namespace

int main() {
    int failures = 0;
    failures += run("complete slot payload preserves legacy shape", completeSlotPayloadPreservesLegacyShape);
    failures += run("typed areas use legacy uppercase names", typedAreasUseLegacyUppercaseNames);
    failures += run("slot payload escapes tattoo fields", slotPayloadEscapesTattooFields);
    failures += run("apply success payload escapes tattoo identity", applySuccessPayloadEscapesTattooIdentity);
    return failures == 0 ? 0 : 1;
}
