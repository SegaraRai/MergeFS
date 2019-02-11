#include "Util.hpp"

#include <Windows.h>

#include <cassert>


namespace {
  bool gPluginInitializeInfoSet = false;
  PLUGIN_INITIALIZE_INFO gPluginInitializeInfo;
}


void _SetPluginInitializeInfo(const PLUGIN_INITIALIZE_INFO& pluginInitializeInfo) noexcept {
  assert(!gPluginInitializeInfoSet);
  if (gPluginInitializeInfoSet) {
    return;
  }
  gPluginInitializeInfo = pluginInitializeInfo;
  gPluginInitializeInfoSet = true;
}


PLUGIN_INITIALIZE_INFO& GetPluginInitializeInfo() noexcept {
  return gPluginInitializeInfo;
}


bool IsValidHandle(HANDLE handle) noexcept {
  return handle != NULL && handle != INVALID_HANDLE_VALUE;
}


bool IsRootDirectory(LPCWSTR filepath) noexcept {
  assert(filepath && filepath[0] == L'\\');
  return filepath[1] == L'\0';
}


LARGE_INTEGER CreateLargeInteger(LONGLONG quadPart) noexcept {
  LARGE_INTEGER li;
  li.QuadPart = quadPart;
  return li;
}


NTSTATUS NtstatusFromWin32(DWORD win32ErrorCode) noexcept {
  return gPluginInitializeInfo.dokanFuncs.DokanNtStatusFromWin32 ? gPluginInitializeInfo.dokanFuncs.DokanNtStatusFromWin32(win32ErrorCode) : STATUS_UNSUCCESSFUL;
}


NTSTATUS NtstatusFromWin32Api(BOOL result) noexcept {
  return result ? STATUS_SUCCESS : NtstatusFromWin32();
}
