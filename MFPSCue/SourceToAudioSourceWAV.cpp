#include <dokan/dokan.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>

#include "SourceToAudioSourceWAV.hpp"
#include "PartialSource.hpp"



namespace {
  template<typename T>
  T ReadData(Source& source, Source::SourceOffset offset) {
    T data;
    std::size_t readSize = 0;
    if (source.Read(offset, reinterpret_cast<std::byte*>(&data), sizeof(data), &readSize) != STATUS_SUCCESS || readSize != sizeof(data)) {
      throw std::runtime_error("read failed");
    }
    return data;
  }
}


SourceToAudioSourceWAV::SourceToAudioSourceWAV(std::shared_ptr<Source> source) {
  constexpr std::uint32_t SignatureRIFF = 0x46464952;
  constexpr std::uint32_t SignatureWAVE = 0x45564157;
  constexpr std::uint32_t Signaturefmt  = 0x20746D66;
  constexpr std::uint32_t Signaturedata = 0x61746164;

  const auto sourceSize = source->GetSize();
  if (sourceSize < 12) {
    throw std::runtime_error("invalid wav file");
  }

  if (ReadData<std::uint32_t>(*source, 0) != SignatureRIFF) {
    throw std::runtime_error("invalid wav file");
  }

  if (ReadData<std::uint32_t>(*source, 4) != sourceSize - 8) {
    throw std::runtime_error("invalid wav file");
  }

  if (ReadData<std::uint32_t>(*source, 8) != SignatureWAVE) {
    throw std::runtime_error("invalid wav file");
  }

  SourceOffset audioDataOffset = 0;
  SourceSize audioDataSize = 0;

  std::uint32_t currentChunkSignature = 0;
  std::uint32_t currentChunkSize = 0;
  SourceOffset offset = 12;
  do {
    currentChunkSignature = ReadData<std::uint32_t>(*source, offset);
    offset += 4;
    currentChunkSize = ReadData<std::uint32_t>(*source, offset);
    offset += 4;
    switch (currentChunkSignature) {
      case Signaturefmt:
      {
        if (!mChannelInfo.empty()) {
          throw std::runtime_error("too many fmt chunk");
        }

        if (currentChunkSize != 16) {
          throw std::runtime_error("unsupported format");
        }
        auto tempOffset = offset;
        const auto formatId = ReadData<std::uint16_t>(*source, tempOffset);
        tempOffset += sizeof(formatId);
        const auto numChannels = ReadData<std::uint16_t>(*source, tempOffset);
        tempOffset += sizeof(numChannels);
        const auto samplingRate = ReadData<std::uint32_t>(*source, tempOffset);
        tempOffset += sizeof(samplingRate);
        const auto dataRate = ReadData<std::uint32_t>(*source, tempOffset);
        tempOffset += sizeof(dataRate);
        const auto blockSize = ReadData<std::uint16_t>(*source, tempOffset);
        tempOffset += sizeof(blockSize);
        const auto bitsPerSample = ReadData<std::uint16_t>(*source, tempOffset);
        tempOffset += sizeof(bitsPerSample);
        if (formatId != 1) {
          throw std::runtime_error("unsupported format");
        }
        mSamplingRate = samplingRate;
        mDataType = bitsPerSample == 16 ? DataType::Int16 : DataType::Other;
        if (numChannels == 2) {
          mChannelInfo.emplace_back(ChannelInfo::Left);
          mChannelInfo.emplace_back(ChannelInfo::Right);
        } else {
          mChannelInfo.resize(numChannels);
          for (unsigned int i = 0; i < numChannels; i++) {
            mChannelInfo[i] = ChannelInfo::Other;
          }
        }
        break;
      }

      case Signaturedata:
        if (audioDataOffset) {
          throw std::runtime_error("too many data chunk");
        }
        audioDataOffset = offset;
        audioDataSize = currentChunkSize;
        break;
    }
    offset += currentChunkSize;
    if (offset > sourceSize) {
      throw std::runtime_error("invalid wav file");
    }
  } while (offset != sourceSize);

  if (mChannelInfo.empty()) {
    throw std::runtime_error("no fmt chunk");
  }

  if (!audioDataOffset) {
    throw std::runtime_error("no data chunk");
  }

  mAudioSource = std::make_shared<PartialSource>(source, audioDataOffset, audioDataSize);
}


bool SourceToAudioSourceWAV::IsCompressed() const {
  return false;
}


std::size_t SourceToAudioSourceWAV::GetChannels() const {
  return mChannelInfo.size();
}


AudioSource::ChannelInfo SourceToAudioSourceWAV::GetChannelInfo(std::size_t channelIndex) const {
  if (channelIndex >= mChannelInfo.size()) {
    return ChannelInfo::None;
  }
  return mChannelInfo[channelIndex];
}


AudioSource::DataType SourceToAudioSourceWAV::GetDataType() const {
  return mDataType;
}


std::uint_fast32_t SourceToAudioSourceWAV::GetSamplingRate() const {
  return mSamplingRate;
}


Source::SourceSize SourceToAudioSourceWAV::GetSize() {
  return mAudioSource->GetSize();
}


NTSTATUS SourceToAudioSourceWAV::Read(SourceOffset offset, std::byte* buffer, std::size_t size, std::size_t* readSize) {
  return mAudioSource->Read(offset, buffer, size, readSize);
}
