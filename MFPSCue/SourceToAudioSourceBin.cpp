#include <cstdint>
#include <memory>
#include <stdexcept>
#include <vector>

#include "SourceToAudioSourceBin.hpp"



SourceToAudioSourceBin::SourceToAudioSourceBin(std::shared_ptr<Source> source) :
  AudioSourceWrapper(source, DSamplingRate, std::vector<AudioSource::ChannelInfo>(SourceToAudioSourceBin::DChannelInfo.cbegin(), SourceToAudioSourceBin::DChannelInfo.cend()), DDataType)
{
  if (source->GetSize() % (sizeof(int16_t) * DChannelInfo.size())) {
    throw std::runtime_error(u8"invalid source size");
  }
}
