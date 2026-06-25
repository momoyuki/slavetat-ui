#pragma once

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <Windows.h>
#include <string>
#include <string_view>
#include <format>
#include <functional>
#include <filesystem>

#include <fstream>
#include <vector>
#include <thread>
#include <unordered_set>
#include <unordered_map>

#include <DirectXTex.h>
// COM headers (pulled in by DirectXTex) define `interface` as `__interface`,
// which breaks slavetats::interface:: namespace.
#ifdef interface
#  undef interface
#endif

#include <nlohmann/json.hpp>
#include <spdlog/sinks/basic_file_sink.h>

using namespace std::literals;

namespace logger = SKSE::log;
