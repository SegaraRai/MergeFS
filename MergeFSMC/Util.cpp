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
