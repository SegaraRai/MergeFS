#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>


struct CueSheet {
  struct File {
    struct Track {
      using TrackNumber = unsigned int;
      using OffsetNumber = unsigned int;

      static constexpr OffsetNumber DefaultOffsetNumber = 1;

      TrackNumber number;
      std::wstring type;
      std::optional<std::wstring> flags;
      std::optional<std::wstring> isrc;
      std::optional<std::wstring> performer;
      std::optional<std::wstring> songWriter;
      std::optional<std::wstring> title;
      std::optional<unsigned long> postGap;
      std::optional<unsigned long> preGap;
      std::map<std::wstring, std::wstring> remEntryMap;
      std::vector<std::wstring> rems;
      std::map<OffsetNumber, unsigned long> offsetMap;
    };

    std::wstring filename;
    std::wstring type;
    std::vector<Track> tracks;
  };

  std::optional<std::wstring> catalog;
  std::optional<std::wstring> cdTextFile;
  std::optional<std::wstring> performer;
  std::optional<std::wstring> songWriter;
  std::optional<std::wstring> title;
  std::map<std::wstring, std::wstring> remEntryMap;
  std::vector<std::wstring> rems;
  std::vector<File> files;

  static CueSheet ParseCueSheet(const std::wstring& data);

  std::optional<std::pair<std::size_t, std::size_t>> FindTrack(File::Track::TrackNumber trackNumber) const;
};
