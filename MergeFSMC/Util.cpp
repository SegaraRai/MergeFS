#include <cassert>
#include <memory>
#include <stdexcept>
#include <string>

#include <Windows.h>

#include "Util.hpp"



void PlaySystemSound(SystemSound systemSound) {
  LPCWSTR soundName = nullptr;
  switch (systemSound) {
    case SystemSound::DeviceConnect:
      soundName = L"DeviceConnect";
      break;

    case SystemSound::DeviceDisconnect:
      soundName = L"DeviceDisconnect";
      break;
  }
  assert(soundName != nullptr);
  if (!soundName) {
    return;
  }
  PlaySoundW(soundName, NULL, SND_ALIAS | SND_NODEFAULT | SND_SYNC);
}


std::wstring ToAbsoluteFilepath(const std::wstring& filepath) {
  const DWORD size = GetFullPathNameW(filepath.c_str(), 0, NULL, NULL);
  if (!size) {
    throw std::runtime_error("frist call of GetFullPathNameW failed");
  }
  auto buffer = std::make_unique<wchar_t[]>(size);
  DWORD size2 = GetFullPathNameW(filepath.c_str(), size, buffer.get(), NULL);
  if (!size2) {
    throw std::runtime_error("second call of GetFullPathNameW failed");
  }
  // trim trailing backslash
  if (buffer[size2 - 1] == L'\\') {
    buffer[--size2] = L'\0';
  }
  return std::wstring(buffer.get(), size2);
}
