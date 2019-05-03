#define DISABLE_LOCKING_CUE_SHEET

#pragma comment(lib, "Shlwapi.lib")

#include <dokan/dokan.h>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <Windows.h>
#include <shlwapi.h>

#include "../Util/RealFs.hpp"

#include "CueAudioLoader.hpp"
#include "AudioSource.hpp"
#include "AudioSourceWrapper.hpp"
#include "EncodingConverter.hpp"
#include "FileSource.hpp"
#include "MergedSource.hpp"
#include "OnMemorySourceWrapper.hpp"
#include "PartialSource.hpp"
#include "Source.hpp"
#include "TransformToAudioSource.hpp"
#include "Util.hpp"

using namespace std::literals;



namespace {
  const std::vector<AudioSource::ChannelInfo> gCDChannelInfo{
    AudioSource::ChannelInfo::Left,
    AudioSource::ChannelInfo::Right,
  };

  constexpr auto CDDataType = AudioSource::DataType::Int16;
}



CueAudioLoader::CueAudioLoader(LPCWSTR filepath, ExtractToMemory extractToMemory) {
  const auto fullCueFilepath = util::rfs::ToAbsoluteFilepath(filepath);
  const auto baseDirectoryPath = util::rfs::GetParentPath(fullCueFilepath);

  mCueFileSource = std::make_shared<FileSource>(fullCueFilepath.c_str());
  auto rawCueSheetData = std::make_unique<std::byte[]>(static_cast<std::size_t>(mCueFileSource->GetSize()));
  std::size_t readSize = 0;
  if (mCueFileSource->Read(0, rawCueSheetData.get(), static_cast<std::size_t>(mCueFileSource->GetSize()), &readSize) != STATUS_SUCCESS) {
    throw std::runtime_error(u8"failed to read cue file");
  }
  if (readSize != mCueFileSource->GetSize()) {
    throw std::runtime_error(u8"failed to read cue file");
  }
  auto wCueSheetData = ConvertFileContentToWString(rawCueSheetData.get(), static_cast<std::size_t>(mCueFileSource->GetSize()));

#ifdef DISABLE_LOCKING_CUE_SHEET
  mCueFileSource.reset();
#endif

  mCueSheet = CueSheet::ParseCueSheet(wCueSheetData);
  
  const std::wstring directoryPrefix = std::wstring(baseDirectoryPath) + L"\\"s;

  for (std::size_t fileIndex = 0; fileIndex < mCueSheet.files.size(); fileIndex++) {
    const auto& file = mCueSheet.files[fileIndex];

    auto audioFilepath = PathIsRelativeW(file.filename.c_str()) ? directoryPrefix + file.filename : file.filename;
    
    auto audioFileSource = std::make_shared<FileSource>(audioFilepath.c_str());

    // TODO: check extension for bin file
    auto audioSource = TransformToAudioSource(audioFileSource, file.type == L"BINARY"sv);
    
    if (!audioSource) {
      throw std::runtime_error(u8"cannot load audio");
    }
    if (audioSource->GetChannels() != CDChannels || audioSource->GetSamplingRate() != CDSamplingRate) {
      throw std::runtime_error(u8"invalid audio format");
    }
    if (audioSource->GetChannelInfo(0) != AudioSource::ChannelInfo::Left || audioSource->GetChannelInfo(1) != AudioSource::ChannelInfo::Right) {
      throw std::runtime_error(u8"unsupported channel layout");
    }
    if (audioSource->GetDataType() != AudioSource::DataType::Int16) {
      throw std::runtime_error(u8"unsupported data type");
    }

    if (extractToMemory == ExtractToMemory::Always || (extractToMemory == ExtractToMemory::Compressed && audioSource->IsCompressed())) {
      audioSource = std::make_shared<AudioSourceWrapper>(std::make_shared<OnMemorySourceWrapper>(audioSource), *audioSource, false);
    }

    mAudioFileSources.emplace_back(audioFileSource);
    mAudioSources.emplace_back(audioSource);

    for (std::size_t trackIndex = 0; trackIndex < file.tracks.size(); trackIndex++) {
      const auto& track = file.tracks[trackIndex];

      if (!track.offsetMap.count(CueSheet::File::Track::DefaultOffsetNumber)) {
        throw std::runtime_error(u8"missing INDEX 01 offset");
      }

      mTrackNumberToAdditionalTrackInfoMap.emplace(track.number, AdditionalTrackInfo{
        fileIndex,
        trackIndex,
        track.offsetMap,
      });
    }
  }

  {
    bool hasCompressedAudio = false;

    std::vector<std::shared_ptr<Source>> sources(mAudioSources.size());
    for (std::size_t sourceIndex = 0; sourceIndex < mAudioSources.size(); sourceIndex++) {
      if (mAudioSources[sourceIndex]->IsCompressed()) {
        hasCompressedAudio = true;
      }
      sources[sourceIndex] = mAudioSources[sourceIndex];
    }

    mFullAudioSource = std::make_shared<AudioSourceWrapper>(std::make_shared<MergedSource>(sources), hasCompressedAudio, CDSamplingRate, gCDChannelInfo, CDDataType);
  }

  if (mTrackNumberToAdditionalTrackInfoMap.empty()) {
    throw std::runtime_error(u8"no track provided");
  }

  mFirstTrackNumber = mTrackNumberToAdditionalTrackInfoMap.cbegin()->first;
  mLastTrackNumber = (--mTrackNumberToAdditionalTrackInfoMap.cend())->first;

  if (mTrackNumberToAdditionalTrackInfoMap.size() != mLastTrackNumber - mFirstTrackNumber + 1) {
    throw std::runtime_error(u8"missing track");
  }

  for (const auto& [trackNumber, additionalTrackInfo] : mTrackNumberToAdditionalTrackInfoMap) {
    for (const auto& [offsetNumber, offset] : additionalTrackInfo.offsetMap) {
      mOffsetNumberSet.emplace(offsetNumber);
    }
  }

  for (TrackNumber trackNumber = mFirstTrackNumber; trackNumber <= mLastTrackNumber; trackNumber++) {
    auto& additionalTrackInfo = mTrackNumberToAdditionalTrackInfoMap.at(trackNumber);
    
    for (const auto& offsetNumber : mOffsetNumberSet) {
      Source::SourceOffset currentTrackOffset;
      if (additionalTrackInfo.offsetMap.count(offsetNumber)) {
        currentTrackOffset = additionalTrackInfo.offsetMap.at(offsetNumber);
      } else {
        currentTrackOffset = additionalTrackInfo.offsetMap.at(CueSheet::File::Track::DefaultOffsetNumber);
      }

      Source::SourceOffset nextTrackOffset;
      if (trackNumber == mLastTrackNumber || additionalTrackInfo.fileIndex != mTrackNumberToAdditionalTrackInfoMap.at(trackNumber + 1).fileIndex) {
        nextTrackOffset = mAudioSources.at(additionalTrackInfo.fileIndex)->GetSize() / CDBytesPerFrame;
      } else {
        const auto& nextTrackAdditionalTrackInfo = mTrackNumberToAdditionalTrackInfoMap.at(trackNumber + 1);
        if (nextTrackAdditionalTrackInfo.offsetMap.count(offsetNumber)) {
          nextTrackOffset = nextTrackAdditionalTrackInfo.offsetMap.at(offsetNumber);
        } else {
          nextTrackOffset = nextTrackAdditionalTrackInfo.offsetMap.at(CueSheet::File::Track::DefaultOffsetNumber);
        }
      }

      if (currentTrackOffset > nextTrackOffset) {
        throw std::runtime_error(u8"invalid track offset provided");
      }

      auto currentAudioSource = mAudioSources.at(additionalTrackInfo.fileIndex);

      additionalTrackInfo.partialAudioSources.emplace(offsetNumber, std::make_shared<AudioSourceWrapper>(std::make_shared<PartialSource>(currentAudioSource, currentTrackOffset * CDBytesPerFrame, (nextTrackOffset - currentTrackOffset) * CDBytesPerFrame), *currentAudioSource));
    }
  }
}


const CueSheet& CueAudioLoader::GetCueSheet() const {
  return mCueSheet;
}


CueAudioLoader::TrackNumber CueAudioLoader::GetFirstTrackNumber() const {
  return mFirstTrackNumber;
}


CueAudioLoader::TrackNumber CueAudioLoader::GetLastTrackNumber() const {
  return mLastTrackNumber;
}


const CueAudioLoader::Track* CueAudioLoader::GetTrack(TrackNumber trackNumber) const {
  if (!mTrackNumberToAdditionalTrackInfoMap.count(trackNumber)) {
    return nullptr;
  }
  const auto& additionalTrackInfo = mTrackNumberToAdditionalTrackInfoMap.at(trackNumber);
  return &mCueSheet.files.at(additionalTrackInfo.fileIndex).tracks.at(additionalTrackInfo.trackIndexOfFile);
}


const CueAudioLoader::File* CueAudioLoader::GetFileForTrack(TrackNumber trackNumber) const {
  if (!mTrackNumberToAdditionalTrackInfoMap.count(trackNumber)) {
    return nullptr;
  }
  const auto& additionalTrackInfo = mTrackNumberToAdditionalTrackInfoMap.at(trackNumber);
  return &mCueSheet.files.at(additionalTrackInfo.fileIndex);
}


std::shared_ptr<AudioSource> CueAudioLoader::GetFullAudioSource() const {
  return mFullAudioSource;
}


std::set<CueAudioLoader::OffsetNumber> CueAudioLoader::GetOffsetNumberSet() const {
  return mOffsetNumberSet;
}


std::shared_ptr<AudioSource> CueAudioLoader::GetTrackAudioSource(TrackNumber trackNumber, OffsetNumber offsetIndex) const {
  if (!mTrackNumberToAdditionalTrackInfoMap.count(trackNumber)) {
    return nullptr;
  }
  const auto& additionalTrackInfo = mTrackNumberToAdditionalTrackInfoMap.at(trackNumber);
  if (!additionalTrackInfo.partialAudioSources.count(offsetIndex)) {
    return nullptr;
  }
  return additionalTrackInfo.partialAudioSources.at(offsetIndex);
}

