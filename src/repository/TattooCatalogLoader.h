#pragma once

#include "repository/TattooRepository.h"

#include <cstddef>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace stui::repository {

struct TattooCatalogIssue {
    std::string sourceId;
    std::string sourceFile;
    std::optional<std::size_t> entryIndex;
    std::string message;
};

struct TattooCatalog {
    TattooRepository repository;
    std::size_t sourceCount{};
    std::vector<TattooCatalogIssue> issues;
};

using TattooCatalogLoadResult = std::expected<TattooCatalog, std::error_code>;

[[nodiscard]] TattooCatalogLoadResult loadTattooCatalog(
    const std::filesystem::path& sourceDirectory);

}  // namespace stui::repository
