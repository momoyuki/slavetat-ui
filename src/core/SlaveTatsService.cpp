#include "core/SlaveTatsService.h"

#include <utility>

namespace stui::core {

SlaveTatsService::SlaveTatsService(ITattooRuntime& runtime) noexcept :
    m_runtime(runtime) {}

TattooQueryResult SlaveTatsService::queryAvailable(std::string_view domain) {
    if (!m_runtime.apiAvailable()) {
        return std::unexpected(ServiceError{
            ServiceErrorCode::slaveTatsUnavailable,
            "SlaveTatsNG not available",
        });
    }

    if (!m_runtime.jContainersReady()) {
        return std::unexpected(ServiceError{
            ServiceErrorCode::jContainersUnavailable,
            "JContainers not ready",
        });
    }

    return m_runtime.queryAvailable(domain);
}

}  // namespace stui::core
