#pragma once

#include <memory>
#include <vector>

#include "AudioSource.hpp"
#include "CueSheet.hpp"
#include "Source.hpp"


class AudioSourceToSourceWAV : public Source {
  std::shared_ptr<Source> mSource;

public:
  AudioSourceToSourceWAV(std::shared_ptr<AudioSource> inAudioSource, const CueSheet* cueSheet, CueSheet::File::Track::TrackNumber trackNumber);
  AudioSourceToSourceWAV(std::shared_ptr<AudioSource> inAudioSource);

  SourceSize GetSize() override;
  NTSTATUS Read(SourceOffset offset, std::byte* buffer, std::size_t size, std::size_t* readSize) override;
};
