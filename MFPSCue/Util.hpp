#pragma once

#include <dokan/dokan.h>

#include "../SDK/Plugin/Source.h"

#include <optional>
#include <string>
#include <string_view>

#include <Windows.h>


void _SetPluginInitializeInfo(const PLUGIN_INITIALIZE_INFO& pluginInitializeInfo) noexcept;
PLUGIN_INITIALIZE_INFO& GetPluginInitializeInfo() noexcept;
NTSTATUS NtstatusFromWin32(DWORD win32ErrorCode = GetLastError()) noexcept;
NTSTATUS NtstatusFromWin32Api(BOOL Result) noexcept;
