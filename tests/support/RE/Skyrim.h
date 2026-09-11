#pragma once

#include <string>

namespace RE {

class TESForm {};

class BSFixedString {
public:
    BSFixedString() = default;
    BSFixedString(const char* value) : m_value(value ? value : "") {}

    [[nodiscard]] const char* c_str() const noexcept { return m_value.c_str(); }

private:
    std::string m_value;
};

}  // namespace RE
