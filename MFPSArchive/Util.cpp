#include "Util.hpp"

#include <Windows.h>

#include <cassert>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

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


std::wstring_view GetParentPath(std::wstring_view filename) {
  // ルートディレクトリの親ディレクトリを取得しようとするべきではない
  assert(filename.size() > 1);
  const std::size_t lastSlashPos = filename.find_last_of(L'\\');
  if (lastSlashPos == 0) {
    // root直下
    return L"\\"sv;
  }
  return filename.substr(0, lastSlashPos);
  //return std::wstring{filename.cbegin(), filename.cbegin() + lastSlashPos};
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
