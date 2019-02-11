#pragma once


#include <string>
#include <string_view>

#include <Windows.h>


bool IsValidHandle(HANDLE handle) noexcept;
std::wstring FilenameToKey(std::wstring_view filename, bool caseSensitive);
std::wstring_view GetParentPath(std::wstring_view filename);
std::wstring_view GetBaseName(std::wstring_view filename);
bool IsRootDirectory(std::wstring_view filename) noexcept;
