#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include <Windows.h>

#include "GeneratePlaylistCue.hpp"
#include "CueSheet.hpp"
#include "EncodingConverter.hpp"

using namespace std::literals;



namespace {
  enum class Type {
    Single,
    Multiple,
  };

  constexpr UINT CodePage = CP_UTF8;
  constexpr bool MarkBOM = true;
  constexpr bool QuoteREM = false;
  const std::wstring gNewLine = L"\r\n"s;



  std::wstring Pad2(unsigned int value) {
    if (value < 10) {
      return L"0"s + std::to_wstring(value);
    }
    return std::to_wstring(value);
  }


  std::wstring FormatTime(unsigned long value) {
    return Pad2(value / 75 / 60) + L":"s + Pad2((value / 75) % 60) + L":"s + Pad2(value % 75);
  }


  template<typename T>
  void WriteREM(std::wstring& wstrData, const T& object, const std::wstring& indent) {
    for (const auto& [key, value] : object.remEntryMap) {
      const bool quote = QuoteREM || value.empty() || key == L"COMPOSER"sv;
      if (quote) {
        wstrData += indent + L"REM "s + key + L" \""s + value + L"\""s + gNewLine;
      } else {
        wstrData += indent + L"REM "s + key + L" "s + value + gNewLine;
      }
    }
    for (const auto& value : object.rems) {
      wstrData += indent + L"REM "s + value + gNewLine;
    }
  }


  std::vector<std::byte> GeneratePlaylistCueCommon(const CueSheet& cueSheet, Type type, const std::wstring& fileType, const std::vector<std::wstring>& audioFilepaths, CueSheet::File::Track::OffsetNumber offsetNumber) {
    std::wstring wstrData;
    if constexpr ((CodePage == CP_UTF7 || CodePage == CP_UTF8) && MarkBOM) {
      wstrData = L"\uFEFF"s;
    }
    if (cueSheet.catalog) {
      wstrData += L"CATALOG "s + cueSheet.catalog.value() + gNewLine;
    }
    if (cueSheet.cdTextFile) {
      wstrData += L"CDTEXTFILE \""s + cueSheet.cdTextFile.value() + L"\""s + gNewLine;
    }
    if (cueSheet.performer) {
      wstrData += L"PERFORMER \""s + cueSheet.performer.value() + L"\""s + gNewLine;
    }
    if (cueSheet.songWriter) {
      wstrData += L"SONGWRITER \""s + cueSheet.songWriter.value() + L"\""s + gNewLine;
    }
    if (cueSheet.title) {
      wstrData += L"TITLE \""s + cueSheet.title.value() + L"\""s + gNewLine;
    }
    WriteREM(wstrData, cueSheet, L""s);

    if (type == Type::Single) {
      wstrData += L"FILE \""s + audioFilepaths[0] + L"\" "s + fileType + gNewLine;
    }

    std::size_t fileIndex = 0;
    for (const auto& file : cueSheet.files) {
      for (const auto& track : file.tracks) {
        if (type == Type::Multiple) {
          wstrData += L"FILE \""s + audioFilepaths.at(fileIndex) + L"\" "s + fileType + gNewLine;
          fileIndex++;
        }

        wstrData += L"  TRACK "s + Pad2(track.number) + L" "s + track.type + gNewLine;
        if (track.flags) {
          wstrData += L"    FLAGS "s + track.flags.value() + gNewLine;
        }
        if (track.isrc) {
          wstrData += L"    ISRC "s + track.isrc.value() + gNewLine;
        }
        if (track.performer) {
          wstrData += L"    PERFORMER \""s + track.performer.value() + L"\""s + gNewLine;
        }
        if (track.songWriter) {
          wstrData += L"    SONGWRITER \""s + track.songWriter.value() + L"\""s + gNewLine;
        }
        if (track.title) {
          wstrData += L"    TITLE \""s + track.title.value() + L"\""s + gNewLine;
        }
        if (type == Type::Single || offsetNumber == CueSheet::File::Track::DefaultOffsetNumber) {
          if (track.postGap) {
            wstrData += L"    POSTGAP "s + FormatTime(track.postGap.value()) + gNewLine;
          }
          if (track.preGap) {
            wstrData += L"    PREGAP "s + FormatTime(track.preGap.value()) + gNewLine;
          }
        }
        WriteREM(wstrData, track, L"    "s);
        if (type == Type::Single) {
          for (const auto&[index, offset] : track.offsetMap) {
            wstrData += L"    INDEX "s + Pad2(index) + L" "s + FormatTime(offset) + gNewLine;
          }
        } else {
          const auto baseOffset = track.offsetMap.count(offsetNumber) ? track.offsetMap.at(offsetNumber) : track.offsetMap.at(CueSheet::File::Track::DefaultOffsetNumber);
          for (const auto& [index, offset] : track.offsetMap) {
            if (offset >= baseOffset) {
              wstrData += L"    INDEX "s + Pad2(index) + L" "s + FormatTime(offset - baseOffset) + gNewLine;
            } else {
              wstrData += L"    REM INDEX "s + Pad2(index) + L" -"s + FormatTime(baseOffset - offset) + gNewLine;
              if (index == CueSheet::File::Track::DefaultOffsetNumber) {
                wstrData += L"    INDEX 01 00:00:00"s + gNewLine;
              }
            }
          }
        }
      }
    }
    const auto strData = ConvertWStringToCodePage(wstrData, CodePage);
    std::vector<std::byte> data(strData.size());
    std::memcpy(data.data(), strData.c_str(), strData.size() * sizeof(char));
    return data;
  }
}



std::vector<std::byte> GeneratePlaylistCue(const CueSheet& cueSheet, std::wstring_view audioFilepath) {
  return GeneratePlaylistCueCommon(cueSheet, Type::Single, L"WAVE"s, std::vector<std::wstring>{std::wstring(audioFilepath)}, 0);
}


std::vector<std::byte> GeneratePlaylistCue(const CueSheet& cueSheet, CueSheet::File::Track::OffsetNumber offsetNumber, const std::vector<std::wstring>& audioFilepaths) {
  return GeneratePlaylistCueCommon(cueSheet, Type::Multiple, L"WAVE"s, audioFilepaths, offsetNumber);
}
