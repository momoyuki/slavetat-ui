#include "native/NativeCatalogBrowserModel.h"

#include <utility>

namespace stui::native {

NativeCatalogBrowserModel::NativeCatalogBrowserModel(CatalogSnapshotProvider provider) :
    m_provider(std::move(provider)) {}

void NativeCatalogBrowserModel::refresh() {
    repository::TattooCatalogSnapshot next = m_provider ? m_provider() : nullptr;
    if (next == m_snapshot) {
        return;
    }

    m_snapshot = std::move(next);
    resetFilter();
    query();
}

void NativeCatalogBrowserModel::setSearch(std::string value) {
    m_filter.search = std::move(value);
    m_filter.pageIndex = 0;
    query();
}

void NativeCatalogBrowserModel::setSourceId(std::string value) {
    m_filter.sourceId = std::move(value);
    m_filter.pageIndex = 0;
    query();
}

void NativeCatalogBrowserModel::setSection(std::string value) {
    m_filter.section = std::move(value);
    m_filter.pageIndex = 0;
    query();
}

void NativeCatalogBrowserModel::setArea(std::string value) {
    m_filter.area = std::move(value);
    m_filter.pageIndex = 0;
    query();
}

void NativeCatalogBrowserModel::previousPage() {
    if (m_filter.pageIndex == 0) {
        return;
    }

    --m_filter.pageIndex;
    query();
}

void NativeCatalogBrowserModel::nextPage() {
    if (m_page.pageCount == 0 || m_filter.pageIndex + 1 >= m_page.pageCount) {
        return;
    }

    ++m_filter.pageIndex;
    query();
}

void NativeCatalogBrowserModel::setPageNumber(std::size_t oneBasedPage) {
    m_filter.pageIndex = oneBasedPage == 0 ? 0 : oneBasedPage - 1;
    query();
}

const repository::TattooFilter& NativeCatalogBrowserModel::filter() const noexcept {
    return m_filter;
}

const repository::TattooPage& NativeCatalogBrowserModel::page() const noexcept {
    return m_page;
}

repository::TattooCatalogSnapshot NativeCatalogBrowserModel::snapshot() const noexcept {
    return m_snapshot;
}

void NativeCatalogBrowserModel::resetFilter() {
    m_filter = repository::TattooFilter{.pageSize = kPageSize};
}

void NativeCatalogBrowserModel::query() {
    if (!m_snapshot) {
        m_page = repository::TattooPage{.pageSize = kPageSize};
        return;
    }

    m_page = m_snapshot->repository.query(m_filter);
    m_filter.pageIndex = m_page.pageIndex;
    m_filter.pageSize = kPageSize;
}

}  // namespace stui::native