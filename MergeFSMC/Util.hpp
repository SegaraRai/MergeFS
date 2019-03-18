#pragma once

#include <string>

#include <Windows.h>


enum class SystemSound {
  DeviceConnect,
  DeviceDisconnect,
};


void PlaySystemSound(SystemSound systemSound);
