#pragma once

#include <dokan/dokan.h>

#include "../SDK/Plugin/Source.h"

#include <optional>
#include <string>
#include <string_view>

#include <Windows.h>


void _SetPluginInitializeInfo(const PLUGIN_INITIALIZE_INFO& pluginInitializeInfo) noexcept;
PLUGIN_INITIALIZE_INFO& GetPluginInitializeInfo() noexcept;
bool IsValidHandle(HANDLE handle) noexcept;
bool IsRootDirectory(LPCWSTR filepath) noexcept;
LARGE_INTEGER CreateLargeInteger(LONGLONG quadPart) noexcept;
NTSTATUS NtstatusFromWin32(DWORD win32ErrorCode = GetLastError()) noexcept;
NTSTATUS NtstatusFromWin32Api(BOOL Result) noexcept;
std::wstring_view GetParentPath(std::wstring_view filename);
std::wstring ToAbsoluteFilepath(LPCWSTR filepath);
std::string ToLowerString(std::string_view string);
