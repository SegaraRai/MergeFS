#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <Windows.h>

#include "AudioSource.hpp"
#include "Source.hpp"


class AudioSourceWrapper : public AudioSource {
  std::shared_ptr<Source> mSource;
  SourceSize mSize;
  bool mCompressed;
  unsigned long mSamplingRate;
  std::vector<ChannelInfo> mChannelInfo;
  DataType mDataType;

public:
  AudioSourceWrapper(std::shared_ptr<Source> source, bool compressed, unsigned long samplingRate, const std::vector<ChannelInfo>& channelInfo, DataType dataType);
  AudioSourceWrapper(std::shared_ptr<Source> source, const AudioSource& audioSource, bool compressed);
  AudioSourceWrapper(std::shared_ptr<Source> source, const AudioSource& audioSource);
  virtual ~AudioSourceWrapper() = default;

  bool IsCompressed() const override;
  std::size_t GetChannels() const override;
  ChannelInfo GetChannelInfo(std::size_t channelIndex) const override;
  DataType GetDataType() const override;
  std::uint_fast32_t GetSamplingRate() const override;

  SourceSize GetSize() override;
  NTSTATUS Read(SourceOffset offset, std::byte* buffer, std::size_t size, std::size_t* readSize) override;
};
