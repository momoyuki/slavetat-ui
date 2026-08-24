#include "repository/TattooCatalogLoader.h"

#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

namespace fs = std::filesystem;
using stui::repository::loadTattooCatalog;

class TemporarySourceDirectory {
public:
    TemporarySourceDirectory() {
        const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = fs::temp_directory_path() /
            ("slavetats-ui-catalog-loader-" + std::to_string(unique));
        fs::create_directories(m_path);
    }

    ~TemporarySourceDirectory() {
        std::error_code error;
        fs::remove_all(m_path, error);
    }

    void write(std::string_view filename, std::string_view json) const {
        std::ofstream stream(m_path / filename);
        stream << json;
    }

    [[nodiscard]] const fs::path& path() const noexcept {
        return m_path;
    }

private:
    fs::path m_path;
};

void expect(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

void loadsEffectiveSourcesIntoQueryableCatalog() {
    TemporarySourceDirectory directory;
    directory.write(
        "Marks.json",
        R"([{"name":"Rose","section":"Flowers","texture":"marks/rose.dds","area":"BODY"}])");
    directory.write(
        "Runes.JSON",
        R"([{"name":"Moon","section":"Magic","texture":"runes/moon.dds","area":"FACE"}])");

    const auto result = loadTattooCatalog(directory.path());

    expect(result.has_value(), "expected catalog load to succeed");
    expect(result->sourceCount == 2, "expected both effective JSON sources");
    expect(result->issues.empty(), "expected valid sources without issues");
    const auto page = result->repository.query();
    expect(page.totalEntries == 2, "expected definitions from every source");
    expect(page.entries[0].sourceFile == "Marks.json", "expected source metadata preserved");
    expect(page.entries[1].sourceFile == "Runes.JSON", "expected stable pack ordering");
    expect(page.entries[0].sourceId.find("marks.json") != std::string::npos,
        "expected stable source identity preserved");
    expect(page.entries[0].packName == "Marks" && page.entries[0].section == "Flowers",
        "expected pack and section to remain distinct");
    const auto& facets = result->repository.facets();
    expect(facets.sources.size() == 2 && facets.sections.size() == 2 &&
            facets.areas.size() == 2,
        "expected loaded metadata to remain filterable");
}

void keepsValidDefinitionsAndAttributesParseIssues() {
    TemporarySourceDirectory directory;
    directory.write(
        "Mixed.json",
        R"([{"name":"Broken","section":"Marks","area":"BODY"},{"name":"Valid","section":"Marks","texture":"mixed/valid.dds","area":"BODY"}])");
    directory.write("Invalid.json", R"({"not":"an array"})");

    const auto result = loadTattooCatalog(directory.path());

    expect(result.has_value(), "expected parse issues not to fail the whole catalog");
    expect(result->sourceCount == 2, "expected attempted source count");
    expect(result->repository.query().totalEntries == 1, "expected valid sibling retained");
    expect(result->issues.size() == 2, "expected both source and entry issues");
    expect(result->issues[0].sourceId.find("invalid.json") != std::string::npos,
        "expected deterministic issue ordering by source");
    expect(result->issues[0].sourceFile == "Invalid.json",
        "expected issue source filename preserved");
    expect(!result->issues[0].entryIndex.has_value(), "expected source-level issue");
    expect(result->issues[1].sourceId.find("mixed.json") != std::string::npos,
        "expected entry issue source attribution");
    expect(result->issues[1].entryIndex == 0, "expected original entry index");
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
    failures += run(
        "loads effective sources into queryable catalog",
        loadsEffectiveSourcesIntoQueryableCatalog);
    failures += run(
        "keeps valid definitions and attributes parse issues",
        keepsValidDefinitionsAndAttributesParseIssues);
    return failures == 0 ? 0 : 1;
}
