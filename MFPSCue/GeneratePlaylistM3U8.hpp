#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "CueSheet.hpp"


std::vector<std::byte> GeneratePlaylistM3U8(const CueSheet& cueSheet, const std::vector<unsigned long long>& audioSizes, const std::vector<std::wstring>& audioFilepaths);
