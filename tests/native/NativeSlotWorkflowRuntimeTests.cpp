#include "native/NativeSlotWorkflowRuntime.h"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using stui::core::ApplyTattooRequest;
using stui::core::ApplyTattooSuccess;
using stui::core::RemoveTattooRequest;
using stui::core::RemoveTattooSuccess;
using stui::core::ServiceErrorCode;
using stui::core::SlotOccupancy;
using stui::core::TattooArea;
using stui::core::TattooSlot;
using stui::core::TattooSlots;
using stui::native::NativeCatalogBrowserModel;
using stui::native::NativeSlotTask;
using stui::native::NativeSlotWorkflowModel;
using stui::native::NativeSlotWorkflowRuntime;
using stui::native::SlotWorkflowScreen;
using stui::repository::TattooCatalog;
using stui::repository::TattooDefinition;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

TattooSlots bodySlots() {
    return TattooSlots{
        .actorFormId = 0x14,
        .area = TattooArea::body,
        .configuredCount = 2,
        .slots = {
            TattooSlot{.index = 0, .occupancy = SlotOccupancy::empty},
            TattooSlot{.index = 1, .occupancy = SlotOccupancy::empty},
        },
    };
}

TattooSlots bodySlotsWithOwnedTattoo() {
    auto result = bodySlots();
    result.slots[1].occupancy = SlotOccupancy::slaveTats;
    result.slots[1].tattoo = stui::core::TattooEntry{
        .section = "Marks",
        .name = "Existing",
        .area = "BODY",
        .slot = 1,
    };
    return result;
}

TattooDefinition tattoo() {
    return TattooDefinition{
        .sourceId = "fixture.json",
        .sourceFile = "fixture.json",
        .packName = "Fixture",
        .name = "Rose",
        .section = "Marks",
        .texturePath = "rose.dds",
        .area = "Body",
    };
}

struct Fixture {
    Fixture()
        : catalog([this] { return snapshot; }),
          model(catalog),
          runtime(
              model,
              [this](std::uint32_t actor, TattooArea area) {
                  ++queryCount;
                  queriedActor = actor;
                  queriedArea = area;
                  if (queryThrows) {
                      throw std::runtime_error("query failed");
                  }
                  return stui::core::TattooSlotsResult(
                      returnOwnedSlot ? bodySlotsWithOwnedTattoo() : bodySlots());
              },
              [this](const ApplyTattooRequest& request) {
                  ++applyCount;
                  appliedRequest = request;
                  if (applyThrows) {
                      throw std::runtime_error("apply failed");
                  }
                  return stui::core::ApplyTattooResult(ApplyTattooSuccess{
                      .actorFormId = request.actorFormId,
                      .area = request.area,
                      .slot = request.slot,
                      .section = request.section,
                      .name = request.name,
                  });
              },
              [this](const RemoveTattooRequest& request) {
                  ++removeCount;
                  removedRequest = request;
                  if (removeThrows) {
                      throw std::runtime_error("remove failed");
                  }
                  return stui::core::RemoveTattooResult(RemoveTattooSuccess{
                      .actorFormId = request.actorFormId,
                      .area = request.area,
                      .slot = request.slot,
                  });
              },
              [this](NativeSlotTask task) {
                  if (schedulerThrows) {
                      schedulerThrows = false;
                      throw std::runtime_error("scheduler failed");
                  }
                  scheduled.push_back(std::move(task));
              }) {}

    stui::repository::TattooCatalogSnapshot snapshot =
        std::make_shared<const TattooCatalog>(TattooCatalog{
            .repository = stui::repository::TattooRepository({}),
            .sourceCount = 0,
        });
    NativeCatalogBrowserModel catalog;
    NativeSlotWorkflowModel model;
    std::vector<NativeSlotTask> scheduled;
    std::size_t queryCount{};
    std::size_t applyCount{};
    std::size_t removeCount{};
    std::uint32_t queriedActor{};
    TattooArea queriedArea{TattooArea::feet};
    ApplyTattooRequest appliedRequest;
    RemoveTattooRequest removedRequest;
    bool returnOwnedSlot{};
    bool queryThrows{};
    bool applyThrows{};
    bool removeThrows{};
    bool schedulerThrows{};
    NativeSlotWorkflowRuntime runtime;
};

void schedulesOnlyOneQueryAndCompletesModel() {
    Fixture fixture;
    fixture.model.start();

    fixture.runtime.pump();
    fixture.runtime.pump();
    expect(fixture.scheduled.size() == 1,
        "expected repeated pumps to preserve one in-flight query");
    expect(fixture.queryCount == 0, "expected operation deferred to scheduler task");

    fixture.scheduled.front()();
    expect(fixture.queryCount == 1 && fixture.queriedActor == 0x14 &&
            fixture.queriedArea == TattooArea::body,
        "expected Player BODY query on scheduled task");
    expect(fixture.model.slots() && fixture.model.slots()->slots.size() == 2,
        "expected query completion published to workflow model");
}

void applySchedulesOnlyAfterExplicitConfirmation() {
    Fixture fixture;
    fixture.model.start();
    fixture.runtime.pump();
    fixture.scheduled.front()();
    expect(fixture.model.selectSlot(1), "expected empty slot selection");
    fixture.model.selectTattoo(tattoo());

    fixture.runtime.pump();
    expect(fixture.scheduled.size() == 1,
        "expected preview alone not to schedule Apply");
    expect(fixture.model.confirmApply(), "expected explicit Apply confirmation");
    fixture.runtime.pump();
    fixture.runtime.pump();
    expect(fixture.scheduled.size() == 2,
        "expected one Apply task after confirmation");

    fixture.scheduled.back()();
    expect(fixture.applyCount == 1 && fixture.appliedRequest.slot == 1,
        "expected confirmed slot request forwarded once");
    expect(fixture.model.screen() == SlotWorkflowScreen::currentSlots,
        "expected successful Apply to return to Current Slots");
    fixture.runtime.pump();
    expect(fixture.scheduled.size() == 3,
        "expected successful Apply to schedule a fresh slot query");
}

void removeSchedulesOnlyAfterExplicitConfirmation() {
    Fixture fixture;
    fixture.returnOwnedSlot = true;
    fixture.model.start();
    fixture.runtime.pump();
    fixture.scheduled.front()();
    expect(fixture.model.selectSlot(1), "expected owned slot selection");
    expect(fixture.model.requestRemove(), "expected Remove action");

    fixture.runtime.pump();
    expect(fixture.scheduled.size() == 1,
        "expected confirmation screen alone not to schedule Remove");
    expect(fixture.model.confirmRemove(), "expected explicit Remove confirmation");
    fixture.runtime.pump();
    fixture.runtime.pump();
    expect(fixture.scheduled.size() == 2,
        "expected one Remove task after confirmation");

    fixture.scheduled.back()();
    expect(fixture.removeCount == 1 &&
            fixture.removedRequest.actorFormId == 0x14 &&
            fixture.removedRequest.area == TattooArea::body &&
            fixture.removedRequest.slot == 1,
        "expected confirmed Remove target forwarded once");
    expect(fixture.model.screen() == SlotWorkflowScreen::currentSlots,
        "expected successful Remove to return to Current Slots");
    fixture.runtime.pump();
    expect(fixture.scheduled.size() == 3,
        "expected successful Remove to schedule a fresh slot query");
}

void convertsOperationAndSchedulerExceptionsToModelErrors() {
    Fixture queryFailure;
    queryFailure.queryThrows = true;
    queryFailure.model.start();
    queryFailure.runtime.pump();
    queryFailure.scheduled.front()();
    expect(queryFailure.model.error() &&
            queryFailure.model.error()->code == ServiceErrorCode::slotQueryFailed,
        "expected query exception converted to slotQueryFailed");

    Fixture schedulingFailure;
    schedulingFailure.schedulerThrows = true;
    schedulingFailure.model.start();
    schedulingFailure.runtime.pump();
    expect(schedulingFailure.model.error() &&
            schedulingFailure.model.error()->code == ServiceErrorCode::slotQueryFailed,
        "expected scheduler exception to complete the pending query");
}

void convertsApplyAndApplySchedulerExceptionsToModelErrors() {
    Fixture applyFailure;
    applyFailure.model.start();
    applyFailure.runtime.pump();
    applyFailure.scheduled.front()();
    expect(applyFailure.model.selectSlot(0), "expected Apply failure target slot");
    applyFailure.model.selectTattoo(tattoo());
    expect(applyFailure.model.confirmApply(), "expected Apply failure confirmation");
    applyFailure.applyThrows = true;
    applyFailure.runtime.pump();
    applyFailure.scheduled.back()();
    expect(applyFailure.model.screen() == SlotWorkflowScreen::preview &&
            applyFailure.model.error() &&
            applyFailure.model.error()->code == ServiceErrorCode::applyFailed,
        "expected Apply exception converted to retryable applyFailed error");

    Fixture schedulingFailure;
    schedulingFailure.model.start();
    schedulingFailure.runtime.pump();
    schedulingFailure.scheduled.front()();
    expect(schedulingFailure.model.selectSlot(0),
        "expected Apply scheduler failure target slot");
    schedulingFailure.model.selectTattoo(tattoo());
    expect(schedulingFailure.model.confirmApply(),
        "expected Apply scheduler failure confirmation");
    schedulingFailure.schedulerThrows = true;
    schedulingFailure.runtime.pump();
    expect(schedulingFailure.model.screen() == SlotWorkflowScreen::preview &&
            schedulingFailure.model.error() &&
            schedulingFailure.model.error()->code == ServiceErrorCode::applyFailed,
        "expected Apply scheduler exception converted to retryable error");
}

void convertsRemoveExceptionsToRetryableModelErrors() {
    Fixture removeFailure;
    removeFailure.returnOwnedSlot = true;
    removeFailure.model.start();
    removeFailure.runtime.pump();
    removeFailure.scheduled.front()();
    expect(removeFailure.model.selectSlot(1) &&
            removeFailure.model.requestRemove() &&
            removeFailure.model.confirmRemove(),
        "expected confirmed Remove failure flow");
    removeFailure.removeThrows = true;
    removeFailure.runtime.pump();
    removeFailure.scheduled.back()();

    expect(removeFailure.model.screen() == SlotWorkflowScreen::removeConfirmation &&
            removeFailure.model.error() &&
            removeFailure.model.error()->code == ServiceErrorCode::removeFailed,
        "expected Remove exception converted to retryable error");
}

void ignoresStaleCompletionAfterAReplacementQuery() {
    Fixture fixture;
    fixture.model.start();
    fixture.runtime.pump();
    fixture.model.refreshSelectedArea();

    fixture.scheduled.front()();
    expect(!fixture.model.slots(), "expected stale completion ignored by generation guard");
    fixture.runtime.pump();
    expect(fixture.scheduled.size() == 2,
        "expected replacement query scheduled after old task completes");
    fixture.scheduled.back()();
    expect(fixture.model.slots(), "expected replacement completion accepted");
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
    failures += run("schedules only one query and completes model", schedulesOnlyOneQueryAndCompletesModel);
    failures += run("apply schedules only after explicit confirmation", applySchedulesOnlyAfterExplicitConfirmation);
    failures += run("remove schedules only after explicit confirmation", removeSchedulesOnlyAfterExplicitConfirmation);
    failures += run("converts operation and scheduler exceptions to model errors", convertsOperationAndSchedulerExceptionsToModelErrors);
    failures += run("converts Apply and scheduler exceptions to model errors", convertsApplyAndApplySchedulerExceptionsToModelErrors);
    failures += run("converts Remove exceptions to retryable model errors", convertsRemoveExceptionsToRetryableModelErrors);
    failures += run("ignores stale completion after replacement query", ignoresStaleCompletionAfterAReplacementQuery);
    return failures == 0 ? 0 : 1;
}
