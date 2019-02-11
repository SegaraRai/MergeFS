#pragma once

#include <optional>
#include <string>

#include <Windows.h>


struct Metadata {
  std::optional<DWORD> fileAttributes;
  std::optional<FILETIME> creationTime;
  std::optional<FILETIME> lastAccessTime;
  std::optional<FILETIME> lastWriteTime;
  std::optional<std::string> security;
};
