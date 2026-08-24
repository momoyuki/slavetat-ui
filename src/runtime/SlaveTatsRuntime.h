#pragma once

#include "core/ITattooRuntime.h"
#include "JContainers/jc_interface.h"
#include "SlaveTatsNG_Interface.h"

#include <cstdint>
#include <string_view>

namespace stui::runtime {

class SlaveTatsRuntime final : public core::ITattooRuntime {
public:
    void bindSlaveTats(const slavetats::interface::Addresses* api) noexcept;
    void noteSlaveTatsVersionMismatch(std::uint32_t version) noexcept;
    [[nodiscard]] bool bindJContainers(const jc::root_interface* root);

    [[nodiscard]] bool apiAvailable() const noexcept override;
    [[nodiscard]] bool jContainersReady() const noexcept override;
    [[nodiscard]] std::uint32_t apiVersion() const noexcept;
    [[nodiscard]] const slavetats::interface::Addresses* api() const noexcept;
    core::TattooQueryResult queryAvailable(std::string_view domain) override;

private:
    const slavetats::interface::Addresses* m_api{nullptr};
    std::uint32_t m_apiVersion{0};
    bool m_jContainersReady{false};
};

}  // namespace stui::runtime
