#pragma once

#include <string>

#include <Windows.h>


enum class SystemSound {
  DeviceConnect,
  DeviceDisconnect,
};


void PlaySystemSound(SystemSound systemSound);
std::wstring ToAbsoluteFilepath(const std::wstring& filepath);
