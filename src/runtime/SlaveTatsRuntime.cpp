#include "runtime/SlaveTatsRuntime.h"

#include "jcontainers_mini.h"

#include <string>
#include <vector>

namespace stui::runtime {
namespace {

constexpr const char* kQueryAvailablePool = "SlaveTatsUI-queryAvailable";

class JContainerPoolGuard {
public:
    explicit JContainerPoolGuard(const char* pool) noexcept : m_pool(pool) {}
    ~JContainerPoolGuard() { jcmini::JValue::cleanPool(m_pool); }

    JContainerPoolGuard(const JContainerPoolGuard&) = delete;
    JContainerPoolGuard& operator=(const JContainerPoolGuard&) = delete;

private:
    const char* m_pool;
};

}  // namespace

void SlaveTatsRuntime::bindSlaveTats(const slavetats::interface::Addresses* api) noexcept {
    m_api = api;
    m_apiVersion = api ? api->current_version : 0;
}

void SlaveTatsRuntime::noteSlaveTatsVersionMismatch(std::uint32_t version) noexcept {
    m_api = nullptr;
    m_apiVersion = version;
}

bool SlaveTatsRuntime::bindJContainers(const jc::root_interface* root) {
    m_jContainersReady = root && jcmini::Init(root);
    return m_jContainersReady;
}

bool SlaveTatsRuntime::apiAvailable() const noexcept {
    return m_api != nullptr;
}

bool SlaveTatsRuntime::jContainersReady() const noexcept {
    return m_jContainersReady;
}

std::uint32_t SlaveTatsRuntime::apiVersion() const noexcept {
    return m_apiVersion;
}

const slavetats::interface::Addresses* SlaveTatsRuntime::api() const noexcept {
    return m_api;
}

core::TattooQueryResult SlaveTatsRuntime::queryAvailable(std::string_view domain) {
    const int matches = jcmini::JValue::addToPool(jcmini::JArray::object(), kQueryAvailablePool);
    const JContainerPoolGuard poolGuard(kQueryAvailablePool);
    const std::string domainString(domain);

    if (m_api->query_available_tattoos(
            0, matches, 0, RE::BSFixedString(domainString.c_str()))) {
        return std::unexpected(core::ServiceError{
            core::ServiceErrorCode::queryAvailableFailed,
            "query_available_tattoos failed",
        });
    }

    std::vector<core::TattooEntry> entries;
    const int count = jcmini::JArray::count(matches);
    entries.reserve(static_cast<std::size_t>(count));

    for (int index = 0; index < count; ++index) {
        const int handle = jcmini::JArray::getObj(matches, index);
        const int rawColor = jcmini::JMap::getInt(handle, "color", 0);
        entries.push_back(core::TattooEntry{
            .runtimeHandle = handle,
            .domain = domainString,
            .section = jcmini::JMap::getStr(handle, "section"),
            .name = jcmini::JMap::getStr(handle, "name"),
            .texturePath = jcmini::JMap::getStr(handle, "texture"),
            .area = jcmini::JMap::getStr(handle, "area"),
            .slot = jcmini::JMap::getInt(handle, "slot"),
            .color = rawColor == 0 ? 0xFFFFFF : rawColor,
            .locked = jcmini::JMap::getInt(handle, "locked") != 0,
            .alpha = jcmini::JMap::getFlt(handle, "alpha", 1.0F),
        });
    }

    return entries;
}

}  // namespace stui::runtime
