#pragma once

#include <cstddef>
#include <vector>

#include "CueSheet.hpp"


std::vector<std::byte> GenerateTagID3(const CueSheet& cueSheet, CueSheet::File::Track::TrackNumber trackNumber);
