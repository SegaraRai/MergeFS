#pragma once

#include <cassert>
#include <cstddef>
#include <string>
#include <vector>

#include "GenerateTagID3.hpp"
#include "CueSheet.hpp"

using namespace std::literals;



namespace {
  constexpr bool EnableFooter = true;



  // ID3 is big endian
  constexpr std::uint_fast32_t ToSynchsafe(std::uint_fast32_t value) {
    assert((value >> 24) == 0);
    return
      ((value & 0b00001111'11100000'00000000'00000000) >> 21) |
      ((value & 0b00000000'00011111'11000000'00000000) >>  6) |
      ((value & 0b00000000'00000000'00111111'10000000) <<  9) |
      ((value & 0b00000000'00000000'00000000'01111111) << 24);
  }
  static_assert(ToSynchsafe(0b1110111'1110011'1000111'1100001) == 0b01100001'01000111'01110011'01110111);


  constexpr std::uint_fast32_t SigBe(const char signatureString[5]) {
    return
      ((static_cast<std::uint_fast32_t>(signatureString[0]) & 0xFF) << 24) | 
      ((static_cast<std::uint_fast32_t>(signatureString[1]) & 0xFF) << 16) | 
      ((static_cast<std::uint_fast32_t>(signatureString[2]) & 0xFF) <<  8) | 
      ((static_cast<std::uint_fast32_t>(signatureString[3]) & 0xFF) <<  0);
  }



  constexpr std::uint_fast32_t SigBeAENC = SigBe("AENC");
  constexpr std::uint_fast32_t SigBeAPIC = SigBe("APIC");
  constexpr std::uint_fast32_t SigBeCOMM = SigBe("COMM");
  constexpr std::uint_fast32_t SigBeCOMR = SigBe("COMR");
  constexpr std::uint_fast32_t SigBeENCR = SigBe("ENCR");
  constexpr std::uint_fast32_t SigBeEQUA = SigBe("EQUA");
  constexpr std::uint_fast32_t SigBeETCO = SigBe("ETCO");
  constexpr std::uint_fast32_t SigBeGEOB = SigBe("GEOB");
  constexpr std::uint_fast32_t SigBeGRID = SigBe("GRID");
  constexpr std::uint_fast32_t SigBeIPLS = SigBe("IPLS");
  constexpr std::uint_fast32_t SigBeLINK = SigBe("LINK");
  constexpr std::uint_fast32_t SigBeMCDI = SigBe("MCDI");
  constexpr std::uint_fast32_t SigBeMLLT = SigBe("MLLT");
  constexpr std::uint_fast32_t SigBeOWNE = SigBe("OWNE");
  constexpr std::uint_fast32_t SigBePRIV = SigBe("PRIV");
  constexpr std::uint_fast32_t SigBePCNT = SigBe("PCNT");
  constexpr std::uint_fast32_t SigBePOPM = SigBe("POPM");
  constexpr std::uint_fast32_t SigBePOSS = SigBe("POSS");
  constexpr std::uint_fast32_t SigBeRBUF = SigBe("RBUF");
  constexpr std::uint_fast32_t SigBeRVAD = SigBe("RVAD");
  constexpr std::uint_fast32_t SigBeRVRB = SigBe("RVRB");
  constexpr std::uint_fast32_t SigBeSYLT = SigBe("SYLT");
  constexpr std::uint_fast32_t SigBeSYTC = SigBe("SYTC");
  constexpr std::uint_fast32_t SigBeTALB = SigBe("TALB");
  constexpr std::uint_fast32_t SigBeTBPM = SigBe("TBPM");
  constexpr std::uint_fast32_t SigBeTCOM = SigBe("TCOM");
  constexpr std::uint_fast32_t SigBeTCON = SigBe("TCON");
  constexpr std::uint_fast32_t SigBeTCOP = SigBe("TCOP");
  constexpr std::uint_fast32_t SigBeTDAT = SigBe("TDAT");
  constexpr std::uint_fast32_t SigBeTDLY = SigBe("TDLY");
  constexpr std::uint_fast32_t SigBeTENC = SigBe("TENC");
  constexpr std::uint_fast32_t SigBeTEXT = SigBe("TEXT");
  constexpr std::uint_fast32_t SigBeTFLT = SigBe("TFLT");
  constexpr std::uint_fast32_t SigBeTIME = SigBe("TIME");
  constexpr std::uint_fast32_t SigBeTIT1 = SigBe("TIT1");
  constexpr std::uint_fast32_t SigBeTIT2 = SigBe("TIT2");
  constexpr std::uint_fast32_t SigBeTIT3 = SigBe("TIT3");
  constexpr std::uint_fast32_t SigBeTKEY = SigBe("TKEY");
  constexpr std::uint_fast32_t SigBeTLAN = SigBe("TLAN");
  constexpr std::uint_fast32_t SigBeTLEN = SigBe("TLEN");
  constexpr std::uint_fast32_t SigBeTMED = SigBe("TMED");
  constexpr std::uint_fast32_t SigBeTOAL = SigBe("TOAL");
  constexpr std::uint_fast32_t SigBeTOFN = SigBe("TOFN");
  constexpr std::uint_fast32_t SigBeTOLY = SigBe("TOLY");
  constexpr std::uint_fast32_t SigBeTOPE = SigBe("TOPE");
  constexpr std::uint_fast32_t SigBeTORY = SigBe("TORY");
  constexpr std::uint_fast32_t SigBeTOWN = SigBe("TOWN");
  constexpr std::uint_fast32_t SigBeTPE1 = SigBe("TPE1");
  constexpr std::uint_fast32_t SigBeTPE2 = SigBe("TPE2");
  constexpr std::uint_fast32_t SigBeTPE3 = SigBe("TPE3");
  constexpr std::uint_fast32_t SigBeTPE4 = SigBe("TPE4");
  constexpr std::uint_fast32_t SigBeTPOS = SigBe("TPOS");
  constexpr std::uint_fast32_t SigBeTPUB = SigBe("TPUB");
  constexpr std::uint_fast32_t SigBeTRCK = SigBe("TRCK");
  constexpr std::uint_fast32_t SigBeTRDA = SigBe("TRDA");
  constexpr std::uint_fast32_t SigBeTRSN = SigBe("TRSN");
  constexpr std::uint_fast32_t SigBeTRSO = SigBe("TRSO");
  constexpr std::uint_fast32_t SigBeTSIZ = SigBe("TSIZ");
  constexpr std::uint_fast32_t SigBeTSRC = SigBe("TSRC");
  constexpr std::uint_fast32_t SigBeTSSE = SigBe("TSSE");
  constexpr std::uint_fast32_t SigBeTYER = SigBe("TYER");
  constexpr std::uint_fast32_t SigBeTXXX = SigBe("TXXX");
  constexpr std::uint_fast32_t SigBeUFID = SigBe("UFID");
  constexpr std::uint_fast32_t SigBeUSER = SigBe("USER");
  constexpr std::uint_fast32_t SigBeUSLT = SigBe("USLT");
  constexpr std::uint_fast32_t SigBeWCOM = SigBe("WCOM");
  constexpr std::uint_fast32_t SigBeWCOP = SigBe("WCOP");
  constexpr std::uint_fast32_t SigBeWOAF = SigBe("WOAF");
  constexpr std::uint_fast32_t SigBeWOAR = SigBe("WOAR");
  constexpr std::uint_fast32_t SigBeWOAS = SigBe("WOAS");
  constexpr std::uint_fast32_t SigBeWORS = SigBe("WORS");
  constexpr std::uint_fast32_t SigBeWPAY = SigBe("WPAY");
  constexpr std::uint_fast32_t SigBeWPUB = SigBe("WPUB");
  constexpr std::uint_fast32_t SigBeWXXX = SigBe("WXXX");
}



std::vector<std::byte> GenerateTagID3(const CueSheet& cueSheet, CueSheet::File::Track::TrackNumber trackNumber) {
  // ID3v2.3.0

  const auto indexPairN = cueSheet.FindTrack(trackNumber);
  if (!indexPairN) {
    return {};
  }

  const auto[fileIndex, trackIndex] = indexPairN.value();

  const auto& file = cueSheet.files.at(fileIndex);
  const auto& track = file.tracks.at(trackIndex);

  // TODO

  return {};
}
