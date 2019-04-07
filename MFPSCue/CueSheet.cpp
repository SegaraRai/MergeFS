#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "CueSheet.hpp"

using namespace std::literals;


namespace {
  // supports only LF and CRLF
  class LineReader {
    std::wstring_view data;
    std::size_t offset;

  public:
    LineReader(std::wstring_view data) :
      data(data),
      offset(0)
    {}

    std::optional<std::wstring_view> ReadLine() {
      if (offset >= data.size()) {
        return std::nullopt;
      }
      const auto rest = data.substr(offset);
      const auto firstLfPos = rest.find_first_of(L'\n');
      if (firstLfPos == std::wstring_view::npos) {
        offset = data.size();
        return rest;
      }
      const bool hasCr = firstLfPos > 0 && rest.at(firstLfPos - 1) == L'\r';
      offset += firstLfPos + 1;
      return rest.substr(0, hasCr ? firstLfPos - 1 : firstLfPos);
    }
  };


  std::wstring_view TrimDoubleQuotes(std::wstring_view args) {
    if (args.size() < 2 || args[0] != L'"' || args[args.size() - 1] != L'"') {
      return args;
    }
    return args.substr(1, args.size() - 2);
  }


  unsigned long ParseTime(std::wstring_view time) {
    const auto firstColon = time.find_first_of(L':');
    const auto lastColon = time.find_last_of(L':');
    const auto minutes = std::stoul(std::wstring(time.substr(0, firstColon)));
    const auto seconds = std::stoul(std::wstring(time.substr(firstColon + 1, lastColon - firstColon - 1)));
    const auto frames = std::stoul(std::wstring(time.substr(lastColon + 1)));
    if (seconds >= 60 || frames >= 75) {
      throw std::runtime_error(u8"invalid time format");
    }
    return (minutes * 60 + seconds) * 75 + frames;
  }


  template<typename T>
  void ParseREM(T& object, std::wstring_view args) {
    const auto firstSpacePos = args.find_first_of(L' ');
    if (firstSpacePos == std::wstring_view::npos) {
      object.rems.emplace_back(args);
      return;
    }

    auto remCommand = args.substr(0, firstSpacePos);
    auto remArgs = args.substr(firstSpacePos + 1);
    object.remEntryMap.emplace(remCommand, TrimDoubleQuotes(remArgs));
  }
}


CueSheet CueSheet::ParseCueSheet(const std::wstring& data) {
  using Track = File::Track;

  LineReader lineReader(data);
  CueSheet cueSheet;
  File* lastFile = nullptr;
  Track* lastTrack = nullptr;

  // https://wiki.hydrogenaud.io/index.php?title=Cue_sheet
  std::unordered_map<std::wstring, std::function<void(std::wstring_view)>> commands{
    {L"CATALOG"s, [&](std::wstring_view args) {
      cueSheet.catalog.emplace(TrimDoubleQuotes(args));
    }},
    {L"CDTEXTFILE"s, [&](std::wstring_view args) {
      cueSheet.cdTextFile.emplace(TrimDoubleQuotes(args));
    }},
    {L"FILE"s, [&](std::wstring_view args) {
      if (args.size() < 1) {
        throw std::runtime_error(u8"invalid FILE arguments");
      }
      std::wstring filename;
      std::wstring type;
      if (args[0] == L'"') {
        const auto lastDoubleQuotePos = args.find_last_of(L'"');
        if (lastDoubleQuotePos == 0 || lastDoubleQuotePos == std::wstring_view::npos) {
          throw std::runtime_error(u8"invalid FILE arguments");
        }
        if (lastDoubleQuotePos + 2 > args.size()) {
          throw std::runtime_error(u8"invalid FILE arguments");
        }
        filename = args.substr(1, lastDoubleQuotePos - 1);
        type = args.substr(lastDoubleQuotePos + 2);
      } else {
        const auto firstSpacePos = args.find_first_of(L' ');
        if (firstSpacePos == std::wstring_view::npos) {
          throw std::runtime_error(u8"invalid FILE arguments");
        }
        filename = args.substr(0, firstSpacePos);
        type = args.substr(firstSpacePos + 1);
      }
      cueSheet.files.emplace_back(File{
        std::wstring(filename),
        std::wstring(type),
      });
      lastFile = &cueSheet.files.at(cueSheet.files.size() - 1);
      lastTrack = nullptr;
    }},
    {L"FLAGS"s, [&](std::wstring_view args) {
      if (!lastTrack) {
        throw std::runtime_error(u8"no current TRACK");
      }
      lastTrack->flags.emplace(TrimDoubleQuotes(args));
    }},
    {L"INDEX"s, [&](std::wstring_view args) {
      if (!lastTrack) {
        throw std::runtime_error(u8"no current TRACK");
      }
      const auto firstSpacePos = args.find_first_of(L' ');
      if (firstSpacePos == std::wstring_view::npos) {
        throw std::runtime_error(u8"invalid TRACK arguments");
      }
      const auto index = std::stoul(std::wstring(args.substr(0, firstSpacePos)));
      const auto offset = ParseTime(args.substr(firstSpacePos + 1));
      lastTrack->offsetMap.emplace(index, offset);
    }},
    {L"ISRC"s, [&](std::wstring_view args) {
      if (!lastTrack) {
        throw std::runtime_error(u8"no current TRACK");
      }
      lastTrack->isrc.emplace(TrimDoubleQuotes(args));
    }},
    {L"PERFORMER"s, [&](std::wstring_view args) {
      if (lastTrack) {
        lastTrack->performer.emplace(TrimDoubleQuotes(args));
      } else {
        cueSheet.performer.emplace(TrimDoubleQuotes(args));
      }
    }},
    {L"POSTGAP"s, [&](std::wstring_view args) {
      if (!lastTrack) {
        throw std::runtime_error(u8"no current TRACK");
      }
      lastTrack->postGap.emplace(ParseTime(args));
    }},
    {L"PREGAP"s, [&](std::wstring_view args) {
      if (!lastTrack) {
        throw std::runtime_error(u8"no current TRACK");
      }
      lastTrack->preGap.emplace(ParseTime(args));
    }},
    {L"REM"s, [&](std::wstring_view args) {
      if (lastTrack) {
        ParseREM(*lastTrack, args);
      } else {
        ParseREM(cueSheet, args);
      }
    }},
    {L"SONGWRITER"s, [&](std::wstring_view args) {
      if (lastTrack) {
        lastTrack->songWriter.emplace(TrimDoubleQuotes(args));
      } else {
        cueSheet.songWriter.emplace(TrimDoubleQuotes(args));
      }
    }},
    {L"TITLE"s, [&](std::wstring_view args) {
      if (lastTrack) {
        lastTrack->title.emplace(TrimDoubleQuotes(args));
      } else {
        cueSheet.title.emplace(TrimDoubleQuotes(args));
      }
    }},
    {L"TRACK"s, [&](std::wstring_view args) {
      if (!lastFile) {
        throw std::runtime_error(u8"no current FILE");
      }
      const auto firstSpacePos = args.find_first_of(L' ');
      if (firstSpacePos == std::wstring_view::npos) {
        throw std::runtime_error(u8"invalid TRACK arguments");
      }
      const auto index = std::stoul(std::wstring(args.substr(0, firstSpacePos)));
      const auto type = args.substr(firstSpacePos + 1);
      lastFile->tracks.emplace_back(Track{
        index,
        std::wstring(type),
      });
      lastTrack = &lastFile->tracks.at(lastFile->tracks.size() - 1);
    }},
  };

  while (auto lineN = lineReader.ReadLine()) {
    auto line = lineN.value();
    if (line.empty()) {
      continue;
    }

    const auto numSpaces = line.find_first_not_of(L' ');
    if (numSpaces == std::string_view::npos) {
      continue;
    }

    const auto unindentLine = line.substr(numSpaces);

    const auto firstSpacePos = unindentLine.find_first_of(L' ');
    auto command = unindentLine.substr(0, firstSpacePos);
    auto args = firstSpacePos == std::wstring_view::npos ? L""sv : unindentLine.substr(firstSpacePos + 1);

    commands.at(std::wstring(command))(args);
  }

  return cueSheet;
}


std::optional<std::pair<std::size_t, std::size_t>> CueSheet::FindTrack(File::Track::TrackNumber trackNumber) const {
  for (std::size_t fileIndex = 0; fileIndex < files.size(); fileIndex++) {
    const auto& file = files[fileIndex];
    const auto& tracks = file.tracks;
    for (std::size_t trackIndex = 0; trackIndex < tracks.size(); trackIndex++) {
      const auto& track = tracks[trackIndex];
      if (track.number == trackNumber) {
        return std::make_optional<std::pair<std::size_t, std::size_t>>(fileIndex, trackIndex);
      }
    }
  }
  return std::nullopt;
}
