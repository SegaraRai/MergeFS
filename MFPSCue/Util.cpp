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


std::string ToLowerString(std::string_view string) {
  constexpr auto LowerCharTable = ([]() constexpr {
    static_assert(CHAR_BIT == 8);
    std::array<char, 256> table{};
    for (int i = 0; i < 256; i++) {
      table[i] = 'A' <= i && i <= 'Z'
        ? i - 'A' + 'a'
        : i;
    }
    return table;
  })();

  std::string lowerString(string.size(), '\0');
  for (std::size_t i = 0; i < string.size(); i++) {
    lowerString[i] = LowerCharTable[static_cast<unsigned int>(string[i])];
  }
  return lowerString;
}
