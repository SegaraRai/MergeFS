#pragma once

#include <cassert>
#include <cstddef>
#include <memory>
#include <regex>
#include <string>
#include <vector>

#include "GenerateTagID3.hpp"
#include "CueSheet.hpp"
#include "EncodingConverter.hpp"

using namespace std::literals;



namespace {
  enum class ID3Version {
    v23,
    v24,
  };

  constexpr ID3Version ID3Ver = ID3Version::v24;
  constexpr bool EnableFooter = true;
  constexpr bool UseUtf8 = true;



  // ID3 is big endian
  constexpr std::uint_fast32_t ToSynchsafe(std::uint_fast32_t value) {
    assert((value >> 24) == 0);
    return
      ((value & 0b00001111'11100000'00000000'00000000) >> 21) |
      ((value & 0b00000000'00011111'11000000'00000000) >>  6) |
      ((value & 0b00000000'00000000'00111111'10000000) <<  9) |
      ((value & 0b00000000'00000000'00000000'01111111) << 24);
  }


  constexpr std::uint_fast32_t SigLe(const char signatureString[5]) {
    return
      ((static_cast<std::uint_fast32_t>(signatureString[0]) & 0xFF) <<  0) | 
      ((static_cast<std::uint_fast32_t>(signatureString[1]) & 0xFF) <<  8) | 
      ((static_cast<std::uint_fast32_t>(signatureString[2]) & 0xFF) << 16) | 
      ((static_cast<std::uint_fast32_t>(signatureString[3]) & 0xFF) << 24);
  }


  constexpr std::uint_fast32_t ConvertEndianess32(std::uint_fast32_t value) {
    return (((value >> 0) & 0xFF) << 24) | (((value >> 8) & 0xFF) << 16) | (((value >> 16) & 0xFF) << 8) | (((value >> 24) & 0xFF) << 0);
  }


  constexpr std::uint_fast32_t RIFFChunkName = SigLe("id3 ");


  constexpr std::uint_fast32_t SigLeAENC = SigLe("AENC");
  constexpr std::uint_fast32_t SigLeAPIC = SigLe("APIC");
  constexpr std::uint_fast32_t SigLeCOMM = SigLe("COMM");
  constexpr std::uint_fast32_t SigLeCOMR = SigLe("COMR");
  constexpr std::uint_fast32_t SigLeENCR = SigLe("ENCR");
  constexpr std::uint_fast32_t SigLeEQUA = SigLe("EQUA");
  constexpr std::uint_fast32_t SigLeETCO = SigLe("ETCO");
  constexpr std::uint_fast32_t SigLeGEOB = SigLe("GEOB");
  constexpr std::uint_fast32_t SigLeGRID = SigLe("GRID");
  constexpr std::uint_fast32_t SigLeIPLS = SigLe("IPLS");
  constexpr std::uint_fast32_t SigLeLINK = SigLe("LINK");
  constexpr std::uint_fast32_t SigLeMCDI = SigLe("MCDI");
  constexpr std::uint_fast32_t SigLeMLLT = SigLe("MLLT");
  constexpr std::uint_fast32_t SigLeOWNE = SigLe("OWNE");
  constexpr std::uint_fast32_t SigLePRIV = SigLe("PRIV");
  constexpr std::uint_fast32_t SigLePCNT = SigLe("PCNT");
  constexpr std::uint_fast32_t SigLePOPM = SigLe("POPM");
  constexpr std::uint_fast32_t SigLePOSS = SigLe("POSS");
  constexpr std::uint_fast32_t SigLeRBUF = SigLe("RBUF");
  constexpr std::uint_fast32_t SigLeRVAD = SigLe("RVAD");
  constexpr std::uint_fast32_t SigLeRVRB = SigLe("RVRB");
  constexpr std::uint_fast32_t SigLeSYLT = SigLe("SYLT");
  constexpr std::uint_fast32_t SigLeSYTC = SigLe("SYTC");
  constexpr std::uint_fast32_t SigLeTALB = SigLe("TALB");
  constexpr std::uint_fast32_t SigLeTBPM = SigLe("TBPM");
  constexpr std::uint_fast32_t SigLeTCOM = SigLe("TCOM");
  constexpr std::uint_fast32_t SigLeTCON = SigLe("TCON");
  constexpr std::uint_fast32_t SigLeTCOP = SigLe("TCOP");
  constexpr std::uint_fast32_t SigLeTDAT = SigLe("TDAT");
  constexpr std::uint_fast32_t SigLeTDLY = SigLe("TDLY");
  constexpr std::uint_fast32_t SigLeTENC = SigLe("TENC");
  constexpr std::uint_fast32_t SigLeTEXT = SigLe("TEXT");
  constexpr std::uint_fast32_t SigLeTFLT = SigLe("TFLT");
  constexpr std::uint_fast32_t SigLeTIME = SigLe("TIME");
  constexpr std::uint_fast32_t SigLeTIT1 = SigLe("TIT1");
  constexpr std::uint_fast32_t SigLeTIT2 = SigLe("TIT2");
  constexpr std::uint_fast32_t SigLeTIT3 = SigLe("TIT3");
  constexpr std::uint_fast32_t SigLeTKEY = SigLe("TKEY");
  constexpr std::uint_fast32_t SigLeTLAN = SigLe("TLAN");
  constexpr std::uint_fast32_t SigLeTLEN = SigLe("TLEN");
  constexpr std::uint_fast32_t SigLeTMED = SigLe("TMED");
  constexpr std::uint_fast32_t SigLeTOAL = SigLe("TOAL");
  constexpr std::uint_fast32_t SigLeTOFN = SigLe("TOFN");
  constexpr std::uint_fast32_t SigLeTOLY = SigLe("TOLY");
  constexpr std::uint_fast32_t SigLeTOPE = SigLe("TOPE");
  constexpr std::uint_fast32_t SigLeTORY = SigLe("TORY");
  constexpr std::uint_fast32_t SigLeTOWN = SigLe("TOWN");
  constexpr std::uint_fast32_t SigLeTPE1 = SigLe("TPE1");
  constexpr std::uint_fast32_t SigLeTPE2 = SigLe("TPE2");
  constexpr std::uint_fast32_t SigLeTPE3 = SigLe("TPE3");
  constexpr std::uint_fast32_t SigLeTPE4 = SigLe("TPE4");
  constexpr std::uint_fast32_t SigLeTPOS = SigLe("TPOS");
  constexpr std::uint_fast32_t SigLeTPUB = SigLe("TPUB");
  constexpr std::uint_fast32_t SigLeTRCK = SigLe("TRCK");
  constexpr std::uint_fast32_t SigLeTRDA = SigLe("TRDA");
  constexpr std::uint_fast32_t SigLeTRSN = SigLe("TRSN");
  constexpr std::uint_fast32_t SigLeTRSO = SigLe("TRSO");
  constexpr std::uint_fast32_t SigLeTSIZ = SigLe("TSIZ");
  constexpr std::uint_fast32_t SigLeTSRC = SigLe("TSRC");
  constexpr std::uint_fast32_t SigLeTSSE = SigLe("TSSE");
  constexpr std::uint_fast32_t SigLeTYER = SigLe("TYER");
  constexpr std::uint_fast32_t SigLeTXXX = SigLe("TXXX");
  constexpr std::uint_fast32_t SigLeUFID = SigLe("UFID");
  constexpr std::uint_fast32_t SigLeUSER = SigLe("USER");
  constexpr std::uint_fast32_t SigLeUSLT = SigLe("USLT");
  constexpr std::uint_fast32_t SigLeWCOM = SigLe("WCOM");
  constexpr std::uint_fast32_t SigLeWCOP = SigLe("WCOP");
  constexpr std::uint_fast32_t SigLeWOAF = SigLe("WOAF");
  constexpr std::uint_fast32_t SigLeWOAR = SigLe("WOAR");
  constexpr std::uint_fast32_t SigLeWOAS = SigLe("WOAS");
  constexpr std::uint_fast32_t SigLeWORS = SigLe("WORS");
  constexpr std::uint_fast32_t SigLeWPAY = SigLe("WPAY");
  constexpr std::uint_fast32_t SigLeWPUB = SigLe("WPUB");
  constexpr std::uint_fast32_t SigLeWXXX = SigLe("WXXX");


  class ID3Frame;


  class ID3Tag {
    ID3Version version;
    bool enableFooter;
    std::vector<std::shared_ptr<ID3Frame>> frames;

    std::size_t GetContentSize() const;

  public:
    ID3Tag(ID3Version version, bool enableFooter);

    ID3Version GetVersion() const;
    bool GetFooterEnabled() const;

    std::size_t GetSize() const;
    void WriteData(std::byte* dest) const;

    void AddFrame(std::shared_ptr<ID3Frame> id3Frame);
  };


  class ID3Frame {
  protected:
    ID3Tag& id3Tag;
    std::uint_fast32_t frameId;
    std::uint_fast16_t flags;

    ID3Frame(ID3Tag& id3Tag, std::uint_fast32_t frameId, std::uint_fast16_t flags = 0) :
      id3Tag(id3Tag),
      frameId(frameId),
      flags(flags)
    {}

    virtual std::size_t GetContentSize() const = 0;
    virtual void WriteContentData(std::byte* dest) const = 0;

  public:
    std::size_t GetSize() const {
      return GetContentSize() + 10;
    }

    void WriteData(std::byte* dest) const {
      *reinterpret_cast<std::uint32_t*>(dest + 0) = frameId;
      *reinterpret_cast<std::uint32_t*>(dest + 4) = id3Tag.GetVersion() == ID3Version::v24
        ? ToSynchsafe(GetContentSize())
        : ConvertEndianess32(GetContentSize());
      *reinterpret_cast<std::uint16_t*>(dest + 8) = ConvertEndianess32(flags);
      WriteContentData(dest + 10);
    }
  };


  class ID3TextFrame : public ID3Frame {
    std::size_t size;
    std::unique_ptr<std::byte[]> data;

  public:
    ID3TextFrame(ID3Tag& id3Tag, std::uint_fast32_t frameId, std::wstring_view text, bool useUtf8) :
      ID3Frame(id3Tag, frameId),
      size(0),
      data()
    {
      static_assert(sizeof(wchar_t) == 2);

      if (id3Tag.GetVersion() == ID3Version::v24 && useUtf8) {
        const auto utf8Text = ConvertWStringToCodePage(text, CP_UTF8);
        size = utf8Text.size() + 2;
        data = std::make_unique<std::byte[]>(size);
        data[0] = std::byte{0x03};    // UTF-8
        std::memcpy(&data[1], utf8Text.c_str(), utf8Text.size() + 1);
      } else {
        size = text.size() * sizeof(wchar_t) + 5;
        data = std::make_unique<std::byte[]>(size);
        data[0] = std::byte{0x01};    // UTF-16 (with BOM in v2.4)
        data[1] = std::byte{0xFF};    // BOM
        data[2] = std::byte{0xFE};    // BOM
        std::memcpy(&data[3], text.data(), text.size() * sizeof(wchar_t));
        data[size - 2] = std::byte{0x00};
        data[size - 1] = std::byte{0x00};
      }
    }

    std::size_t GetContentSize() const override {
      return size;
    }

    void WriteContentData(std::byte* dest) const override {
      std::memcpy(dest, data.get(), size);
    }
  };


  //

  std::size_t ID3Tag::GetContentSize() const {
    std::size_t size = 0;
    for (const auto& frame : frames) {
      size += frame->GetSize();
    }
    return size;
  }

  ID3Tag::ID3Tag(ID3Version version, bool enableFooter) :
    version(version),
    enableFooter(enableFooter),
    frames()
  {
    if (version != ID3Version::v24) {
      this->enableFooter = false;
    }
  }

  ID3Version ID3Tag::GetVersion() const {
    return version;
  }

  bool ID3Tag::GetFooterEnabled() const {
    return enableFooter;
  }

  void ID3Tag::AddFrame(std::shared_ptr<ID3Frame> id3Frame) {
    frames.push_back(id3Frame);
  }

  std::size_t ID3Tag::GetSize() const {
    return GetContentSize() + (enableFooter ? 20 : 10);
  }

  void ID3Tag::WriteData(std::byte* dest) const {
    dest[0] = std::byte{0x49};    // I
    dest[1] = std::byte{0x44};    // D
    dest[2] = std::byte{0x33};    // 3

    switch (version) {
      case ID3Version::v23:
        dest[3] = std::byte{0x03};
        dest[4] = std::byte{0x00};
        break;

      case ID3Version::v24:
        dest[3] = std::byte{0x04};
        dest[4] = std::byte{0x00};
        break;
    }

    dest[5] = std::byte{0x00};

    // do not include footer size
    *reinterpret_cast<std::uint32_t*>(dest + 6) = ToSynchsafe(GetContentSize());

    auto ptr = dest + 10;
    for (const auto& frame : frames) {
      frame->WriteData(ptr);
      ptr += frame->GetSize();
    }

    if (enableFooter) {
      ptr[0] = std::byte{0x33};    // 3
      ptr[1] = std::byte{0x44};    // D
      ptr[2] = std::byte{0x49};    // I
      std::memcpy(ptr + 3, dest + 3, 7);
    }
  }
}



std::vector<std::byte> GenerateTagID3(const CueSheet& cueSheet, CueSheet::File::Track::TrackNumber trackNumber) {
  const auto indexPairN = cueSheet.FindTrack(trackNumber);
  if (!indexPairN) {
    return {};
  }

  // TODO: 高効率化
  unsigned long numTracks = 0;
  for (const auto& file : cueSheet.files) {
    numTracks += file.tracks.size();
  }

  const auto [fileIndex, trackIndex] = indexPairN.value();

  const auto& file = cueSheet.files.at(fileIndex);
  const auto& track = file.tracks.at(trackIndex);
  
  // TODO

  ID3Tag id3Tag(ID3Ver, EnableFooter);
  
  id3Tag.AddFrame(std::make_shared<ID3TextFrame>(id3Tag, SigLeTRCK, std::to_wstring(trackNumber) + L"/"s + std::to_wstring(numTracks), UseUtf8));

  if (track.title) {
    id3Tag.AddFrame(std::make_shared<ID3TextFrame>(id3Tag, SigLeTIT2, track.title.value(), UseUtf8));
  }

  if (track.performer) {
    id3Tag.AddFrame(std::make_shared<ID3TextFrame>(id3Tag, SigLeTPE1, track.performer.value(), UseUtf8));
  }
  
  if (cueSheet.title) {
    id3Tag.AddFrame(std::make_shared<ID3TextFrame>(id3Tag, SigLeTALB, cueSheet.title.value(), UseUtf8));
  }

  if (cueSheet.performer) {
    id3Tag.AddFrame(std::make_shared<ID3TextFrame>(id3Tag, SigLeTPE2, cueSheet.performer.value(), UseUtf8));
  }

  if (cueSheet.remEntryMap.count(L"DATE"s)) {
    auto GetTail = [] (const std::wstring& str, std::size_t size) {
      return str.substr(str.size() - size);
    };

    const auto dateText = cueSheet.remEntryMap.at(L"DATE"s);

    // 適当に解釈
    // YYYY-mm-ddとdd.mm.YYYYとYYYYに対応する

    int year = 0;
    int month = 0;
    int date = 0;

    std::wsmatch match;
    if (std::regex_match(dateText, match, std::wregex(LR"(\b(\d{4})\D+(\d{1,2})\D+(\d{1,2})\b)"))) {
      // YYYY-mm-dd (区切り文字はハイフンでなくても良い)
      year = std::stoi(match[1]);
      month = std::stoi(match[2]);
      date = std::stoi(match[3]);
    } else if (std::regex_match(dateText, match, std::wregex(LR"(\b(\d{1,2})\D+(\d{1,2})\D+(\d{4})\b)"))) {
      // dd.mm.YYYY (区切り文字はピリオドでなくても良い)
      year = std::stoi(match[3]);
      month = std::stoi(match[2]);
      date = std::stoi(match[1]);
    } else if (std::regex_match(dateText, match, std::wregex(LR"(\b(\d{4})\b)"))) {
      // YYYY
      year = std::stoi(match[1]);
    }

    if (year >= 1) {
      const auto strYear = GetTail(L"000"s + std::to_wstring(year), 4);
      id3Tag.AddFrame(std::make_shared<ID3TextFrame>(id3Tag, SigLeTYER, strYear, UseUtf8));
    }

    if (month >= 1 && month <= 12 && date >= 1 && date <= 31) {
      const auto strMonth = GetTail(L"0"s + std::to_wstring(month), 2);
      const auto strDate = GetTail(L"0"s + std::to_wstring(date), 2);
      id3Tag.AddFrame(std::make_shared<ID3TextFrame>(id3Tag, SigLeTDAT, strDate + strMonth, UseUtf8));
    }
  }

  if (cueSheet.remEntryMap.count(L"GENRE"s)) {
    id3Tag.AddFrame(std::make_shared<ID3TextFrame>(id3Tag, SigLeTCON, cueSheet.remEntryMap.at(L"GENRE"s), UseUtf8));
  }

  if (cueSheet.remEntryMap.count(L"COMMENT"s)) {
    // TODO
  }

  id3Tag.AddFrame(std::make_shared<ID3TextFrame>(id3Tag, SigLeTSSE, L"MFPSCue"sv, UseUtf8));


  //

  std::size_t size = ((id3Tag.GetSize() + 1) & ~static_cast<std::size_t>(1)) + 8;
  std::vector<std::byte> data(size);
  data[size - 1] = std::byte{0x00};
  *reinterpret_cast<std::uint32_t*>(data.data() + 0) = RIFFChunkName;
  *reinterpret_cast<std::uint32_t*>(data.data() + 4) = static_cast<std::uint32_t>(size - 8);
  id3Tag.WriteData(data.data() + 8);

  return data;
}
