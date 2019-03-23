#pragma once

#include <string>
#include <string_view>

#include <Windows.h>


namespace util::rfs {
  // functions for real filesystem where paths are represented like "C:\\abc\\def.txt", "\\\\example\\abc\\def.txt" or something like that

  std::wstring GetParentPath(const std::wstring& filepath);
  std::wstring GetParentPath(std::wstring_view filepath);
  std::wstring GetParentPath(LPCWSTR filepath);
  std::wstring_view GetBaseNameAbs(std::wstring_view absoluteFilepath);
  std::wstring GetBaseName(LPCWSTR filepath);
  std::wstring GetBaseName(const std::wstring& filepath);
  std::wstring ToAbsoluteFilepath(LPCWSTR filepath);
  std::wstring ToAbsoluteFilepath(const std::wstring& filepath);
  std::wstring GetModuleFilepath(HMODULE hInstance = NULL);
}
