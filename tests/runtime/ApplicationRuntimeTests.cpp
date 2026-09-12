#include "runtime/ApplicationRuntime.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void expect(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

void retainsMismatchedApiVersionWithoutMakingApiAvailable() {
    stui::runtime::ApplicationRuntime application;

    application.noteSlaveTatsVersionMismatch(99);

    expect(!application.runtime().apiAvailable(), "expected mismatched API to remain unavailable");
    expect(application.runtime().apiVersion() == 99, "expected diagnostic API version retention");
    expect(&application.service() == &application.service(), "expected one stable service instance");
}

void bindsSlaveTatsApiToOwnedRuntime() {
    stui::runtime::ApplicationRuntime application;
    slavetats::interface::Addresses api{};

    application.bindSlaveTats(&api);

    expect(application.runtime().apiAvailable(), "expected bound API availability");
    expect(application.runtime().api() == &api, "expected owned runtime to retain API pointer");
}

}  // namespace

int main() {
    try {
        retainsMismatchedApiVersionWithoutMakingApiAvailable();
        std::cout << "PASS retains mismatched API version without making API available\n";
        bindsSlaveTatsApiToOwnedRuntime();
        std::cout << "PASS binds SlaveTats API to owned runtime\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
    return 0;
}
