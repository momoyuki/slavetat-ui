#include "repository/TattooSourceScanner.h"

#include <Windows.h>

#include <algorithm>
#include <string_view>

namespace stui::repository {
namespace {

std::expected<std::wstring, std::error_code> lowercaseInvariant(std::wstring_view value) {
    std::wstring lowered(value.size(), L'\0');
    if (LCMapStringEx(
            LOCALE_NAME_INVARIANT,
            LCMAP_LOWERCASE,
            value.data(),
            static_cast<int>(value.size()),
            lowered.data(),
            static_cast<int>(lowered.size()),
            nullptr,
            nullptr,
            0) == 0) {
        return std::unexpected(std::error_code(GetLastError(), std::system_category()));
    }
    return lowered;
}

std::expected<std::string, std::error_code> toUtf8(std::wstring_view value) {
    const int size = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
        nullptr, 0, nullptr, nullptr);
    if (size == 0) {
        return std::unexpected(std::error_code(GetLastError(), std::system_category()));
    }

    std::string utf8(static_cast<std::size_t>(size), '\0');
    if (WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
            utf8.data(), size, nullptr, nullptr) == 0) {
        return std::unexpected(std::error_code(GetLastError(), std::system_category()));
    }
    return utf8;
}

}  // namespace

TattooSourceScanResult scanTattooSources(const std::filesystem::path& directory) {
    std::error_code error;
    std::filesystem::directory_iterator iterator(directory, error);
    if (error) return std::unexpected(error);

    std::vector<TattooSourceFile> sources;
    const std::filesystem::directory_iterator end;

    while (iterator != end) {
        const auto& entry = *iterator;
        const bool regularFile = entry.is_regular_file(error);
        if (error) return std::unexpected(error);

        const auto extension = entry.path().extension().wstring();
        const bool jsonFile = regularFile && CompareStringOrdinal(
            extension.c_str(), static_cast<int>(extension.size()),
            L".json", 5, TRUE) == CSTR_EQUAL;

        if (jsonFile) {
            const auto filename = entry.path().filename().wstring();
            const auto loweredFilename = lowercaseInvariant(filename);
            if (!loweredFilename) return std::unexpected(loweredFilename.error());

            const auto sourceFile = toUtf8(filename);
            if (!sourceFile) return std::unexpected(sourceFile.error());
            const auto normalizedFilename = toUtf8(*loweredFilename);
            if (!normalizedFilename) return std::unexpected(normalizedFilename.error());
            const auto packName = toUtf8(entry.path().stem().wstring());
            if (!packName) return std::unexpected(packName.error());

            sources.push_back(TattooSourceFile{
                .sourceId = std::string(kTattooSourceIdPrefix) + *normalizedFilename,
                .sourceFile = *sourceFile,
                .packName = *packName,
                .effectivePath = entry.path(),
            });
        }

        iterator.increment(error);
        if (error) return std::unexpected(error);
    }

    std::ranges::sort(sources, {}, &TattooSourceFile::sourceId);
    return sources;
}

}  // namespace stui::repository
