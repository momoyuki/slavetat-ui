#include "runtime/ApplicationRuntime.h"

namespace stui::runtime {

void ApplicationRuntime::bindSlaveTats(
    const slavetats::interface::Addresses* api) noexcept {
    runtime_.bindSlaveTats(api);
}

void ApplicationRuntime::noteSlaveTatsVersionMismatch(std::uint32_t version) noexcept {
    runtime_.noteSlaveTatsVersionMismatch(version);
}

bool ApplicationRuntime::bindJContainers(const jc::root_interface* root) {
    return runtime_.bindJContainers(root);
}

core::SlaveTatsService& ApplicationRuntime::service() noexcept {
    return service_;
}

SlaveTatsRuntime& ApplicationRuntime::runtime() noexcept {
    return runtime_;
}

}  // namespace stui::runtime
