#include <dokan/dokan.h>

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


// filepath must be an absolute path
std::optional<std::wstring> FindRootFilepath(std::wstring_view filepath) {
  const std::size_t filepathLength = filepath.size();
  auto currentFilepath = std::make_unique<wchar_t[]>(filepathLength + 1);
  std::memcpy(currentFilepath.get(), filepath.data(), filepathLength * sizeof(wchar_t));
  currentFilepath[filepathLength] = L'\0';
  std::size_t currentFilepathLength = filepathLength;
  for (;;) {
    if (const DWORD fileAttributes = GetFileAttributesW(currentFilepath.get()); fileAttributes != INVALID_FILE_ATTRIBUTES) {
      const bool directory = fileAttributes != FILE_ATTRIBUTE_NORMAL && (fileAttributes & FILE_ATTRIBUTE_DIRECTORY);
      if (directory) {
        return std::nullopt;
      }
      break;
    }
    if (const DWORD error = GetLastError(); error == ERROR_BAD_NETPATH) {
      return std::nullopt;
    } else if (error != ERROR_PATH_NOT_FOUND && error != ERROR_FILE_NOT_FOUND) {
      throw std::runtime_error("GetFileAttributesW failed with unknown error");
    }
    const auto lastSlashPos = std::wstring_view(currentFilepath.get(), currentFilepathLength).find_last_of(L'\\');
    if (lastSlashPos == std::wstring_view::npos) {
      return std::nullopt;
    }
    currentFilepathLength = lastSlashPos;
    currentFilepath[currentFilepathLength] = L'\0';
  }
  return std::make_optional<std::wstring>(currentFilepath.get(), currentFilepathLength);
}
