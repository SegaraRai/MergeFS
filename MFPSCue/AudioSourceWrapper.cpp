#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

#include <Windows.h>

#include "AudioSourceWrapper.hpp"



AudioSourceWrapper::AudioSourceWrapper(std::shared_ptr<Source> source, bool compressed, unsigned long samplingRate, const std::vector<ChannelInfo>& channelInfo, DataType dataType) :
  mSource(source),
  mSize(source->GetSize()),
  mCompressed(compressed),
  mSamplingRate(samplingRate),
  mChannelInfo(channelInfo),
  mDataType(dataType)
{
  if (mSize % mChannelInfo.size()) {
    throw std::runtime_error(u8"invalid source size");
  }
}


AudioSourceWrapper::AudioSourceWrapper(std::shared_ptr<Source> source, const AudioSource& audioSource, bool compressed) :
  mSource(source),
  mSize(source->GetSize()),
  mCompressed(compressed),
  mSamplingRate(audioSource.GetSamplingRate()),
  mChannelInfo(audioSource.GetChannels()),
  mDataType(audioSource.GetDataType())
{
  for (std::size_t channelIndex = 0; channelIndex < mChannelInfo.size(); channelIndex++) {
    mChannelInfo[channelIndex] = audioSource.GetChannelInfo(channelIndex);
  }
  if (mSize % mChannelInfo.size()) {
    throw std::runtime_error(u8"invalid source size");
  }
}


AudioSourceWrapper::AudioSourceWrapper(std::shared_ptr<Source> source, const AudioSource& audioSource) :
  AudioSourceWrapper(source, audioSource, audioSource.IsCompressed())
{}


bool AudioSourceWrapper::IsCompressed() const {
  return mCompressed;
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
