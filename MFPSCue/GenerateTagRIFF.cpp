#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include <Windows.h>

#include "GenerateTagRIFF.hpp"
#include "CueSheet.hpp"
#include "EncodingConverter.hpp"

using namespace std::literals;



namespace {
  constexpr UINT CodePage = CP_OEMCP;



  template<typename T>
  constexpr T AlignRIFFChunkSize(T size) {
    const auto mask = ~static_cast<T>(1);
    return (size + 1) & mask;
  }

  static_assert(AlignRIFFChunkSize(0) == 0);
  static_assert(AlignRIFFChunkSize(1) == 2);
  static_assert(AlignRIFFChunkSize(2) == 2);
  static_assert(AlignRIFFChunkSize(3) == 4);


  constexpr std::uint_fast32_t Be32ToLe32(std::uint_fast32_t value) {
    return (((value >> 0) & 0xFF) << 24) | (((value >> 8) & 0xFF) << 16) | (((value >> 16) & 0xFF) << 8) | (((value >> 24) & 0xFF) << 0);
  }


  constexpr std::uint_fast32_t SigBe(const char signatureString[5]) {
    return
      ((static_cast<std::uint_fast32_t>(signatureString[0]) & 0xFF) << 24) |
      ((static_cast<std::uint_fast32_t>(signatureString[1]) & 0xFF) << 16) |
      ((static_cast<std::uint_fast32_t>(signatureString[2]) & 0xFF) <<  8) |
      ((static_cast<std::uint_fast32_t>(signatureString[3]) & 0xFF) <<  0);
  }



  // Little endian chunk ids
  constexpr std::uint_fast32_t SigLeLIST = Be32ToLe32(SigBe("LIST"));
  constexpr std::uint_fast32_t SigLeINFO = Be32ToLe32(SigBe("INFO"));

  // Big endian chunk ids
  constexpr std::uint_fast32_t SigBeIART = SigBe("IART");
  constexpr std::uint_fast32_t SigBeICMT = SigBe("ICMT");
  constexpr std::uint_fast32_t SigBeICRD = SigBe("ICRD");
  constexpr std::uint_fast32_t SigBeIENG = SigBe("IENG");
  constexpr std::uint_fast32_t SigBeIGNR = SigBe("IGNR");
  constexpr std::uint_fast32_t SigBeINAM = SigBe("INAM");
  constexpr std::uint_fast32_t SigBeIPRD = SigBe("IPRD");
  constexpr std::uint_fast32_t SigBeITRK = SigBe("ITRK");
  constexpr std::uint_fast32_t SigBeISFT = SigBe("ISFT");
  constexpr std::uint_fast32_t SigBeTRCK = SigBe("TRCK");
}


std::vector<std::byte> GenerateTagRIFF(const CueSheet& cueSheet, CueSheet::File::Track::TrackNumber trackNumber) {
  const auto indexPairN = cueSheet.FindTrack(trackNumber);
  if (!indexPairN) {
    throw std::runtime_error("track not found in cue sheet");
  }

  const auto [fileIndex, trackIndex] = indexPairN.value();

  const auto& file = cueSheet.files.at(fileIndex);
  const auto& track = file.tracks.at(trackIndex);

  // store chunk id in BIG ENDIAN form to make chunks sorted
  std::map<std::uint_fast32_t, std::wstring> wstringChunks;

  wstringChunks.emplace(SigBeISFT, L"MFPSCue"s);

  wstringChunks.emplace(SigBeITRK, std::to_wstring(trackNumber));
  wstringChunks.emplace(SigBeTRCK, std::to_wstring(trackNumber));

  if (cueSheet.title) {
    wstringChunks.emplace(SigBeIPRD, cueSheet.title.value());
  }

  if (cueSheet.remEntryMap.count(L"DATE"s)) {
    wstringChunks.emplace(SigBeICRD, cueSheet.remEntryMap.at(L"DATE"s));
  }

  if (cueSheet.remEntryMap.count(L"GENRE"s)) {
    wstringChunks.emplace(SigBeIGNR, cueSheet.remEntryMap.at(L"GENRE"s));
  }

  if (cueSheet.remEntryMap.count(L"COMMENT"s)) {
    wstringChunks.emplace(SigBeICMT, cueSheet.remEntryMap.at(L"COMMENT"s));
  }

  if (track.title) {
    wstringChunks.emplace(SigBeINAM, track.title.value());
  }

  if (track.performer) {
    wstringChunks.emplace(SigBeIART, track.performer.value());
  } else if (cueSheet.performer) {
    wstringChunks.emplace(SigBeIART, cueSheet.performer.value());
  }

  /*
  if (cueSheet.performer) {
    wstringChunks.emplace(SigBeIENG, cueSheet.performer.value());
  }
  //*/


  std::map<std::uint_fast32_t, std::string> chunks;
  for (const auto& [key, value] : wstringChunks) {
    chunks.emplace(key, ConvertWStringToCodePage(value, CodePage));
  }


  std::size_t totalSize = 12;   // LIST [SIZE] INFO
  for (const auto& [key, value] : chunks) {
    totalSize += 8 + AlignRIFFChunkSize(value.size() + 1);
  }


  std::vector<std::byte> data(totalSize);
  std::size_t offset = 0;

  *reinterpret_cast<std::uint32_t*>(data.data() + offset) = SigLeLIST;
  offset += 4;

  *reinterpret_cast<std::uint32_t*>(data.data() + offset) = static_cast<std::uint32_t>(totalSize - 8);
  offset += 4;

  *reinterpret_cast<std::uint32_t*>(data.data() + offset) = SigLeINFO;
  offset += 4;

  for (const auto& [key, value] : chunks) {
    const auto alignedSize = AlignRIFFChunkSize(value.size() + 1);

    std::uint32_t chunkId = Be32ToLe32(key);
    *reinterpret_cast<std::uint32_t*>(data.data() + offset) = chunkId;
    offset += 4;

    *reinterpret_cast<std::uint32_t*>(data.data() + offset) = static_cast<std::uint32_t>(value.size() + 1);
    offset += 4;

    std::memcpy(data.data() + offset, value.c_str(), value.size());
    std::memset(data.data() + offset + value.size(), 0, alignedSize - value.size());
    offset += alignedSize;
  }

  return data;
}
