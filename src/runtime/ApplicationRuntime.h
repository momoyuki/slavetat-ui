#pragma once

#include "core/SlaveTatsService.h"
#include "runtime/SlaveTatsRuntime.h"

#include <cstdint>

namespace stui::runtime {

class ApplicationRuntime {
public:
    void bindSlaveTats(const slavetats::interface::Addresses* api) noexcept;
    void noteSlaveTatsVersionMismatch(std::uint32_t version) noexcept;
    [[nodiscard]] bool bindJContainers(const jc::root_interface* root);

    [[nodiscard]] core::SlaveTatsService& service() noexcept;
    [[nodiscard]] SlaveTatsRuntime& runtime() noexcept;

private:
    SlaveTatsRuntime runtime_;
    core::SlaveTatsService service_{runtime_};
};

}  // namespace stui::runtime
