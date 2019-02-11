#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "CueSheet.hpp"


std::vector<std::byte> GeneratePlaylistCue(const CueSheet& cueSheet, std::wstring_view audioFilepath);
std::vector<std::byte> GeneratePlaylistCue(const CueSheet& cueSheet, CueSheet::File::Track::OffsetNumber offsetNumber, const std::vector<std::wstring>& audioFilepaths);
