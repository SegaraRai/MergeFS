#include "MetadataStore.hpp"
#include "Metadata.hpp"
#include "Util.hpp"
#include "NsError.hpp"

#include <memory>
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
}


namespace MetadataFile {
  constexpr DWORD Version = 0x000000001;

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


static_assert(sizeof(DWORD) == 4);
static_assert(sizeof(FILETIME) == sizeof(DWORD) * 2);


std::wstring MetadataStore::FilenameToKeyL(std::wstring_view filename) const {
  return ::FilenameToKey(filename, mCaseSensitive);
}


void MetadataStore::LoadFromFileL() {
  using namespace MetadataFile;

  if (!IsValidHandle(mHFile)) {
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

  DWORD version = 0;
  if (!ReadFile(mHFile, &version, sizeof(version), &read, NULL) || read != sizeof(version)) {
    throw W32Error();
  }

  if (version != Version) {
    throw W32Error(ERROR_INVALID_PARAMETER);
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


void MetadataStore::SaveToFileL() {
  using namespace MetadataFile;

  if (!IsValidHandle(mHFile)) {
    throw W32Error(ERROR_INVALID_HANDLE);
  }

  if (SetFilePointer(mHFile, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
    throw W32Error();
  }
  if (!SetEndOfFile(mHFile)) {
    throw W32Error();
  }

  DWORD written = 0;

  const DWORD version = Version;
  if (!WriteFile(mHFile, &version, sizeof(version), &written, NULL) || written != sizeof(version)) {
    throw W32Error();
  }

  const EntryCount count = mMetadataMap.size();
  if (!WriteFile(mHFile, &count, sizeof(count), &written, NULL) || written != sizeof(count)) {
    throw W32Error();
  }

  for (const auto& kvPair : mMetadataMap) {
    const auto& keyName = kvPair.first;
    const auto& metadata = kvPair.second;

    const auto securityCount = metadata.security ? metadata.security.value().size() : 0;
    const auto keyNameCount = keyName.size();

    const std::size_t maxBufferSize =
      sizeof(EntrySize)
      + sizeof(EntryFlag)
      + sizeof(KeyNameCount)
      + keyNameCount * sizeof(wchar_t)
      + sizeof(DWORD)
      + sizeof(FILETIME)
      + sizeof(FILETIME)
      + sizeof(FILETIME)
      + sizeof(SecurityCount)
      + securityCount * sizeof(char);

    auto buffer = std::make_unique<char[]>(maxBufferSize);

    char* ptr = buffer.get();

    ptr += sizeof(EntrySize);
    ptr += sizeof(EntryFlag);

    std::memcpy(ptr, &keyNameCount, sizeof(keyNameCount));
    ptr += sizeof(keyNameCount);
    std::memcpy(ptr, keyName.data(), keyNameCount * sizeof(wchar_t));
    ptr += keyNameCount * sizeof(wchar_t);

    EntryFlag entryFlag = 0;

    if (metadata.fileAttributes) {
      entryFlag |= EntryFlags::HasAttributes;
      const auto fileAttributes = metadata.fileAttributes.value();
      std::memcpy(ptr, &fileAttributes, sizeof(fileAttributes));
      ptr += sizeof(fileAttributes);
    }
    if (metadata.creationTime) {
      entryFlag |= EntryFlags::HasCreationTime;
      const auto creationTime = metadata.creationTime.value();
      std::memcpy(ptr, &creationTime, sizeof(creationTime));
      ptr += sizeof(creationTime);
    }
    if (metadata.lastAccessTime) {
      entryFlag |= EntryFlags::HasLastAccessTime;
      const auto lastAccessTime = metadata.lastAccessTime.value();
      std::memcpy(ptr, &lastAccessTime, sizeof(lastAccessTime));
      ptr += sizeof(lastAccessTime);
    }
    if (metadata.lastWriteTime) {
      entryFlag |= EntryFlags::HasLastWriteTime;
      const auto lastWriteTime = metadata.lastWriteTime.value();
      std::memcpy(ptr, &lastWriteTime, sizeof(lastWriteTime));
      ptr += sizeof(lastWriteTime);
    }
    if (metadata.security) {
      std::memcpy(ptr, &securityCount, sizeof(securityCount));
      ptr += sizeof(securityCount);
      std::memcpy(ptr, metadata.security.value().data(), securityCount * sizeof(char));
      ptr += securityCount * sizeof(char);
    }

    // set EntrySize and EntryFlag
    const EntrySize entrySize = static_cast<EntrySize>(ptr - buffer.get() - sizeof(EntrySize));
    ptr = buffer.get();
    std::memcpy(ptr, &entrySize, sizeof(entrySize));
    ptr += sizeof(entrySize);
    std::memcpy(ptr, &entryFlag, sizeof(entryFlag));

    // write
    const std::size_t bufferSize = entrySize + sizeof(entrySize);
    if (!WriteFile(mHFile, buffer.get(), static_cast<DWORD>(bufferSize), &written, NULL) || written != bufferSize) {
      throw W32Error();
    }
  }

  const auto renameEntries = mRenameStore.GetEntries();

  const std::uint64_t renameCount = renameEntries.size();
  if (!WriteFile(mHFile, &renameCount, sizeof(renameCount), &written, NULL) || written != sizeof(renameCount)) {
    throw W32Error();
  }

  for (auto& [renamed, original] : renameEntries) {
    DWORD written = 0;

    const std::uint32_t renamedSize = static_cast<std::uint32_t>(renamed.size());
    if (!WriteFile(mHFile, &renamedSize, sizeof(renamedSize), &written, NULL) || written != sizeof(renamedSize)) {
      throw W32Error();
    }

    const std::uint32_t originalSize = static_cast<std::uint32_t>(original.size());
    if (!WriteFile(mHFile, &originalSize, sizeof(originalSize), &written, NULL) || written != sizeof(originalSize)) {
      throw W32Error();
    }

    const std::size_t renamedStrSize = renamedSize * sizeof(wchar_t);
    if (!WriteFile(mHFile, renamed.data(), static_cast<DWORD>(renamedStrSize), &written, NULL) || written != renamedStrSize) {
      throw W32Error();
    }

    const std::size_t originalStrSize = originalSize * sizeof(wchar_t);
    if (!WriteFile(mHFile, original.data(), static_cast<DWORD>(originalStrSize), &written, NULL) || written != originalStrSize) {
      throw W32Error();
    }
  }
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
  std::lock_guard lock(mMutex);
  if (IsValidHandle(mHFile)) {
    CloseHandle(mHFile);
    mHFile = NULL;
  }
}


void MetadataStore::SetFilePath(std::wstring_view storeFilename) {
  std::lock_guard lock(mMutex);

  const bool previousExists = IsValidHandle(mHFile);

  if (previousExists) {
    SaveToFileL();
    CloseHandle(mHFile);
  }

  const std::wstring sStoreFilename(storeFilename);
  mHFile = CreateFileW(sStoreFilename.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (!mHFile || mHFile == INVALID_HANDLE_VALUE) {
    throw W32Error(GetLastError());
  }

  if (!previousExists) {
    LoadFromFileL();
  }
}


std::optional<std::wstring> MetadataStore::ResolveFilepathL(std::wstring_view filename) const {
  return mRenameStore.Resolve(filename);
}


std::optional<std::wstring> MetadataStore::ResolveFilepath(std::wstring_view filename) {
  std::shared_lock lock(mMutex);
  return ResolveFilepathL(filename);
}


bool MetadataStore::HasMetadataRL(std::wstring_view resolvedFilename) const {
  if (!IsValidHandle(mHFile)) {
    return false;
  }
  return mMetadataMap.count(FilenameToKeyL(resolvedFilename));
}


bool MetadataStore::HasMetadataR(std::wstring_view resolvedFilename) {
  std::shared_lock lock(mMutex);
  return HasMetadataRL(resolvedFilename);
}


bool MetadataStore::HasMetadata(std::wstring_view filename) {
  std::shared_lock lock(mMutex);
  const auto resolvedFilenameN = ResolveFilepathL(filename);
  if (!resolvedFilenameN) {
    return false;
  }
  return HasMetadataRL(resolvedFilenameN.value());
}


const Metadata& MetadataStore::GetMetadataRL(std::wstring_view resolvedFilename) const {
  if (!IsValidHandle(mHFile)) {
    throw W32Error(ERROR_FILE_NOT_FOUND);
  }
  const std::wstring key = FilenameToKeyL(resolvedFilename);
  if (!mMetadataMap.count(key)) {
    throw W32Error(ERROR_FILE_NOT_FOUND);
  }
  return mMetadataMap.at(key);
}


const Metadata& MetadataStore::GetMetadataR(std::wstring_view resolvedFilename) {
  std::shared_lock lock(mMutex);
  return GetMetadataRL(resolvedFilename);
}


const Metadata& MetadataStore::GetMetadata(std::wstring_view filename) {
  std::shared_lock lock(mMutex);
  const auto resolvedFilenameN = ResolveFilepathL(filename);
  if (!resolvedFilenameN) {
    throw W32Error(ERROR_FILE_NOT_FOUND);
  }
  return GetMetadataRL(resolvedFilenameN.value());
}


Metadata MetadataStore::GetMetadata2RL(std::wstring_view resolvedFilename) const {
  if (!IsValidHandle(mHFile)) {
    return Metadata{};
  }
  const std::wstring key = FilenameToKeyL(resolvedFilename);
  return mMetadataMap.count(key) ? mMetadataMap.at(key) : Metadata{};
}


Metadata MetadataStore::GetMetadata2R(std::wstring_view resolvedFilename) {
  std::shared_lock lock(mMutex);
  return GetMetadata2RL(resolvedFilename);
}


Metadata MetadataStore::GetMetadata2(std::wstring_view filename) {
  std::shared_lock lock(mMutex);
  const auto resolvedFilenameN = ResolveFilepathL(filename);
  if (!resolvedFilenameN) {
    return Metadata{};
  }
  return GetMetadata2RL(resolvedFilenameN.value());
}


void MetadataStore::SetMetadataRL(std::wstring_view resolvedFilename, const Metadata& metadata) {
  if (!IsValidHandle(mHFile)) {
    return;
  }
  mMetadataMap.insert_or_assign(FilenameToKeyL(resolvedFilename), metadata);
  SaveToFileL();
}


void MetadataStore::SetMetadataR(std::wstring_view resolvedFilename, const Metadata& metadata) {
  std::lock_guard lock(mMutex);
  SetMetadataRL(resolvedFilename, metadata);
}


void MetadataStore::SetMetadata(std::wstring_view filename, const Metadata& metadata) {
  std::lock_guard lock(mMutex);
  const auto resolvedFilenameN = ResolveFilepathL(filename);
  if (!resolvedFilenameN) {
    throw W32Error(ERROR_FILE_NOT_FOUND);
  }
  SetMetadataRL(resolvedFilenameN.value(), metadata);
}


bool MetadataStore::RemoveMetadataRL(std::wstring_view resolvedFilename) {
  if (!IsValidHandle(mHFile)) {
    return false;
  }
  const auto ret = mMetadataMap.erase(FilenameToKeyL(resolvedFilename));
  SaveToFileL();
  return ret;
}


bool MetadataStore::RemoveMetadataR(std::wstring_view resolvedFilename) {
  std::lock_guard lock(mMutex);
  return RemoveMetadataRL(resolvedFilename);
}


bool MetadataStore::RemoveMetadata(std::wstring_view filename) {
  std::lock_guard lock(mMutex);
  const auto resolvedFilenameN = ResolveFilepathL(filename);
  if (!resolvedFilenameN) {
    return false;
  }
  return RemoveMetadataRL(resolvedFilenameN.value());
}


bool MetadataStore::ExistsRL(std::wstring_view resolvedFilename) const {
  if (!IsValidHandle(mHFile)) {
    return true;
  }
  /*
  if (resolvedFilename.size() > StrRemovedPrefixB.size() && resolvedFilename.substr(0, StrRemovedPrefixB.size()) == StrRemovedPrefixB) {
    return false;
  }
  //*/
  return true;
}


bool MetadataStore::ExistsR(std::wstring_view resolvedFilename) {
  std::shared_lock lock(mMutex);
  return ExistsRL(resolvedFilename);
}


bool MetadataStore::Exists(std::wstring_view filename) {
  std::shared_lock lock(mMutex);
  const auto resolvedFilenameN = ResolveFilepathL(filename);
  if (!resolvedFilenameN) {
    return false;
  }
  return ExistsRL(resolvedFilenameN.value());
}


std::optional<bool> MetadataStore::ExistsO(std::wstring_view filename) {
  std::shared_lock lock(mMutex);
  return mRenameStore.Exists(filename);
}


std::vector<std::pair<std::wstring, std::wstring>> MetadataStore::ListChildrenInForwardLookupTree(std::wstring_view filename) {
  std::shared_lock lock(mMutex);
  return mRenameStore.ListChildrenInForwardLookupTree(filename);
}


std::vector<std::pair<std::wstring, std::wstring>> MetadataStore::ListChildrenInReverseLookupTree(std::wstring_view filename) {
  std::shared_lock lock(mMutex);
  return mRenameStore.ListChildrenInReverseLookupTree(filename);
}


void MetadataStore::RenameL(std::wstring_view srcFilename, std::wstring_view destFilename) {
  if (!IsValidHandle(mHFile)) {
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
  SaveToFileL();
}


void MetadataStore::Rename(std::wstring_view srcFilename, std::wstring_view destFilename) {
  std::lock_guard lock(mMutex);
  RenameL(srcFilename, destFilename);
}


void MetadataStore::Delete(std::wstring_view filename) {
  std::lock_guard lock(mMutex);
  if (!IsValidHandle(mHFile)) {
    return;
  }
  const auto resolvedFilenameN = ResolveFilepathL(filename);
  if (!resolvedFilenameN) {
    return;
  }
  RenameL(filename, StrRemovedPrefix + resolvedFilenameN.value());
}


bool MetadataStore::RemoveRenameEntry(std::wstring_view filename) {
  std::lock_guard lock(mMutex);
  if (!IsValidHandle(mHFile)) {
    return false;
  }
  const auto result = mRenameStore.RemoveEntry(filename);
  SaveToFileL();
  return result;
}
