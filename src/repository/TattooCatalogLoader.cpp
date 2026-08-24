#include "repository/TattooCatalogLoader.h"

#include "repository/TattooSourceParser.h"
#include "repository/TattooSourceScanner.h"

#include <iterator>
#include <utility>

namespace stui::repository {

TattooCatalogLoadResult loadTattooCatalog(const std::filesystem::path& sourceDirectory) {
    auto sources = scanTattooSources(sourceDirectory);
    if (!sources) {
        return std::unexpected(sources.error());
    }

    std::vector<TattooDefinition> definitions;
    std::vector<TattooCatalogIssue> issues;
    for (const auto& source : *sources) {
        auto report = parseTattooSource(source);
        definitions.insert(
            definitions.end(),
            std::make_move_iterator(report.definitions.begin()),
            std::make_move_iterator(report.definitions.end()));
        for (auto& issue : report.issues) {
            issues.push_back(TattooCatalogIssue{
                .sourceId = source.sourceId,
                .sourceFile = source.sourceFile,
                .entryIndex = issue.entryIndex,
                .message = std::move(issue.message),
            });
        }
    }

    return TattooCatalog{
        .repository = TattooRepository(std::move(definitions)),
        .sourceCount = sources->size(),
        .issues = std::move(issues),
    };
}

}  // namespace stui::repository
