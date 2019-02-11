#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

#include <Windows.h>

#include "AudioSourceWrapper.hpp"



AudioSourceWrapper::AudioSourceWrapper(std::shared_ptr<Source> source, unsigned long samplingRate, const std::vector<ChannelInfo>& channelInfo, DataType dataType) :
  mSource(source),
  mSize(source->GetSize()),
  mSamplingRate(samplingRate),
  mChannelInfo(channelInfo),
  mDataType(dataType)
{
  if (mSize % mChannelInfo.size()) {
    throw std::runtime_error(u8"invalid source size");
  }
}


AudioSourceWrapper::AudioSourceWrapper(std::shared_ptr<Source> source, std::shared_ptr<AudioSource> inAudioSource) :
  mSource(source),
  mSize(source->GetSize()),
  mSamplingRate(inAudioSource->GetSamplingRate()),
  mChannelInfo(inAudioSource->GetChannels()),
  mDataType(inAudioSource->GetDataType())
{
  for (std::size_t channelIndex = 0; channelIndex < mChannelInfo.size(); channelIndex++) {
    mChannelInfo[channelIndex] = inAudioSource->GetChannelInfo(channelIndex);
  }
  if (mSize % mChannelInfo.size()) {
    throw std::runtime_error(u8"invalid source size");
  }
}


std::size_t AudioSourceWrapper::GetChannels() const {
  return mChannelInfo.size();
}


AudioSource::ChannelInfo AudioSourceWrapper::GetChannelInfo(std::size_t channelIndex) const {
  if (channelIndex >= mChannelInfo.size()) {
    return ChannelInfo::None;
  }
  return mChannelInfo[channelIndex];
}


AudioSource::DataType AudioSourceWrapper::GetDataType() const {
  return mDataType;
}


std::uint_fast32_t AudioSourceWrapper::GetSamplingRate() const {
  return mSamplingRate;
}


Source::SourceSize AudioSourceWrapper::GetSize() {
  return mSize;
}


NTSTATUS AudioSourceWrapper::Read(SourceOffset offset, std::byte* buffer, std::size_t size, std::size_t* readSize) {
  return mSource->Read(offset, buffer, size, readSize);
}
