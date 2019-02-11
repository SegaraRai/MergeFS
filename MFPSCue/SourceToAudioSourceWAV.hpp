#pragma once

#include <memory>
#include <vector>

#include "AudioSource.hpp"
#include "Source.hpp"


class SourceToAudioSourceWAV : public AudioSource {
  std::shared_ptr<Source> mAudioSource;
  unsigned long mSamplingRate;
  std::vector<ChannelInfo> mChannelInfo;
  DataType mDataType;

public:
  SourceToAudioSourceWAV(std::shared_ptr<Source> source);

  std::size_t GetChannels() const override;
  ChannelInfo GetChannelInfo(std::size_t channelIndex) const override;
  DataType GetDataType() const override;
  std::uint_fast32_t GetSamplingRate() const override;

  SourceSize GetSize() override;
  NTSTATUS Read(SourceOffset offset, std::byte* buffer, std::size_t size, std::size_t* readSize) override;
};
