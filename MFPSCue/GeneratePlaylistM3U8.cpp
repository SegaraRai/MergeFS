#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <Windows.h>

#include "GeneratePlaylistM3U8.hpp"
#include "CueSheet.hpp"
#include "EncodingConverter.hpp"

using namespace std::literals;



namespace {
  const std::wstring gNewLine = L"\r\n"s;
}



std::vector<std::byte> GeneratePlaylistM3U8(const CueSheet& cueSheet, const std::vector<unsigned long long>& audioSizes, const std::vector<std::wstring>& audioFilepaths) {
  std::wstring wstrData;
  wstrData += L"#EXTM3U"s + gNewLine;
  std::size_t index = 0;
  for (const auto& file : cueSheet.files) {
    for (const auto& track : file.tracks) {
      wstrData += gNewLine;
      wstrData += L"#EXTINF:"s + std::to_wstring(static_cast<double>(audioSizes.at(index) / 4) / 44100.) + L","s + track.title.value_or(L""s) + gNewLine;
      wstrData += audioFilepaths.at(index) + gNewLine;
      index++;
    }
  }
  const auto strData = ConvertWStringToCodePage(wstrData, CP_UTF8);
  std::vector<std::byte> data(strData.size());
  std::memcpy(data.data(), strData.c_str(), strData.size() * sizeof(char));
  return data;
}
