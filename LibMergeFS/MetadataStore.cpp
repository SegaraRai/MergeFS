#include "MetadataStore.hpp"
#include "Metadata.hpp"
#include "Util.hpp"
#include "NsError.hpp"

#include "../Util/Common.hpp"

#include <malloc.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <new>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Windows.h>

using namespace std::literals;


namespace {
  const std::wstring StrRemovedPrefix(MetadataStore::RemovedPrefix);
  const std::wstring StrRemovedPrefixB(StrRemovedPrefix + L"\\"s);


  template<typename T, std::size_t Alignment>
  auto make_unique_aligned(std::size_t size) {
    struct aligned_deleter {
      void operator()(T* ptr) const {
        _aligned_free(ptr);
      }
    };

    auto ptr = _aligned_malloc(sizeof(T) * size, Alignment);
    if (!ptr) {
      throw std::bad_alloc();
    }
    return std::unique_ptr<T[], aligned_deleter>(reinterpret_cast<T*>(ptr), aligned_deleter());
  }
}


namespace MetadataFileV1 {
  constexpr std::uint32_t Signature = 0x00000001;

  /*
  EntryCount
  {
    EntrySize
    EntryFlag
    KeyNameCount
    WCHAR[KeyNameCount]
    ?FileAttributes
    ?CreationTime
    ?LastAccessTime
    ?LastWriteTime
    ?SecurityCount
    ?CHAR[SecurityCount]
  }[EntryCount]
  */

  using EntryCount = std::uint64_t;
  using EntrySize = std::uint32_t;
  using EntryFlag = std::uint16_t;
  using DeletedFlag = std::uint8_t;
  using KeyNameCount = std::uint32_t;
  using SecurityCount = std::uint32_t;

  namespace EntryFlags {
    // 1 << 0 is reserved
    constexpr EntryFlag HasAttributes     = 1 << 1;
    constexpr EntryFlag HasCreationTime   = 1 << 2;
    constexpr EntryFlag HasLastAccessTime = 1 << 3;
    constexpr EntryFlag HasLastWriteTime  = 1 << 4;
    constexpr EntryFlag HasSecurity       = 1 << 5;
  }
}


namespace MetadataFileV2 {
  constexpr std::uint32_t Signature = 0x444D464D;   // "MFMD"
  constexpr std::uint32_t Version   = 0x00020000;

  constexpr unsigned int Alignment = 16;

  constexpr std::uint64_t ReadFILETIME(const FILETIME& filetime) {
    return static_cast<std::uint64_t>(filetime.dwLowDateTime) | (static_cast<std::uint64_t>(filetime.dwHighDateTime) << 32);
  }

  constexpr FILETIME ToFILETIME(std::uint64_t filetime) {
    return FILETIME{
      filetime & 0xFFFFFFFF,
      (filetime >> 32) & 0xFFFFFFFF,
    };
  }

  template<typename T>
  constexpr T Align(T size) {
    return (size + (Alignment - 1)) & ~static_cast<T>(Alignment - 1);
  }

  struct Header {
    std::uint32_t signature;
    std::uint32_t version;
    std::uint64_t dataSize;
    std::uint64_t renameSectionOffset;
    std::uint64_t renameSectionSize;
    std::uint64_t renameSectionCount;
    std::uint64_t reserved1;
    std::uint64_t metadataSectionOffset;
    std::uint64_t metadataSectionSize;
    std::uint64_t metadataSectionCount;
    std::uint64_t reserved2;
  };
  static_assert(sizeof(Header) == 16 * 5);

  struct RenameEntryHeader {
    std::uint32_t blockSize;
    std::uint32_t reserved1;
    std::uint32_t aSize;
    std::uint32_t bSize;
  };
  static_assert(sizeof(RenameEntryHeader) == 16 * 1);

  struct RenameEntry : RenameEntryHeader {
    std::wstring a;
    std::wstring b;

    RenameEntry(const RenameEntryHeader& header, const std::wstring& a, const std::wstring& b) :
      RenameEntryHeader(header),
      a(a),
      b(b)
    {}

    static RenameEntry Parse(const std::byte* data, std::function<void(const std::byte* ptr)> checkPtr, std::size_t& size) {
      auto ptr = data;

      checkPtr(ptr + sizeof(RenameEntryHeader));
      const auto& header = *reinterpret_cast<const RenameEntryHeader*>(ptr);
      const auto nextPtr = ptr + header.blockSize;
      checkPtr(nextPtr);
      ptr += sizeof(RenameEntryHeader);

      static_assert(sizeof(wchar_t) == sizeof(char16_t));

      const auto alignedASize = Align(header.aSize * sizeof(char16_t));
      auto aStrPtr = reinterpret_cast<const wchar_t*>(ptr);
      const std::wstring a(aStrPtr, aStrPtr + header.aSize);
      ptr += alignedASize;

      const auto alignedBSize = Align(header.bSize * sizeof(char16_t));
      auto bStrPtr = reinterpret_cast<const wchar_t*>(ptr);
      const std::wstring b(bStrPtr, bStrPtr + header.bSize);
      ptr += alignedBSize;

      assert(ptr == nextPtr);

      size = nextPtr - data;

      return RenameEntry(header, a, b);
    }
  };

  namespace EntryFlags {
    // 1 << 0 is reserved
    constexpr std::uint32_t HasAttributes = 1 << 1;
    constexpr std::uint32_t HasCreationTime = 1 << 2;
    constexpr std::uint32_t HasLastAccessTime = 1 << 3;
    constexpr std::uint32_t HasLastWriteTime = 1 << 4;
    constexpr std::uint32_t HasSecurity = 1 << 5;
  }

  struct MetadataEntryHeader {
    std::uint32_t blockSize;
    std::uint32_t reserved1;
    std::uint32_t filenameSize;
    std::uint32_t securitySize;
    std::uint32_t flags;
    std::uint32_t attributes;
    std::uint64_t creationTime;
    std::uint64_t lastAccessTime;
    std::uint64_t lastWriteTime;
  };
  static_assert(sizeof(MetadataEntryHeader) == 16 * 3);

  struct MetadataEntry : MetadataEntryHeader {
    std::wstring filename;
    std::string security;

    MetadataEntry(const MetadataEntryHeader& header, const std::wstring& filename, const std::string& security) :
      MetadataEntryHeader(header),
      filename(filename),
      security(security)
    {}

    operator Metadata() const {
      using namespace MetadataFileV2;

      Metadata metadata;

      if (flags & EntryFlags::HasAttributes) {
        metadata.fileAttributes.emplace(attributes);
      }
      if (flags & EntryFlags::HasCreationTime) {
        metadata.creationTime.emplace(ToFILETIME(creationTime));
      }
      if (flags & EntryFlags::HasLastAccessTime) {
        metadata.lastAccessTime.emplace(ToFILETIME(lastAccessTime));
      }
      if (flags & EntryFlags::HasLastWriteTime) {
        metadata.lastWriteTime.emplace(ToFILETIME(lastWriteTime));
      }
      if (flags & EntryFlags::HasSecurity) {
        metadata.security.emplace(security);
      }

      return metadata;
    }

    static MetadataEntry Parse(const std::byte* data, std::function<void(const std::byte * ptr)> checkPtr, std::size_t& size) {
      auto ptr = data;

      checkPtr(ptr + sizeof(MetadataEntryHeader));
      const auto& header = *reinterpret_cast<const MetadataEntryHeader*>(ptr);
      const auto nextPtr = ptr + header.blockSize;
      checkPtr(nextPtr);
      ptr += sizeof(MetadataEntryHeader);

      static_assert(sizeof(wchar_t) == sizeof(char16_t));

      const auto alignedFilenameSize = Align(header.filenameSize * sizeof(char16_t));
      auto filenameStrPtr = reinterpret_cast<const wchar_t*>(ptr);
      const std::wstring filename(filenameStrPtr, filenameStrPtr + header.filenameSize);
      ptr += alignedFilenameSize;

      const auto alignedSecuritySize = Align(header.securitySize);
      auto securityStrPtr = reinterpret_cast<const char*>(ptr);
      const std::string security(securityStrPtr, securityStrPtr + header.securitySize);
      ptr += alignedSecuritySize;

      assert(ptr == nextPtr);

      size = nextPtr - data;

      return MetadataEntry(header, filename, security);
    }
  };

  enum class AppendixDataType : std::uint32_t {
    Rename   = 0x00000001,
    Metadata = 0x00000002,
  };

  struct AppendixEntryHeader {
    std::uint32_t blockSize;
    std::uint32_t reserved1;
    AppendixDataType dataType;
    std::uint32_t reserved2;
  };
  static_assert(sizeof(AppendixEntryHeader) == 16 * 1);
}


static_assert(sizeof(DWORD) == 4);
static_assert(sizeof(FILETIME) == sizeof(DWORD) * 2);


std::wstring MetadataStore::FilenameToKey(std::wstring_view filename) const {
  return ::FilenameToKey(filename, mCaseSensitive);
}


void MetadataStore::LoadFromFileV1() {
  using namespace MetadataFileV1;

  DWORD read = 0;

  if (SetFilePointer(mHFile, 4, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
    throw W32Error();
  }

  EntryCount count = 0;
  if (!ReadFile(mHFile, &count, sizeof(count), &read, NULL) || read != sizeof(count)) {
    throw W32Error();
  }

  EntrySize maxBufferSize = 0;
  std::unique_ptr<char[]> buffer;
  for (std::size_t i = 0; i < count; i++) {
    EntrySize entrySize;
    if (!ReadFile(mHFile, &entrySize, sizeof(entrySize), &read, NULL) || read != sizeof(entrySize)) {
      throw W32Error();
    }

    if (entrySize == 0) {
      // invalid entry
      continue;
    }

    if (entrySize > maxBufferSize) {
      maxBufferSize = entrySize;
      buffer = std::make_unique<char[]>(maxBufferSize);
    }

    if (!ReadFile(mHFile, buffer.get(), entrySize, &read, NULL) || read != entrySize) {
      throw W32Error();
    }

    Metadata metadata;
    const char* ptr = buffer.get();

    const auto entryFlag = *reinterpret_cast<const EntryFlag*>(ptr);
    ptr += sizeof(entryFlag);

    const auto keyNameCount = *reinterpret_cast<const KeyNameCount*>(ptr);
    ptr += sizeof(keyNameCount);
    const auto keyName = reinterpret_cast<const wchar_t*>(ptr);
    ptr += keyNameCount * sizeof(wchar_t);
    const std::wstring key(keyName, keyNameCount);

    if (entryFlag & EntryFlags::HasAttributes) {
      const auto fileAttributes = *reinterpret_cast<const DWORD*>(ptr);
      ptr += sizeof(fileAttributes);
      metadata.fileAttributes = fileAttributes;
    }
    if (entryFlag & EntryFlags::HasCreationTime) {
      const auto creationTime = *reinterpret_cast<const FILETIME*>(ptr);
      ptr += sizeof(creationTime);
      metadata.creationTime = creationTime;
    }
    if (entryFlag & EntryFlags::HasLastAccessTime) {
      const auto lastAccessTime = *reinterpret_cast<const FILETIME*>(ptr);
      ptr += sizeof(lastAccessTime);
      metadata.lastAccessTime = lastAccessTime;
    }
    if (entryFlag & EntryFlags::HasLastWriteTime) {
      const auto lastWriteTime = *reinterpret_cast<const FILETIME*>(ptr);
      ptr += sizeof(lastWriteTime);
      metadata.lastWriteTime = lastWriteTime;
    }
    if (entryFlag & EntryFlags::HasSecurity) {
      const auto securityCount = *reinterpret_cast<const SecurityCount*>(ptr);
      ptr += sizeof(securityCount);
      const auto securityData = reinterpret_cast<const char*>(ptr);
      ptr += securityCount * sizeof(char);
      metadata.security = std::string(securityData, securityCount);
    }

    mMetadataMap.emplace(key, metadata);
  }

  std::uint64_t renameCount;
  if (!ReadFile(mHFile, &renameCount, sizeof(renameCount), &read, NULL) || read != sizeof(renameCount)) {
    throw W32Error();
  }

  for (std::uint64_t i = 0; i < renameCount; i++) {
    std::uint32_t renamedSize;
    if (!ReadFile(mHFile, &renamedSize, sizeof(renamedSize), &read, NULL) || read != sizeof(renamedSize)) {
      throw W32Error();
    }

    std::uint32_t originalSize;
    if (!ReadFile(mHFile, &originalSize, sizeof(originalSize), &read, NULL) || read != sizeof(originalSize)) {
      throw W32Error();
    }

    const std::size_t renamedStrSize = renamedSize * sizeof(wchar_t);
    auto renamedBuffer = std::make_unique<wchar_t[]>(renamedSize);
    if (!ReadFile(mHFile, renamedBuffer.get(), static_cast<DWORD>(renamedStrSize), &read, NULL) || read != renamedStrSize) {
      throw W32Error();
    }
    const std::wstring_view renamed(renamedBuffer.get(), renamedSize);

    const std::size_t originalStrSize = originalSize * sizeof(wchar_t);
    auto originalBuffer = std::make_unique<wchar_t[]>(originalSize);
    if (!ReadFile(mHFile, originalBuffer.get(), static_cast<DWORD>(originalStrSize), &read, NULL) || read != originalStrSize) {
      throw W32Error();
    }
    const std::wstring_view original(originalBuffer.get(), originalSize);

    mRenameStore.AddEntry(original, renamed);
  }
}


bool MetadataStore::LoadFromFileV2() {
  using namespace MetadataFileV2;

  DWORD read = 0;

  LARGE_INTEGER liFileSize;
  if (!GetFileSizeEx(mHFile, &liFileSize)) {
    throw W32Error();
  }
  const std::uint_fast64_t fileSize = liFileSize.QuadPart;

  if (fileSize % Alignment != 0) {
    throw W32Error(ERROR_INVALID_PARAMETER);
  }

  auto fileData = make_unique_aligned<std::byte, Alignment>(fileSize);
  if (!ReadFile(mHFile, fileData.get(), fileSize, &read, NULL) || read != fileSize) {
    throw W32Error();
  }

  // Read Header

  const auto& header = *reinterpret_cast<const Header*>(fileData.get());

  if (header.dataSize % Alignment != 0) {
    throw W32Error(ERROR_INVALID_PARAMETER);
  }

  // Read Rename Entries
  {
    const auto endPtr = const_cast<const std::byte*>(fileData.get()) + header.renameSectionOffset + header.renameSectionSize;
    auto checkPtr = [endPtr] (const std::byte* ptr) {
      if (ptr > endPtr) {
        throw W32Error(ERROR_BUFFER_OVERFLOW);
      }
    };

    auto ptr = const_cast<const std::byte*>(fileData.get()) + header.renameSectionOffset;
    for (std::uint_fast32_t i = 0; i < header.renameSectionCount; i++) {
      std::size_t size = 0;
      const auto renameEntry = RenameEntry::Parse(ptr, checkPtr, size);
      ptr += size;
      if (renameEntry.bSize == 0 || renameEntry.a == renameEntry.b) {
        continue;
      }
      mRenameStore.AddEntry(renameEntry.a, renameEntry.b);
    }

    assert(ptr == endPtr);
  }

  // Read Metadata Entries
  {
    const auto endPtr = const_cast<const std::byte*>(fileData.get()) + header.metadataSectionOffset + header.metadataSectionSize;
    auto checkPtr = [endPtr] (const std::byte* ptr) {
      if (ptr > endPtr) {
        throw W32Error(ERROR_BUFFER_OVERFLOW);
      }
    };

    auto ptr = const_cast<const std::byte*>(fileData.get()) + header.metadataSectionOffset;
    for (std::uint_fast32_t i = 0; i < header.metadataSectionCount; i++) {
      std::size_t size = 0;
      const auto metadataEntry = MetadataEntry::Parse(ptr, checkPtr, size);
      ptr += size;
      if (metadataEntry.flags == 0) {
        continue;
      }
      mMetadataMap.emplace(metadataEntry.filename, static_cast<Metadata>(metadataEntry));
    }

    assert(ptr == endPtr);
  }

  // Read Appendix Entries
  const bool hasAppendix = header.dataSize != fileSize;
  if (hasAppendix) {
    const auto endPtr = const_cast<const std::byte*>(fileData.get()) + fileSize;
    auto checkPtr = [endPtr] (const std::byte* ptr) {
      if (ptr > endPtr) {
        throw W32Error(ERROR_BUFFER_OVERFLOW);
      }
    };

    auto ptr = const_cast<const std::byte*>(fileData.get()) + header.dataSize;
    while (ptr != endPtr) {
      checkPtr(ptr + sizeof(AppendixEntryHeader));
      const auto& appendixEntry = *reinterpret_cast<const AppendixEntryHeader*>(ptr);
      const auto nextPtr = ptr + appendixEntry.blockSize;
      checkPtr(nextPtr);
      ptr += sizeof(AppendixEntryHeader);

      auto checkPtr2 = [nextPtr] (const std::byte* ptr) {
        if (ptr > nextPtr) {
          throw W32Error(ERROR_BUFFER_OVERFLOW);
        }
      };

      std::size_t size = 0;

      switch (appendixEntry.dataType) {
        case AppendixDataType::Rename:
        {
          const auto renameEntry = RenameEntry::Parse(ptr, checkPtr, size);
          if (renameEntry.bSize != 0) {
            mRenameStore.Rename(renameEntry.a, renameEntry.b);
          } else {
            mRenameStore.RemoveEntry(renameEntry.a);
          }
          break;
        }

        case AppendixDataType::Metadata:
        {
          const auto metadataEntry = MetadataEntry::Parse(ptr, checkPtr, size);
          if (metadataEntry.flags != 0) {
            mMetadataMap.insert_or_assign(metadataEntry.filename, static_cast<Metadata>(metadataEntry));
          } else {
            mMetadataMap.erase(metadataEntry.filename);
          }
          break;
        }

        default:
          throw W32Error(ERROR_INVALID_PARAMETER);
      }

      ptr += size;
      assert(ptr == nextPtr);

      ptr = nextPtr;
    }

    assert(ptr == endPtr);
  }

  return hasAppendix;
}


void MetadataStore::LoadFromFile() {
  if (!util::IsValidHandle(mHFile)) {
    throw W32Error(ERROR_INVALID_HANDLE);
  }

  mMetadataMap.clear();
  if (SetFilePointer(mHFile, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
    throw W32Error();
  }

  LARGE_INTEGER fileSize;
  if (!GetFileSizeEx(mHFile, &fileSize)) {
    throw W32Error();
  }
  if (fileSize.QuadPart == 0) {
    // the first time
    return;
  }

  DWORD read = 0;

  std::uint32_t signature = 0;
  if (!ReadFile(mHFile, &signature, sizeof(signature), &read, NULL) || read != sizeof(signature)) {
    throw W32Error();
  }

  if (SetFilePointer(mHFile, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
    throw W32Error();
  }

  switch (signature) {
    case MetadataFileV1::Signature:
      LoadFromFileV1();
      // convert to V2 format
      SaveToFile();
      break;

    case MetadataFileV2::Signature:
      if (LoadFromFileV2()) {
        // has appendix section; remove it
        SaveToFile();
      }
      break;

    default:
      throw W32Error(ERROR_INVALID_PARAMETER);
  }
}


void MetadataStore::SaveToFile() {
  using namespace MetadataFileV2;

  if (!util::IsValidHandle(mHFile)) {
    throw W32Error(ERROR_INVALID_HANDLE);
  }

  const auto renameEntries = mRenameStore.GetEntries();

  // calculate fileSize
  std::size_t fileSize = 0;

  fileSize += sizeof(Header);
  const auto offsetToRenameSection = fileSize;
  for (const auto& [b, a] : renameEntries) {
    fileSize += sizeof(RenameEntryHeader);
    fileSize += Align(a.size() * sizeof(char16_t));
    fileSize += Align(b.size() * sizeof(char16_t));
  }
  const auto offsetToMetadataSection = fileSize;
  for (const auto& [keyName, metadata] : mMetadataMap) {
    fileSize += sizeof(MetadataEntryHeader);
    fileSize += Align(keyName.size() * sizeof(char16_t));
    fileSize += Align(metadata.security ? metadata.security.value().size() : 0);
  }

  assert(fileSize % Alignment == 0);

  auto fileData = make_unique_aligned<std::byte, Alignment>(fileSize);
  std::memset(fileData.get(), 0, fileSize);

  auto ptr = fileData.get();

  auto& header = *reinterpret_cast<Header*>(ptr);
  ptr += sizeof(Header);
  header = Header{
    Signature,
    Version,
    static_cast<std::uint64_t>(fileSize),
    static_cast<std::uint64_t>(offsetToRenameSection),
    static_cast<std::uint64_t>(offsetToMetadataSection - offsetToRenameSection),
    static_cast<std::uint64_t>(renameEntries.size()),
    0,
    static_cast<std::uint64_t>(offsetToMetadataSection),
    static_cast<std::uint64_t>(fileSize - offsetToMetadataSection),
    static_cast<std::uint64_t>(mMetadataMap.size()),
    0,
  };

  for (const auto& [b, a] : renameEntries) {
    const auto prevPtr = ptr;

    auto& entryHeader = *reinterpret_cast<RenameEntryHeader*>(ptr);
    ptr += sizeof(RenameEntryHeader);
    entryHeader = RenameEntryHeader{
      0,    // filled later
      0,
      static_cast<std::uint32_t>(a.size()),
      static_cast<std::uint32_t>(b.size()),
    };

    std::memcpy(ptr, a.c_str(), a.size() * sizeof(char16_t));
    ptr += Align(a.size() * sizeof(char16_t));

    std::memcpy(ptr, b.c_str(), b.size() * sizeof(char16_t));
    ptr += Align(b.size() * sizeof(char16_t));

    entryHeader.blockSize = static_cast<std::uint32_t>(ptr - prevPtr);
  }

  for (const auto& [keyName, metadata] : mMetadataMap) {
    const auto prevPtr = ptr;

    std::uint32_t flags = 0;
    if (metadata.fileAttributes) flags |= EntryFlags::HasAttributes;
    if (metadata.creationTime)   flags |= EntryFlags::HasCreationTime;
    if (metadata.lastAccessTime) flags |= EntryFlags::HasLastAccessTime;
    if (metadata.lastWriteTime)  flags |= EntryFlags::HasLastWriteTime;
    if (metadata.security)       flags |= EntryFlags::HasSecurity;

    auto& entryHeader = *reinterpret_cast<MetadataEntryHeader*>(ptr);
    ptr += sizeof(MetadataEntryHeader);
    entryHeader = MetadataEntryHeader{
      0,    // filled later
      0,
      static_cast<std::uint32_t>(keyName.size()),
      metadata.security ? static_cast<std::uint32_t>(metadata.security.value().size()) : 0,
      flags,
      metadata.fileAttributes ? metadata.fileAttributes.value() : 0,
      metadata.creationTime   ? ReadFILETIME(metadata.creationTime.value())   : 0,
      metadata.lastAccessTime ? ReadFILETIME(metadata.lastAccessTime.value()) : 0,
      metadata.lastWriteTime  ? ReadFILETIME(metadata.lastWriteTime.value())  : 0,
    };

    std::memcpy(ptr, keyName.c_str(), keyName.size() * sizeof(char16_t));
    ptr += Align(keyName.size() * sizeof(char16_t));

    if (metadata.security) {
      const auto& security = metadata.security.value();
      std::memcpy(ptr, security.c_str(), security.size() * sizeof(char16_t));
      ptr += Align(security.size() * sizeof(char16_t));
    }

    entryHeader.blockSize = static_cast<std::uint32_t>(ptr - prevPtr);
  }

  if (SetFilePointer(mHFile, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
    throw W32Error();
  }
  if (!SetEndOfFile(mHFile)) {
    throw W32Error();
  }

  DWORD written = 0;
  if (!WriteFile(mHFile, fileData.get(), fileSize, &written, NULL) || written != fileSize) {
    throw W32Error();
  }
}


void MetadataStore::AddRenameAppendix(std::wstring_view a, std::wstring_view b) {
  using namespace MetadataFileV2;

  const auto alignedASize = Align(a.size() * sizeof(char16_t));
  const auto alignedBSize = Align(b.size() * sizeof(char16_t));

  std::size_t dataSize = 0;
  dataSize += sizeof(AppendixEntryHeader);
  dataSize += sizeof(RenameEntryHeader);
  dataSize += alignedASize;
  dataSize += alignedBSize;

  assert(dataSize % Alignment == 0);

  auto data = make_unique_aligned<std::byte, Alignment>(dataSize);
  std::memset(data.get(), 0, dataSize);

  auto ptr = data.get();

  auto& appendixHeader = *reinterpret_cast<AppendixEntryHeader*>(ptr);
  ptr += sizeof(AppendixEntryHeader);
  appendixHeader = AppendixEntryHeader{
    static_cast<std::uint32_t>(dataSize),
    0,
    AppendixDataType::Rename,
    0,
  };

  auto& entryHeader = *reinterpret_cast<RenameEntryHeader*>(ptr);
  ptr += sizeof(RenameEntryHeader);
  entryHeader = RenameEntryHeader{
    static_cast<std::uint32_t>(dataSize - sizeof(AppendixEntryHeader)),
    0,
    static_cast<std::uint32_t>(a.size()),
    static_cast<std::uint32_t>(b.size()),
  };

  std::memcpy(ptr, a.data(), a.size() * sizeof(char16_t));
  ptr += alignedASize;

  std::memcpy(ptr, b.data(), b.size() * sizeof(char16_t));
  ptr += alignedBSize;

  DWORD written = 0;
  if (!WriteFile(mHFile, data.get(), dataSize, &written, NULL) || written != dataSize) {
    throw W32Error();
  }
}


void MetadataStore::AddRenameAppendix(std::wstring_view a) {
  // on removed
  AddRenameAppendix(a, L""sv);
}


void MetadataStore::AddMetadataAppendix(std::wstring_view keyName, const Metadata& metadata) {
  using namespace MetadataFileV2;

  const auto alignedFilenameSize = Align(keyName.size() * sizeof(char16_t));
  const auto alignedSecuritySize = Align(metadata.security ? metadata.security.value().size() : 0);

  std::size_t dataSize = 0;
  dataSize += sizeof(AppendixEntryHeader);
  dataSize += sizeof(MetadataEntryHeader);
  dataSize += alignedFilenameSize;
  dataSize += alignedSecuritySize;

  assert(dataSize % Alignment == 0);

  auto data = make_unique_aligned<std::byte, Alignment>(dataSize);
  std::memset(data.get(), 0, dataSize);

  auto ptr = data.get();

  auto& appendixHeader = *reinterpret_cast<AppendixEntryHeader*>(ptr);
  ptr += sizeof(AppendixEntryHeader);
  appendixHeader = AppendixEntryHeader{
    static_cast<std::uint32_t>(dataSize),
    0,
    AppendixDataType::Metadata,
    0,
  };

  std::uint32_t flags = 0;
  if (metadata.fileAttributes) flags |= EntryFlags::HasAttributes;
  if (metadata.creationTime)   flags |= EntryFlags::HasCreationTime;
  if (metadata.lastAccessTime) flags |= EntryFlags::HasLastAccessTime;
  if (metadata.lastWriteTime)  flags |= EntryFlags::HasLastWriteTime;
  if (metadata.security)       flags |= EntryFlags::HasSecurity;

  auto& entryHeader = *reinterpret_cast<MetadataEntryHeader*>(ptr);
  ptr += sizeof(MetadataEntryHeader);
  entryHeader = MetadataEntryHeader{
    static_cast<std::uint32_t>(dataSize - sizeof(AppendixEntryHeader)),
    0,
    static_cast<std::uint32_t>(keyName.size()),
    metadata.security ? static_cast<std::uint32_t>(metadata.security.value().size()) : 0,
    flags,
    metadata.fileAttributes ? metadata.fileAttributes.value() : 0,
    metadata.creationTime ? ReadFILETIME(metadata.creationTime.value()) : 0,
    metadata.lastAccessTime ? ReadFILETIME(metadata.lastAccessTime.value()) : 0,
    metadata.lastWriteTime ? ReadFILETIME(metadata.lastWriteTime.value()) : 0,
  };

  std::memcpy(ptr, keyName.data(), keyName.size() * sizeof(char16_t));
  ptr += alignedFilenameSize;

  if (metadata.security) {
    const auto& security = metadata.security.value();
    std::memcpy(ptr, security.c_str(), security.size() * sizeof(char16_t));
    ptr += alignedSecuritySize;
  }

  DWORD written = 0;
  if (!WriteFile(mHFile, data.get(), dataSize, &written, NULL) || written != dataSize) {
    throw W32Error();
  }
}


void MetadataStore::AddMetadataAppendix(std::wstring_view keyName) {
  static const Metadata emptyMetadata{};
  AddMetadataAppendix(keyName, emptyMetadata);
}


MetadataStore::MetadataStore(std::wstring_view storeFileName, bool caseSensitive) :
  mCaseSensitive(caseSensitive),
  mRenameStore(caseSensitive)
{
  if (!storeFileName.empty()) {
    SetFilePath(storeFileName);
  }
}


MetadataStore::~MetadataStore() {
  if (util::IsValidHandle(mHFile)) {
    SaveToFile();
    CloseHandle(mHFile);
    mHFile = NULL;
  }
}


void MetadataStore::SetFilePath(std::wstring_view storeFilename) {
  const bool previousExists = util::IsValidHandle(mHFile);

  if (previousExists) {
    SaveToFile();
    CloseHandle(mHFile);
  }

  const std::wstring sStoreFilename(storeFilename);
  mHFile = CreateFileW(sStoreFilename.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (!mHFile || mHFile == INVALID_HANDLE_VALUE) {
    throw W32Error(GetLastError());
  }

  if (!previousExists) {
    LoadFromFile();
  }
}


std::optional<std::wstring> MetadataStore::ResolveFilepath(std::wstring_view filename) const {
  return mRenameStore.Resolve(filename);
}


bool MetadataStore::HasMetadataR(std::wstring_view resolvedFilename) const {
  if (!util::IsValidHandle(mHFile)) {
    return false;
  }
  return mMetadataMap.count(FilenameToKey(resolvedFilename));
}


bool MetadataStore::HasMetadata(std::wstring_view filename) const {
  const auto resolvedFilenameN = ResolveFilepath(filename);
  if (!resolvedFilenameN) {
    return false;
  }
  return HasMetadataR(resolvedFilenameN.value());
}


const Metadata& MetadataStore::GetMetadataR(std::wstring_view resolvedFilename) const {
  if (!util::IsValidHandle(mHFile)) {
    throw W32Error(ERROR_FILE_NOT_FOUND);
  }
  const std::wstring key = FilenameToKey(resolvedFilename);
  if (!mMetadataMap.count(key)) {
    throw W32Error(ERROR_FILE_NOT_FOUND);
  }
  return mMetadataMap.at(key);
}


const Metadata& MetadataStore::GetMetadata(std::wstring_view filename) const {
  const auto resolvedFilenameN = ResolveFilepath(filename);
  if (!resolvedFilenameN) {
    throw W32Error(ERROR_FILE_NOT_FOUND);
  }
  return GetMetadataR(resolvedFilenameN.value());
}


Metadata MetadataStore::GetMetadata2R(std::wstring_view resolvedFilename) const {
  if (!util::IsValidHandle(mHFile)) {
    return Metadata{};
  }
  const std::wstring key = FilenameToKey(resolvedFilename);
  return mMetadataMap.count(key) ? mMetadataMap.at(key) : Metadata{};
}


Metadata MetadataStore::GetMetadata2(std::wstring_view filename) const {
  const auto resolvedFilenameN = ResolveFilepath(filename);
  if (!resolvedFilenameN) {
    return Metadata{};
  }
  return GetMetadata2R(resolvedFilenameN.value());
}


void MetadataStore::SetMetadataR(std::wstring_view resolvedFilename, const Metadata& metadata) {
  if (!util::IsValidHandle(mHFile)) {
    return;
  }
  const auto key = FilenameToKey(resolvedFilename);
  mMetadataMap.insert_or_assign(key, metadata);
  AddMetadataAppendix(key, metadata);
}


void MetadataStore::SetMetadata(std::wstring_view filename, const Metadata& metadata) {
  const auto resolvedFilenameN = ResolveFilepath(filename);
  if (!resolvedFilenameN) {
    throw W32Error(ERROR_FILE_NOT_FOUND);
  }
  SetMetadataR(resolvedFilenameN.value(), metadata);
}


bool MetadataStore::RemoveMetadataR(std::wstring_view resolvedFilename) {
  if (!util::IsValidHandle(mHFile)) {
    return false;
  }
  const auto key = FilenameToKey(resolvedFilename);
  const auto ret = mMetadataMap.erase(key);
  AddMetadataAppendix(key);
  return ret;
}


bool MetadataStore::RemoveMetadata(std::wstring_view filename) {
  const auto resolvedFilenameN = ResolveFilepath(filename);
  if (!resolvedFilenameN) {
    return false;
  }
  return RemoveMetadataR(resolvedFilenameN.value());
}


bool MetadataStore::ExistsR(std::wstring_view resolvedFilename) const {
  if (!util::IsValidHandle(mHFile)) {
    return true;
  }
  // TODO:
  return true;
}


bool MetadataStore::Exists(std::wstring_view filename) const {
  const auto resolvedFilenameN = ResolveFilepath(filename);
  if (!resolvedFilenameN) {
    return false;
  }
  return ExistsR(resolvedFilenameN.value());
}


std::optional<bool> MetadataStore::ExistsO(std::wstring_view filename) const {
  return mRenameStore.Exists(filename);
}


std::vector<std::pair<std::wstring, std::wstring>> MetadataStore::ListChildrenInForwardLookupTree(std::wstring_view filename) const {
  return mRenameStore.ListChildrenInForwardLookupTree(filename);
}


std::vector<std::pair<std::wstring, std::wstring>> MetadataStore::ListChildrenInReverseLookupTree(std::wstring_view filename) const {
  return mRenameStore.ListChildrenInReverseLookupTree(filename);
}


void MetadataStore::Rename(std::wstring_view srcFilename, std::wstring_view destFilename) {
  if (!util::IsValidHandle(mHFile)) {
    return;
  }
  const auto result = mRenameStore.Rename(srcFilename, destFilename);
  switch (result) {
    case RenameStore::Result::Success:
      break;

    case RenameStore::Result::Invalid:
      throw W32Error(ERROR_ACCESS_DENIED);

    case RenameStore::Result::NotExists:
      throw W32Error(ERROR_FILE_NOT_FOUND);

    case RenameStore::Result::AlreadyExists:
      throw W32Error(ERROR_FILE_EXISTS);

    default:
      throw W32Error(ERROR_GEN_FAILURE);
  }
  
  AddRenameAppendix(srcFilename, destFilename);
}


void MetadataStore::Delete(std::wstring_view filename) {
  if (!util::IsValidHandle(mHFile)) {
    return;
  }
  const auto resolvedFilenameN = ResolveFilepath(filename);
  if (!resolvedFilenameN) {
    return;
  }
  Rename(filename, StrRemovedPrefix + resolvedFilenameN.value());
}


bool MetadataStore::RemoveRenameEntry(std::wstring_view filename) {
  if (!util::IsValidHandle(mHFile)) {
    return false;
  }
  const auto result = mRenameStore.RemoveEntry(filename);
  AddRenameAppendix(filename);
  return result;
}
