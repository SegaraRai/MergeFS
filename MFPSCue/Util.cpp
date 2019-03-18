#include <dokan/dokan.h>

#include <array>
#include <cassert>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <Windows.h>

#include "Util.hpp"

using namespace std::literals;



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


NTSTATUS NtstatusFromWin32(DWORD win32ErrorCode) noexcept {
  return gPluginInitializeInfo.dokanFuncs.DokanNtStatusFromWin32 ? gPluginInitializeInfo.dokanFuncs.DokanNtStatusFromWin32(win32ErrorCode) : STATUS_UNSUCCESSFUL;
}


NTSTATUS NtstatusFromWin32Api(BOOL result) noexcept {
  return result ? STATUS_SUCCESS : NtstatusFromWin32();
}
