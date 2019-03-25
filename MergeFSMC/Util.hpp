#pragma once

#include <Windows.h>


enum class SystemSound {
  DeviceConnect,
  DeviceDisconnect,
};


template <SystemSound Sound>
void PlaySystemSound() {
  static constexpr auto GetSystemSoundName = [](SystemSound systemSound) constexpr -> LPCWSTR {
    switch (systemSound) {
      case SystemSound::DeviceConnect:
        return L"DeviceConnect";

      case SystemSound::DeviceDisconnect:
        return L"DeviceDisconnect";
    }
    return nullptr;
  };

  constexpr LPCWSTR SoundName = GetSystemSoundName(Sound);
  static_assert(SoundName != nullptr);

  PlaySoundW(SoundName, NULL, SND_ALIAS | SND_NODEFAULT | SND_ASYNC);
}
