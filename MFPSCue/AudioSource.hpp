#pragma once

#include <memory>

#include "Source.hpp"


class AudioSource : public Source {
public:
  enum class ChannelInfo {
    None,
    Other,
    Left,
    Right,
  };

  enum class DataType {
    Other,
    Int16,
  };

  virtual ~AudioSource() = default;

  virtual std::size_t GetChannels() const = 0;
  virtual ChannelInfo GetChannelInfo(std::size_t channelIndex) const = 0;
  virtual DataType GetDataType() const = 0;
  virtual std::uint_fast32_t GetSamplingRate() const = 0;
};
