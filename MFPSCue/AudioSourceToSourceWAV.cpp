#include <memory>
#include <utility>

#include "AudioSourceToSourceWAV.hpp"
#include "GenerateTagRIFF.hpp"
#include "MemorySource.hpp"
#include "MergedSource.hpp"



AudioSourceToSourceWAV::AudioSourceToSourceWAV(std::shared_ptr<AudioSource> audioSource, const CueSheet* ptrCueSheet, CueSheet::File::Track::TrackNumber trackNumber) {
  /*
  preamble1:
  00: RIFF [SIZE]
  08: WAVE
  0C:::::

  preamble3:
  00: fmt  [SIZE]
  08: [format data (16 bytes)]
  18: data [SIZE]
  20:::::
  */

  char preambleData1[0x0C] = {
    /* 00# */ 'R', 'I', 'F', 'F', 0x00, 0x00, 0x00, 0x00,
    /* 08  */ 'W', 'A', 'V', 'E',
    /* 0C  */
  };

  char preambleData3[0x20] = {
    /* 00  */ 'f', 'm', 't', ' ', 0x10, 0x00, 0x00, 0x00,
    /* 08  */ 0x01, 0x00,
    /* 0A# */ 0x00, 0x00,
    /* 0C# */ 0x00, 0x00, 0x00, 0x00,
    /* 10# */ 0x00, 0x00, 0x00, 0x00,
    /* 14# */ 0x00, 0x00,
    /* 16# */ 0x00, 0x00,
    /* 18  */ 'd', 'a', 't', 'a', 0x00, 0x00, 0x00, 0x00,
    /* 20  */
  };

  std::vector<std::byte> preambleData2;


  if (ptrCueSheet) {
    try {
      preambleData2 = GenerateTagRIFF(*ptrCueSheet, trackNumber);
    } catch (...) {}
  }

  const std::size_t mergedPreambleDataSize = sizeof(preambleData1) + preambleData2.size() + sizeof(preambleData3);

  const SourceSize audioDataSize = audioSource->GetSize();
  const SourceSize totalSourceSize = mergedPreambleDataSize + audioDataSize;

  const auto bits = 16;
  if (audioSource->GetDataType() != AudioSource::DataType::Int16) {
    throw std::runtime_error(u8"unsupported DataType");
  }

  *reinterpret_cast<std::uint32_t*>(preambleData1 + 0x04) = static_cast<std::uint32_t>(totalSourceSize - 8);

  *reinterpret_cast<std::uint16_t*>(preambleData3 + 0x0A) = static_cast<std::uint16_t>(audioSource->GetChannels());
  *reinterpret_cast<std::uint32_t*>(preambleData3 + 0x0C) = static_cast<std::uint32_t>(audioSource->GetSamplingRate());
  *reinterpret_cast<std::uint32_t*>(preambleData3 + 0x10) = static_cast<std::uint32_t>(audioSource->GetSamplingRate() * bits * audioSource->GetChannels() / 8);
  *reinterpret_cast<std::uint16_t*>(preambleData3 + 0x14) = static_cast<std::uint16_t>(audioSource->GetChannels() * bits / 8);
  *reinterpret_cast<std::uint16_t*>(preambleData3 + 0x16) = static_cast<std::uint16_t>(bits);
  *reinterpret_cast<std::uint32_t*>(preambleData3 + 0x1C) = static_cast<std::uint32_t>(audioDataSize);

  auto mergedPreambleData = std::make_unique<std::byte[]>(mergedPreambleDataSize);
  auto ptr = mergedPreambleData.get();
  std::memcpy(ptr, preambleData1, sizeof(preambleData1));
  ptr += sizeof(preambleData1);
  std::memcpy(ptr, preambleData2.data(), preambleData2.size());
  ptr += preambleData2.size();
  std::memcpy(ptr, preambleData3, sizeof(preambleData3));

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
