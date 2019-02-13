#pragma once

#include <7z/CPP/7zip/IStream.h>
#include <7z/CPP/7zip/Archive/IArchive.h>

#include "NanaZ.hpp"

#include "../SDK/CaseSensitivity.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <Windows.h>
#include <winrt/base.h>


struct DirectoryTree {
  enum class ExtractToMemory : unsigned char {
    Never,
    Auto,
    Always,
  };

  enum class OnExisting : unsigned char {
    Skip,
    Replace,
    RenameNewOne,
  };

  enum class Type : unsigned char {
    File,
    Directory,
    Archive,
  };

  static constexpr wchar_t DirectorySeparator = L'\\';

  static DWORD FilterArchiveFileAttributes(const DirectoryTree& directoryTree);

  std::shared_ptr<std::mutex> streamMutex;
  bool caseSensitive;
  std::unordered_map<std::wstring, DirectoryTree, CaseSensitivity::CiHash, CaseSensitivity::CiEqualTo> children;
  bool valid;
  bool contentAvailable;
  bool onMemory;
  Type type;
  DWORD fileAttributes;
  std::optional<FILETIME> creationTimeN;
  std::optional<FILETIME> lastAccessTimeN;
  std::optional<FILETIME> lastWriteTimeN;
  ULONGLONG fileSize;
  DWORD numberOfLinks;
  ULONGLONG fileIndex;
  winrt::com_ptr<IInStream> inStream;
  winrt::com_ptr<IInArchive> inArchive;
  FILETIME creationTime;
  FILETIME lastAccessTime;
  FILETIME lastWriteTime;
  std::unique_ptr<std::byte[]> extractionMemory;

  const DirectoryTree* Get(std::wstring_view filepath) const;
  bool Exists(std::wstring_view filepath) const;
};


class Archive : DirectoryTree {
public:
  // 7z uses backslash for directory separator
  static constexpr wchar_t DirectorySeparatorFromLibrary = L'\\';

private:
  NanaZ& nanaZ;

public:
  using ArchiveNameCallback = std::function<std::optional<std::pair<std::wstring, bool>>(const std::wstring&, std::size_t)>;
  using PasswordWithFilepathCallback = std::function<std::optional<std::wstring>(const std::wstring&)>;

  Archive(NanaZ& nanaZ, winrt::com_ptr<IInStream> inStream, const BY_HANDLE_FILE_INFORMATION& byHandleFileInformation, const std::wstring& defaultFilepath, std::wstring_view prefixFilter, bool caseSensitive, UInt64 maxCheckStartPosition, OnExisting onExisting, ExtractToMemory extractToMemory, ArchiveNameCallback archiveNameCallback = nullptr, PasswordWithFilepathCallback passwordCallback = nullptr);

  const DirectoryTree* Get(std::wstring_view filepath) const;
  bool Exists(std::wstring_view filepath) const;
};
