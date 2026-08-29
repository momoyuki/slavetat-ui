#include "runtime/OverlaySlotConfiguration.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using stui::core::TattooArea;
using stui::runtime::OverlaySlotConfiguration;

struct ReadCall {
    std::wstring section;
    std::wstring key;
    int fallback;
};

void expect(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

void mappedSectionsUseExpectedFallbacks() {
    std::vector<ReadCall> calls;
    OverlaySlotConfiguration configuration(
        [&](std::wstring_view section, std::wstring_view key, int fallback) {
            calls.push_back(ReadCall{
                .section = std::wstring(section),
                .key = std::wstring(key),
                .fallback = fallback,
            });
            return fallback;
        });

    expect(configuration.count(TattooArea::body) == 12, "expected BODY fallback count");
    expect(configuration.count(TattooArea::face) == 3, "expected FACE fallback count");
    expect(configuration.count(TattooArea::hands) == 3, "expected HANDS fallback count");
    expect(configuration.count(TattooArea::feet) == 3, "expected FEET fallback count");

    expect(calls.size() == 4, "expected one INI read per typed area");
    expect(calls[0].section == L"Overlays/Body" && calls[0].key == L"iNumOverlays" && calls[0].fallback == 12,
        "expected BODY INI mapping");
    expect(calls[1].section == L"Overlays/Face" && calls[1].key == L"iNumOverlays" && calls[1].fallback == 3,
        "expected FACE INI mapping");
    expect(calls[2].section == L"Overlays/Hands" && calls[2].key == L"iNumOverlays" && calls[2].fallback == 3,
        "expected HANDS INI mapping");
    expect(calls[3].section == L"Overlays/Feet" && calls[3].key == L"iNumOverlays" && calls[3].fallback == 3,
        "expected FEET INI mapping");
}

void configuredValueOverridesFallback() {
    OverlaySlotConfiguration configuration(
        [](std::wstring_view section, std::wstring_view, int fallback) {
            if (section == L"Overlays/Body") {
                return 18;
            }
            return fallback;
        });

    expect(configuration.count(TattooArea::body) == 18, "expected configured BODY overlay count");
    expect(configuration.count(TattooArea::face) == 3, "expected FACE fallback count");
}

void eachAreaIsCachedAfterFirstRead() {
    int reads = 0;
    OverlaySlotConfiguration configuration(
        [&](std::wstring_view, std::wstring_view, int fallback) {
            ++reads;
            return fallback + reads;
        });

    expect(configuration.count(TattooArea::body) == 13, "expected first BODY read result");
    expect(configuration.count(TattooArea::body) == 13, "expected cached BODY result");
    expect(configuration.count(TattooArea::face) == 5, "expected first FACE read result");
    expect(configuration.count(TattooArea::face) == 5, "expected cached FACE result");
    expect(reads == 2, "expected exactly one read for each requested area");
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
    failures += run("mapped sections use expected fallbacks", mappedSectionsUseExpectedFallbacks);
    failures += run("configured value overrides fallback", configuredValueOverridesFallback);
    failures += run("each area is cached after first read", eachAreaIsCachedAfterFirstRead);
    return failures == 0 ? 0 : 1;
}
