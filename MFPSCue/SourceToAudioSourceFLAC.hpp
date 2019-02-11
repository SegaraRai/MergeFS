#pragma once

#include <memory>
#include <mutex>
#include <vector>

#include "AudioSource.hpp"
#include "Source.hpp"


class SourceToAudioSourceFLAC : public AudioSource {
  std::mutex mMutex;
  bool mError;
  std::shared_ptr<void> mDecoder;
  unsigned long mSamplingRate;
  std::vector<ChannelInfo> mChannelInfo;
  DataType mDataType;
  SourceSize mTotalSamples;
  SourceSize mTotalSize;
  bool mLastFLACAvailable;
  std::unique_ptr<std::byte[]> mLastFLACFrame;
  std::unique_ptr<std::byte[]> mLastFLACBufferData;
  void* mLastFLACBufferPointers[8];

public:
  SourceToAudioSourceFLAC(std::shared_ptr<Source> source);

  bool IsCompressed() const override;
  std::size_t GetChannels() const override;
  ChannelInfo GetChannelInfo(std::size_t channelIndex) const override;
  DataType GetDataType() const override;
  std::uint_fast32_t GetSamplingRate() const override;

  SourceSize GetSize() override;
  NTSTATUS Read(SourceOffset offset, std::byte* buffer, std::size_t size, std::size_t* readSize) override;
};
