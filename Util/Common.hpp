#pragma once

#include <string>
#include <Windows.h>


namespace util {
  bool IsValidHandle(HANDLE handle) noexcept;
  LARGE_INTEGER CreateLargeInteger(LONGLONG quadPart) noexcept;
  std::string ToLowerString(std::string_view string);
}
