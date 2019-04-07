#pragma once

#include <string>
#include <Windows.h>


namespace util {
  constexpr bool IsValidHandle(HANDLE handle) noexcept {
    return handle != NULL && handle != INVALID_HANDLE_VALUE;
  }

  constexpr LARGE_INTEGER CreateLargeInteger(LONGLONG quadPart) noexcept {
    LARGE_INTEGER li{};
    li.QuadPart = quadPart;
    return li;
  }

  std::string ToLowerString(std::string_view string);
}
