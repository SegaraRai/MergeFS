#pragma once

#include <string>
#include <string_view>

#include <Windows.h>


namespace util::vfs {
  // functions for virtual (i.e. Dokan's) filesystem where paths are provided by Dokan and represented like "\\abc\\def.txt"

  bool IsRootDirectory(LPCWSTR filepath) noexcept;
  bool IsRootDirectory(std::wstring_view filepath) noexcept;
  std::wstring_view GetParentPath(std::wstring_view filepath);
  std::wstring_view GetParentPathTs(std::wstring_view filepath);
  std::wstring_view GetBaseName(std::wstring_view filepath);
}
