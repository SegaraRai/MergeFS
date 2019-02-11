#pragma once

#include <array>
#include <memory>

#include "AudioSource.hpp"
#include "AudioSourceWrapper.hpp"
#include "Source.hpp"


class SourceToAudioSourceBin : public AudioSourceWrapper {
  static constexpr bool DCompressed = false;
  static constexpr unsigned int DNumChannels = 2;
  static constexpr std::array<ChannelInfo, 2> DChannelInfo{
    ChannelInfo::Left,
    ChannelInfo::Right,
  };
  static constexpr DataType DDataType = DataType::Int16;
  static constexpr unsigned long DSamplingRate = 44100;

public:
  SourceToAudioSourceBin(std::shared_ptr<Source> source);
};
