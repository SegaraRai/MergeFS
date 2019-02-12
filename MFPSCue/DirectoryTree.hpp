#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include <Windows.h>

#include "Source.hpp"

#include "../SDK/CaseSensitivity.hpp"


struct DirectoryTree {
  static constexpr wchar_t DirectorySeparator = L'\\';
  static constexpr DWORD FileFileAttributes = FILE_ATTRIBUTE_NORMAL;
  static constexpr DWORD DirectoryFileAttributes = FILE_ATTRIBUTE_DIRECTORY;

  bool caseSensitive;
  bool directory;
  ULONGLONG fileIndex;
  std::shared_ptr<Source> source;
  std::unordered_map<std::wstring, DirectoryTree, CaseSensitivity::CiHash, CaseSensitivity::CiEqualTo> children;

  const DirectoryTree* Get(std::wstring_view filepath) const;
  bool Exists(std::wstring_view filepath) const;
};
