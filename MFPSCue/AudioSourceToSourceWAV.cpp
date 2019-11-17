#include <memory>
#include <stdexcept>
#include <utility>

#include "AudioSourceToSourceWAV.hpp"
#include "GenerateTagID3.hpp"
#include "GenerateTagRIFF.hpp"
#include "MemorySource.hpp"
#include "MergedSource.hpp"



AudioSourceToSourceWAV::AudioSourceToSourceWAV(std::shared_ptr<AudioSource> audioSource, const CueSheet* ptrCueSheet, CueSheet::File::Track::TrackNumber trackNumber) {
  /*
  preambleRiffWave:
  00: RIFF [SIZE]
  08: WAVE
  0C:::::

  preambleFmt:
  00: fmt  [SIZE]
  08: [format data (16 bytes)]
  18:::::

  preambleData:
  00: data [SIZE]
  08:::::
  */

  char preambleDataRiffWave[0x0C] = {
    /* 00# */ 'R', 'I', 'F', 'F', 0x00, 0x00, 0x00, 0x00,
    /* 08  */ 'W', 'A', 'V', 'E',
    /* 0C  */
  };

  char preambleDataFmt[0x18] = {
    /* 00  */ 'f', 'm', 't', ' ', 0x10, 0x00, 0x00, 0x00,
    /* 08  */ 0x01, 0x00,
    /* 0A# */ 0x00, 0x00,
    /* 0C# */ 0x00, 0x00, 0x00, 0x00,
    /* 10# */ 0x00, 0x00, 0x00, 0x00,
    /* 14# */ 0x00, 0x00,
    /* 16# */ 0x00, 0x00,
    /* 18  */
  };

  char preambleDataData[0x08] = {
    /* 00# */ 'd', 'a', 't', 'a', 0x00, 0x00, 0x00, 0x00,
    /* 08  */
  };

  std::vector<std::byte> preambleDataRiffTag;
  std::vector<std::byte> preambleDataId3Tag;


  if (ptrCueSheet) {
    try {
      preambleDataRiffTag = GenerateTagRIFF(*ptrCueSheet, trackNumber);
    } catch (...) {}

    try {
      preambleDataId3Tag = GenerateTagID3(*ptrCueSheet, trackNumber);
    } catch (...) {}
  }

  const std::size_t mergedPreambleDataSize = sizeof(preambleDataRiffWave) + sizeof(preambleDataFmt) + preambleDataRiffTag.size() + preambleDataId3Tag.size() + sizeof(preambleDataData);

  const SourceSize audioDataSize = audioSource->GetSize();
  const SourceSize totalSourceSize = mergedPreambleDataSize + audioDataSize;

  constexpr auto Bits = 16;
  if (audioSource->GetDataType() != AudioSource::DataType::Int16) {
    throw std::runtime_error("unsupported DataType");
  }

  *reinterpret_cast<std::uint32_t*>(preambleDataRiffWave + 0x04) = static_cast<std::uint32_t>(totalSourceSize - 8);

  *reinterpret_cast<std::uint16_t*>(preambleDataFmt + 0x0A) = static_cast<std::uint16_t>(audioSource->GetChannels());
  *reinterpret_cast<std::uint32_t*>(preambleDataFmt + 0x0C) = static_cast<std::uint32_t>(audioSource->GetSamplingRate());
  *reinterpret_cast<std::uint32_t*>(preambleDataFmt + 0x10) = static_cast<std::uint32_t>(audioSource->GetSamplingRate() * Bits * audioSource->GetChannels() / 8);
  *reinterpret_cast<std::uint16_t*>(preambleDataFmt + 0x14) = static_cast<std::uint16_t>(audioSource->GetChannels() * Bits / 8);
  *reinterpret_cast<std::uint16_t*>(preambleDataFmt + 0x16) = static_cast<std::uint16_t>(Bits);

  *reinterpret_cast<std::uint32_t*>(preambleDataData + 0x04) = static_cast<std::uint32_t>(audioDataSize);

  auto mergedPreambleData = std::make_unique<std::byte[]>(mergedPreambleDataSize);
  auto ptr = mergedPreambleData.get();
  std::memcpy(ptr, preambleDataRiffWave, sizeof(preambleDataRiffWave));
  ptr += sizeof(preambleDataRiffWave);
  std::memcpy(ptr, preambleDataFmt, sizeof(preambleDataFmt));
  ptr += sizeof(preambleDataFmt);
  std::memcpy(ptr, preambleDataRiffTag.data(), preambleDataRiffTag.size());
  ptr += preambleDataRiffTag.size();
  std::memcpy(ptr, preambleDataId3Tag.data(), preambleDataId3Tag.size());
  ptr += preambleDataId3Tag.size();
  std::memcpy(ptr, preambleDataData, sizeof(preambleDataData));
  ptr += sizeof(preambleDataData);

  auto preambleSource = std::make_shared<MemorySource>(std::move(mergedPreambleData), mergedPreambleDataSize);

  std::vector<std::shared_ptr<Source>> sources{
    preambleSource,
    audioSource,
  };
  mSource = std::make_shared<MergedSource>(sources);
}


AudioSourceToSourceWAV::AudioSourceToSourceWAV(std::shared_ptr<AudioSource> audioSource) :
  AudioSourceToSourceWAV(audioSource, nullptr, 0)
{}


Source::SourceSize AudioSourceToSourceWAV::GetSize() {
  return mSource->GetSize();
}


NTSTATUS AudioSourceToSourceWAV::Read(SourceOffset offset, std::byte* buffer, std::size_t size, std::size_t* readSize) {
  return mSource->Read(offset, buffer, size, readSize);
}
