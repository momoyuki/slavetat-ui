#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace stui::core {

struct TattooEntry {
    std::int32_t runtimeHandle{0};
    std::string sourceId;
    std::string sourceFile;
    std::string packName;
    std::string domain{"default"};
    std::string section;
    std::string name;
    std::string texturePath;
    std::string area;
    std::int32_t slot{-1};
    std::int32_t color{0xFFFFFF};
    bool locked{false};
    float alpha{1.0F};

    bool operator==(const TattooEntry&) const = default;
};

enum class ServiceErrorCode {
    slaveTatsUnavailable,
    jContainersUnavailable,
    queryAvailableFailed,
};

struct ServiceError {
    ServiceErrorCode code;
    std::string message;
};

using TattooQueryResult = std::expected<std::vector<TattooEntry>, ServiceError>;

}  // namespace stui::core
