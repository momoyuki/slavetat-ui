#include "repository/TattooSourceScanner.h"

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
using stui::repository::scanTattooSources;

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
        m_path = fs::temp_directory_path() / ("slavetats-ui-source-scan-" + std::to_string(unique));
        fs::create_directories(m_path);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        fs::remove_all(m_path, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    [[nodiscard]] const fs::path& path() const noexcept { return m_path; }

private:
    fs::path m_path;
};

void expect(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}

void writeFile(const fs::path& path) {
    std::ofstream stream(path);
    stream << "[]";
}

void scannerReturnsOnlyDirectJsonFilesWithStableIdentity() {
    TemporaryDirectory directory;
    writeFile(directory.path() / "PackB.JSON");
    writeFile(directory.path() / "PackA.json");
    writeFile(directory.path() / "notes.txt");
    fs::create_directory(directory.path() / "nested");
    writeFile(directory.path() / "nested" / "Ignored.json");

    const auto result = scanTattooSources(directory.path());

    expect(result.has_value(), "expected source scan to succeed");
    expect(result->size() == 2, "expected only direct JSON files");
    expect((*result)[0].sourceId == "textures/actors/character/slavetats/packa.json", "expected normalized first source ID");
    expect((*result)[0].sourceFile == "PackA.json", "expected original source filename");
    expect((*result)[0].packName == "PackA", "expected filename stem as pack name");
    expect((*result)[0].effectivePath == directory.path() / "PackA.json", "expected effective source path");
    expect((*result)[1].sourceId == "textures/actors/character/slavetats/packb.json", "expected sources sorted by normalized ID");
}

void scannerNormalizesUnicodeFilenameToLowercaseUtf8() {
    TemporaryDirectory directory;
    writeFile(directory.path() / L"\u00C4Pack.JSON");

    const auto result = scanTattooSources(directory.path());

    expect(result.has_value(), "expected Unicode source scan to succeed");
    expect(result->size() == 1, "expected one Unicode JSON source");
    expect(
        (*result)[0].sourceId == "textures/actors/character/slavetats/\xC3\xA4pack.json",
        "expected invariant lowercase UTF-8 source ID");
}

void missingDirectoryReturnsFilesystemError() {
    TemporaryDirectory parent;

    const auto result = scanTattooSources(parent.path() / "missing");

    expect(!result.has_value(), "expected a missing source directory to fail");
    expect(static_cast<bool>(result.error()), "expected a filesystem error code");
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
    failures += run("scanner returns direct JSON files with stable identity", scannerReturnsOnlyDirectJsonFilesWithStableIdentity);
    failures += run("scanner normalizes Unicode filename", scannerNormalizesUnicodeFilenameToLowercaseUtf8);
    failures += run("missing directory returns filesystem error", missingDirectoryReturnsFilesystemError);
    return failures == 0 ? 0 : 1;
}
