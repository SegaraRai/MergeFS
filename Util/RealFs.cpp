#include <cassert>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include <Windows.h>

#include "RealFs.hpp"

using namespace std::literals;



namespace util::rfs {
  std::wstring GetParentPath(const std::wstring& filepath) {
    return ToAbsoluteFilepath(filepath + L"\\.."s);
  }


  std::wstring GetParentPath(std::wstring_view filepath) {
    return GetParentPath(std::wstring(filepath));
  }


  std::wstring GetParentPath(LPCWSTR filepath) {
    return GetParentPath(std::wstring(filepath));
  }


  std::wstring ToAbsoluteFilepath(LPCWSTR filepath) {
    const DWORD size = GetFullPathNameW(filepath, 0, NULL, NULL);
    if (!size) {
      throw std::runtime_error("frist call of GetFullPathNameW failed");
    }
    auto buffer = std::make_unique<wchar_t[]>(size);
    DWORD size2 = GetFullPathNameW(filepath, size, buffer.get(), NULL);
    if (!size2) {
      throw std::runtime_error("second call of GetFullPathNameW failed");
    }
    // trim trailing backslash
    if (buffer[size2 - 1] == L'\\') {
      buffer[--size2] = L'\0';
    }
    return std::wstring(buffer.get(), size2);
  }


  std::wstring ToAbsoluteFilepath(const std::wstring& filepath) {
    return ToAbsoluteFilepath(filepath.c_str());
  }


  std::wstring GetModuleFilepath(HMODULE hModule) {
    constexpr std::size_t BufferSize = 65600;

    auto buffer = std::make_unique<wchar_t[]>(BufferSize);
    GetModuleFileNameW(hModule, buffer.get(), BufferSize);
    if (GetLastError() != ERROR_SUCCESS) {
      throw std::runtime_error("GetModukeFileNameW failed");
    }

    return std::wstring(buffer.get());
  }
}
