#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <Windows.h>

#include "AudioSource.hpp"
#include "CueSheet.hpp"
#include "FileSource.hpp"
#include "Source.hpp"


class CueAudioLoader {
public:
  using File = CueSheet::File;
  using Track = CueSheet::File::Track;
  using TrackNumber = CueSheet::File::Track::TrackNumber;
  using OffsetNumber = CueSheet::File::Track::OffsetNumber;

  static constexpr unsigned long CDSamplingRate = 44100;
  static constexpr unsigned long CDChannels = 2;
  static constexpr unsigned long CDBits = 16;
  static constexpr unsigned long CDFramePerSecond = 75;
  static constexpr unsigned long CDSamplesPerFrame = CDSamplingRate / CDFramePerSecond;
  static constexpr unsigned long CDBytesPerSample = CDChannels * CDBits / 8;
  static constexpr unsigned long CDBytesPerSecond = CDBytesPerSample * CDSamplingRate;
  static constexpr unsigned long CDBytesPerFrame = CDBytesPerSecond / CDFramePerSecond;

  static_assert(CDBytesPerFrame == 2352);

private:
  struct AdditionalTrackInfo {
    std::size_t fileIndex;
    std::size_t trackIndexOfFile;
    std::map<OffsetNumber, unsigned long> offsetMap;
    std::map<OffsetNumber, std::shared_ptr<AudioSource>> partialAudioSources;
  };

  std::shared_ptr<FileSource> mCueFileSource;
  CueSheet mCueSheet;
  std::vector<std::shared_ptr<FileSource>> mAudioFileSources;
  std::vector<std::shared_ptr<AudioSource>> mAudioSources;
  std::shared_ptr<AudioSource> mFullAudioSource;
  TrackNumber mFirstTrackNumber;
  TrackNumber mLastTrackNumber;
  std::set<OffsetNumber> mOffsetNumberSet;
  std::map<TrackNumber, AdditionalTrackInfo> mTrackNumberToAdditionalTrackInfoMap;

public:
  CueAudioLoader(LPCWSTR filepath);

  const CueSheet& GetCueSheet() const;
  TrackNumber GetFirstTrackNumber() const;
  TrackNumber GetLastTrackNumber() const;
  const Track* GetTrack(TrackNumber trackNumber) const;
  const File* GetFileForTrack(TrackNumber trackNumber) const;
  std::shared_ptr<AudioSource> GetFullAudioSource() const;
  std::set<OffsetNumber> GetOffsetNumberSet() const;
  std::shared_ptr<AudioSource> GetTrackAudioSource(TrackNumber trackNumber, OffsetNumber offsetIndex) const;
};
