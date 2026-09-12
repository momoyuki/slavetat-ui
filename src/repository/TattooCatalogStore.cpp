#include "repository/TattooCatalogStore.h"

#include <utility>

namespace stui::repository {

TattooCatalogRefreshResult TattooCatalogStore::refresh(
    const std::filesystem::path& sourceDirectory) {
    auto catalog = loadTattooCatalog(sourceDirectory);
    if (!catalog) {
        return std::unexpected(catalog.error());
    }

    TattooCatalogSnapshot next =
        std::make_shared<TattooCatalog>(std::move(*catalog));
    m_snapshot.store(next, std::memory_order_release);
    return next;
}

TattooCatalogSnapshot TattooCatalogStore::snapshot() const noexcept {
    return m_snapshot.load(std::memory_order_acquire);
}

}  // namespace stui::repository
