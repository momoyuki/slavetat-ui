#include "runtime/SlaveTatsAlpha.h"

#include <cmath>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

void expect(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

bool nearlyEqual(float left, float right) {
    return std::fabs(left - right) < 0.000001F;
}

void visibleAlphaEncodesAsSlaveTatsInvertedAlpha() {
    expect(nearlyEqual(stui::runtime::toSlaveTatsInvertedAlpha(0.0F), 1.0F),
        "expected transparent visible Alpha encoded as invertedAlpha 1.0");
    expect(nearlyEqual(stui::runtime::toSlaveTatsInvertedAlpha(0.1F), 0.9F),
        "expected visible Alpha 0.1 encoded as invertedAlpha 0.9");
    expect(nearlyEqual(stui::runtime::toSlaveTatsInvertedAlpha(0.4F), 0.6F),
        "expected visible Alpha 0.4 encoded as invertedAlpha 0.6");
    expect(nearlyEqual(stui::runtime::toSlaveTatsInvertedAlpha(1.0F), 0.0F),
        "expected opaque visible Alpha encoded as invertedAlpha 0.0");
}

void slaveTatsInvertedAlphaDecodesAsVisibleAlpha() {
    expect(nearlyEqual(stui::runtime::fromSlaveTatsInvertedAlpha(1.0F), 0.0F),
        "expected invertedAlpha 1.0 decoded as transparent");
    expect(nearlyEqual(stui::runtime::fromSlaveTatsInvertedAlpha(0.9F), 0.1F),
        "expected invertedAlpha 0.9 decoded as visible Alpha 0.1");
    expect(nearlyEqual(stui::runtime::fromSlaveTatsInvertedAlpha(0.6F), 0.4F),
        "expected invertedAlpha 0.6 decoded as visible Alpha 0.4");
    expect(nearlyEqual(stui::runtime::fromSlaveTatsInvertedAlpha(0.0F), 1.0F),
        "expected invertedAlpha 0.0 decoded as opaque");
}

void alphaConversionClampsCorruptStoredValues() {
    expect(nearlyEqual(stui::runtime::toSlaveTatsInvertedAlpha(-1.0F), 1.0F) &&
            nearlyEqual(stui::runtime::toSlaveTatsInvertedAlpha(2.0F), 0.0F),
        "expected visible Alpha clamped before encoding");
    expect(nearlyEqual(stui::runtime::fromSlaveTatsInvertedAlpha(-1.0F), 1.0F) &&
            nearlyEqual(stui::runtime::fromSlaveTatsInvertedAlpha(2.0F), 0.0F),
        "expected stored invertedAlpha clamped before decoding");
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
    failures += run(
        "visible Alpha encodes as SlaveTats invertedAlpha",
        visibleAlphaEncodesAsSlaveTatsInvertedAlpha);
    failures += run(
        "SlaveTats invertedAlpha decodes as visible Alpha",
        slaveTatsInvertedAlphaDecodesAsVisibleAlpha);
    failures += run(
        "Alpha conversion clamps corrupt stored values",
        alphaConversionClampsCorruptStoredValues);
    return failures == 0 ? 0 : 1;
}
