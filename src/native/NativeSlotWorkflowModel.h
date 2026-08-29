#pragma once

#include "core/TattooModels.h"
#include "native/NativeCatalogBrowserModel.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace stui::native {

enum class SlotWorkflowScreen {
    currentSlots,
    picker,
    preview,
    applying,
};

struct SlotQueryTicket {
    std::uint64_t generation{};
    std::uint32_t actorFormId{};
    core::TattooArea area{core::TattooArea::body};
};

struct SlotApplyTicket {
    std::uint64_t generation{};
    core::ApplyTattooRequest request;
};

class NativeSlotWorkflowModel {
public:
    static constexpr std::size_t kPageSize = 6;

    explicit NativeSlotWorkflowModel(NativeCatalogBrowserModel& catalog) noexcept;

    void start();
    void selectArea(core::TattooArea area);
    void refreshSelectedArea();
    void previousSlotPage();
    void nextSlotPage();
    void setSlotPageNumber(std::size_t oneBasedPage);
    [[nodiscard]] bool selectSlot(std::int32_t slot);
    void backToSlots();
    void selectTattoo(const repository::TattooDefinition& tattoo);
    void cancelPreview();
    [[nodiscard]] bool confirmApply();

    [[nodiscard]] std::optional<SlotQueryTicket> takeSlotQuery();
    [[nodiscard]] std::optional<SlotApplyTicket> takeApplyRequest();
    void completeSlotQuery(std::uint64_t generation, core::TattooSlotsResult result);
    void completeApply(std::uint64_t generation, core::ApplyTattooResult result);

    [[nodiscard]] SlotWorkflowScreen screen() const noexcept;
    [[nodiscard]] core::TattooArea selectedArea() const noexcept;
    [[nodiscard]] const core::TattooSlots* slots() const noexcept;
    [[nodiscard]] std::size_t slotPageIndex() const noexcept;
    [[nodiscard]] std::size_t slotPageCount() const noexcept;
    [[nodiscard]] std::optional<std::int32_t> targetSlot() const noexcept;
    [[nodiscard]] const repository::TattooDefinition* previewTattoo() const noexcept;
    [[nodiscard]] const core::ServiceError* error() const noexcept;

private:
    struct AreaState {
        std::optional<core::TattooSlots> slots;
        std::size_t pageIndex{};
    };

    [[nodiscard]] static std::size_t areaIndex(core::TattooArea area) noexcept;
    [[nodiscard]] AreaState& selectedState() noexcept;
    [[nodiscard]] const AreaState& selectedState() const noexcept;
    [[nodiscard]] std::uint64_t nextGeneration() noexcept;
    void scheduleSlotQuery(core::TattooArea area);
    void clampSelectedPage() noexcept;

    NativeCatalogBrowserModel& m_catalog;
    std::array<AreaState, 4> m_areaStates;
    SlotWorkflowScreen m_screen{SlotWorkflowScreen::currentSlots};
    core::TattooArea m_selectedArea{core::TattooArea::body};
    std::optional<std::int32_t> m_targetSlot;
    std::optional<repository::TattooDefinition> m_previewTattoo;
    std::optional<core::ServiceError> m_error;
    std::optional<SlotQueryTicket> m_pendingSlotQuery;
    std::optional<SlotApplyTicket> m_pendingApply;
    std::optional<std::uint64_t> m_activeSlotQueryGeneration;
    std::optional<std::uint64_t> m_activeApplyGeneration;
    std::uint64_t m_generation{};
    bool m_started{false};
};

}  // namespace stui::native
