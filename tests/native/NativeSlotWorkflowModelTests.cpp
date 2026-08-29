#include "native/NativeSlotWorkflowModel.h"

#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using stui::core::ApplyTattooSuccess;
using stui::core::ServiceError;
using stui::core::ServiceErrorCode;
using stui::core::SlotOccupancy;
using stui::core::TattooArea;
using stui::core::TattooEntry;
using stui::core::TattooSlot;
using stui::core::TattooSlots;
using stui::native::NativeCatalogBrowserModel;
using stui::native::NativeSlotWorkflowModel;
using stui::native::SlotWorkflowScreen;
using stui::repository::TattooCatalog;
using stui::repository::TattooCatalogSnapshot;
using stui::repository::TattooDefinition;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

TattooDefinition tattoo(std::string name, std::size_t index, std::string area = "BODY") {
    return TattooDefinition{
        .sourceId = "source.json",
        .sourceFile = "source.json",
        .packName = "Fixture Pack",
        .sourceIndex = index,
        .name = std::move(name),
        .section = "Marks",
        .texturePath = "marks/fixture.dds",
        .area = std::move(area),
    };
}

TattooCatalogSnapshot catalogWithEntries(std::size_t count) {
    std::vector<TattooDefinition> definitions;
    definitions.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        definitions.push_back(tattoo("Entry " + std::to_string(index), index));
    }
    return std::make_shared<const TattooCatalog>(TattooCatalog{
        .repository = stui::repository::TattooRepository(std::move(definitions)),
        .sourceCount = 1,
    });
}

TattooSlots slots(TattooArea area, int count) {
    TattooSlots result{
        .actorFormId = 0x14,
        .area = area,
        .configuredCount = count,
    };
    for (int index = 0; index < count; ++index) {
        result.slots.push_back(TattooSlot{
            .index = index,
            .occupancy = SlotOccupancy::empty,
        });
    }
    return result;
}

void completeInitialQuery(NativeSlotWorkflowModel& model, TattooSlots result) {
    model.start();
    const auto ticket = model.takeSlotQuery();
    expect(ticket.has_value(), "expected initial slot query ticket");
    model.completeSlotQuery(ticket->generation, std::move(result));
}

void startSchedulesOnePlayerBodyQuery() {
    TattooCatalogSnapshot snapshot = catalogWithEntries(1);
    NativeCatalogBrowserModel catalog([&snapshot] { return snapshot; });
    catalog.refresh();
    NativeSlotWorkflowModel model(catalog);

    model.start();
    const auto initial = model.takeSlotQuery();

    expect(initial && initial->actorFormId == 0x14 && initial->area == TattooArea::body,
        "expected one initial Player BODY query");
    expect(!model.takeSlotQuery(), "expected repeated reads not to query again");
    model.start();
    expect(!model.takeSlotQuery(), "expected repeated start not to query again");
    expect(model.screen() == SlotWorkflowScreen::currentSlots,
        "expected Current Slots initial screen");
}

void cachesAreaResultsAndPreservesPerAreaPages() {
    TattooCatalogSnapshot snapshot = catalogWithEntries(1);
    NativeCatalogBrowserModel catalog([&snapshot] { return snapshot; });
    catalog.refresh();
    NativeSlotWorkflowModel model(catalog);
    completeInitialQuery(model, slots(TattooArea::body, 12));

    expect(model.slotPageCount() == 2 && model.slotPageIndex() == 0,
        "expected two six-slot BODY pages");
    model.nextSlotPage();
    expect(model.slotPageIndex() == 1, "expected BODY second page");

    model.selectArea(TattooArea::face);
    const auto faceTicket = model.takeSlotQuery();
    expect(faceTicket && faceTicket->area == TattooArea::face,
        "expected uncached FACE query");
    model.completeSlotQuery(faceTicket->generation, slots(TattooArea::face, 3));
    expect(model.slotPageCount() == 1 && model.slotPageIndex() == 0,
        "expected one FACE page");

    model.selectArea(TattooArea::body);
    expect(!model.takeSlotQuery(), "expected cached BODY area not queried again");
    expect(model.slotPageIndex() == 1, "expected BODY page restored");
    model.selectArea(TattooArea::face);
    expect(model.slotPageIndex() == 0, "expected FACE page restored");
}

void clampsSlotPagination() {
    TattooCatalogSnapshot snapshot = catalogWithEntries(1);
    NativeCatalogBrowserModel catalog([&snapshot] { return snapshot; });
    catalog.refresh();
    NativeSlotWorkflowModel model(catalog);
    completeInitialQuery(model, slots(TattooArea::body, 13));

    model.previousSlotPage();
    expect(model.slotPageIndex() == 0, "expected previous clamped at first slot page");
    model.setSlotPageNumber(99);
    expect(model.slotPageIndex() == 2, "expected one-based slot page clamped at final page");
    model.nextSlotPage();
    expect(model.slotPageIndex() == 2, "expected next clamped at final slot page");
    model.setSlotPageNumber(0);
    expect(model.slotPageIndex() == 0, "expected zero page input clamped to first slot page");
}

void externalSlotsAreRejectedButMutableSlotsOpenPicker() {
    TattooCatalogSnapshot snapshot = catalogWithEntries(1);
    NativeCatalogBrowserModel catalog([&snapshot] { return snapshot; });
    catalog.refresh();
    NativeSlotWorkflowModel model(catalog);
    auto bodySlots = slots(TattooArea::body, 3);
    bodySlots.slots[0].occupancy = SlotOccupancy::external;
    bodySlots.slots[1].occupancy = SlotOccupancy::slaveTats;
    bodySlots.slots[1].tattoo = TattooEntry{
        .section = "Existing",
        .name = "Owned",
        .area = "BODY",
        .slot = 1,
    };
    completeInitialQuery(model, std::move(bodySlots));

    expect(!model.selectSlot(0), "expected external slot selection rejected");
    expect(model.screen() == SlotWorkflowScreen::currentSlots,
        "expected external slot to remain on Current Slots");
    expect(model.selectSlot(1), "expected owned slot replace target accepted");
    expect(model.screen() == SlotWorkflowScreen::picker && model.targetSlot() == 1,
        "expected owned slot to open Picker");

    model.backToSlots();
    expect(model.selectSlot(2), "expected empty slot target accepted");
    expect(model.screen() == SlotWorkflowScreen::picker && model.targetSlot() == 2,
        "expected empty slot to open Picker");
}

void previewDoesNotApplyAndCancelPreservesPickerState() {
    TattooCatalogSnapshot snapshot = catalogWithEntries(13);
    NativeCatalogBrowserModel catalog([&snapshot] { return snapshot; });
    catalog.refresh();
    catalog.setSearch("Entry");
    catalog.setSourceId("source.json");
    catalog.setSection("Marks");
    catalog.setArea("BODY");
    catalog.setPageNumber(2);
    NativeSlotWorkflowModel model(catalog);
    completeInitialQuery(model, slots(TattooArea::body, 3));
    expect(model.selectSlot(2), "expected empty target selected");
    const auto filterBefore = catalog.filter();
    const auto pageBefore = catalog.page().pageIndex;
    const auto selected = catalog.page().entries.front();

    model.selectTattoo(selected);

    expect(model.screen() == SlotWorkflowScreen::preview,
        "expected thumbnail selection to enter Preview");
    expect(model.previewTattoo() && model.previewTattoo()->name == selected.name,
        "expected copied preview tattoo");
    expect(!model.takeApplyRequest(), "expected no mutation before explicit confirmation");

    model.cancelPreview();
    expect(model.screen() == SlotWorkflowScreen::picker,
        "expected Cancel to return to Picker");
    expect(catalog.filter().search == filterBefore.search &&
            catalog.filter().sourceId == filterBefore.sourceId &&
            catalog.filter().section == filterBefore.section &&
            catalog.filter().area == filterBefore.area &&
            catalog.page().pageIndex == pageBefore,
        "expected Cancel to preserve picker filters and page");
}

void explicitConfirmationCreatesOneFixedPolicyRequest() {
    TattooCatalogSnapshot snapshot = catalogWithEntries(1);
    NativeCatalogBrowserModel catalog([&snapshot] { return snapshot; });
    catalog.refresh();
    NativeSlotWorkflowModel model(catalog);
    completeInitialQuery(model, slots(TattooArea::body, 3));
    expect(model.selectSlot(2), "expected apply target selected");
    model.selectTattoo(tattoo("Corruption", 7));

    expect(model.confirmApply(), "expected first Apply confirmation accepted");
    expect(!model.confirmApply(), "expected duplicate Apply confirmation rejected");
    const auto ticket = model.takeApplyRequest();

    expect(ticket.has_value(), "expected one apply ticket");
    expect(ticket->request.actorFormId == 0x14 && ticket->request.area == TattooArea::body &&
            ticket->request.slot == 2,
        "expected Player BODY slot target");
    expect(ticket->request.domain == "default" && ticket->request.section == "Marks" &&
            ticket->request.name == "Corruption",
        "expected selected tattoo identity with fixed domain");
    expect(ticket->request.color == 0xFFFFFF && ticket->request.alpha == 1.0F,
        "expected fixed white opaque apply policy");
    expect(model.screen() == SlotWorkflowScreen::applying,
        "expected Applying state after confirmation");
    expect(!model.takeApplyRequest(), "expected apply ticket consumed once");
}

void applySuccessReturnsToSlotsAndRefreshesArea() {
    TattooCatalogSnapshot snapshot = catalogWithEntries(13);
    NativeCatalogBrowserModel catalog([&snapshot] { return snapshot; });
    catalog.refresh();
    catalog.setSearch("Entry");
    catalog.setPageNumber(2);
    NativeSlotWorkflowModel model(catalog);
    completeInitialQuery(model, slots(TattooArea::body, 3));
    expect(model.selectSlot(2), "expected success-flow target selected");
    const auto filterBefore = catalog.filter();
    const auto pageBefore = catalog.page().pageIndex;
    model.selectTattoo(catalog.page().entries.front());
    expect(model.confirmApply(), "expected success-flow confirmation accepted");
    const auto apply = model.takeApplyRequest();

    model.completeApply(apply->generation, ApplyTattooSuccess{
        .actorFormId = 0x14,
        .area = TattooArea::body,
        .slot = 2,
        .section = "Marks",
        .name = "Entry 6",
    });

    expect(model.screen() == SlotWorkflowScreen::currentSlots,
        "expected success to return to Current Slots");
    const auto refresh = model.takeSlotQuery();
    expect(refresh && refresh->area == TattooArea::body,
        "expected success to refresh selected area");
    expect(catalog.filter().search == filterBefore.search &&
            catalog.filter().sourceId == filterBefore.sourceId &&
            catalog.filter().section == filterBefore.section &&
            catalog.filter().area == filterBefore.area &&
            catalog.page().pageIndex == pageBefore,
        "expected Apply success to preserve picker state");
}

void applyFailureRetainsPreviewForRetry() {
    TattooCatalogSnapshot snapshot = catalogWithEntries(1);
    NativeCatalogBrowserModel catalog([&snapshot] { return snapshot; });
    catalog.refresh();
    NativeSlotWorkflowModel model(catalog);
    completeInitialQuery(model, slots(TattooArea::body, 3));
    expect(model.selectSlot(2), "expected failure-flow target selected");
    model.selectTattoo(tattoo("Corruption", 7));
    expect(model.confirmApply(), "expected failure-flow confirmation accepted");
    const auto first = model.takeApplyRequest();

    model.completeApply(first->generation, std::unexpected(ServiceError{
        ServiceErrorCode::applyFailed,
        "apply failed",
    }));

    expect(model.screen() == SlotWorkflowScreen::preview,
        "expected apply failure to return to Preview");
    expect(model.previewTattoo() && model.targetSlot() == 2,
        "expected failed preview and target retained");
    expect(model.error() && model.error()->message == "apply failed",
        "expected apply error exposed");
    expect(model.confirmApply(), "expected retry confirmation accepted");
    const auto retry = model.takeApplyRequest();
    expect(retry && retry->generation > first->generation,
        "expected retry to use a newer generation");
}

void staleCompletionsAreIgnored() {
    TattooCatalogSnapshot snapshot = catalogWithEntries(1);
    NativeCatalogBrowserModel catalog([&snapshot] { return snapshot; });
    catalog.refresh();
    NativeSlotWorkflowModel model(catalog);
    model.start();
    const auto body = model.takeSlotQuery();
    model.selectArea(TattooArea::face);
    const auto face = model.takeSlotQuery();

    model.completeSlotQuery(body->generation, slots(TattooArea::body, 12));
    expect(model.slots() == nullptr, "expected stale BODY completion ignored while FACE selected");
    model.completeSlotQuery(face->generation, slots(TattooArea::face, 3));
    expect(model.slots() && model.slots()->area == TattooArea::face,
        "expected current FACE completion accepted");

    expect(model.selectSlot(1), "expected FACE target selected");
    model.selectTattoo(tattoo("Face Mark", 1, "FACE"));
    expect(model.confirmApply(), "expected stale-completion confirmation accepted");
    const auto apply = model.takeApplyRequest();
    model.completeApply(apply->generation + 1, ApplyTattooSuccess{});
    expect(model.screen() == SlotWorkflowScreen::applying,
        "expected stale apply completion ignored");
}

void queryFailureRemainsRetryable() {
    TattooCatalogSnapshot snapshot = catalogWithEntries(1);
    NativeCatalogBrowserModel catalog([&snapshot] { return snapshot; });
    catalog.refresh();
    NativeSlotWorkflowModel model(catalog);
    model.start();
    const auto query = model.takeSlotQuery();

    model.completeSlotQuery(query->generation, std::unexpected(ServiceError{
        ServiceErrorCode::jContainersUnavailable,
        "JContainers not ready",
    }));

    expect(model.error() && model.error()->code == ServiceErrorCode::jContainersUnavailable,
        "expected query failure exposed");
    model.refreshSelectedArea();
    const auto retry = model.takeSlotQuery();
    expect(retry && retry->generation > query->generation,
        "expected Refresh to create a newer query generation");
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
    failures += run("start schedules one Player BODY query", startSchedulesOnePlayerBodyQuery);
    failures += run("caches area results and preserves per-area pages", cachesAreaResultsAndPreservesPerAreaPages);
    failures += run("clamps slot pagination", clampsSlotPagination);
    failures += run("external slots are rejected but mutable slots open Picker", externalSlotsAreRejectedButMutableSlotsOpenPicker);
    failures += run("preview does not apply and Cancel preserves picker state", previewDoesNotApplyAndCancelPreservesPickerState);
    failures += run("explicit confirmation creates one fixed-policy request", explicitConfirmationCreatesOneFixedPolicyRequest);
    failures += run("apply success returns to slots and refreshes area", applySuccessReturnsToSlotsAndRefreshesArea);
    failures += run("apply failure retains Preview for retry", applyFailureRetainsPreviewForRetry);
    failures += run("stale completions are ignored", staleCompletionsAreIgnored);
    failures += run("query failure remains retryable", queryFailureRemainsRetryable);
    return failures == 0 ? 0 : 1;
}
