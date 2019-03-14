#pragma comment(lib, "Shlwapi.lib")

#define NOMINMAX

#include <dokan/dokan.h>

#include "../SDK/Plugin/SourceCpp.hpp"

#include <Windows.h>
#include <shlwapi.h>

#include "Util.hpp"
#include "FilesystemSourceMount.hpp"

using namespace std::literals;



namespace {
  // {8F3ACF43-D9DD-0000-1010-200000000000}
  constexpr GUID DPluginGUID = {0x8F3ACF43, 0xD9DD, 0x0000, {0x10, 0x10, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00}};

  constexpr DWORD DMaximumComponentLength = MAX_PATH;
  constexpr DWORD DFileSystemFlags = FILE_CASE_PRESERVED_NAMES | FILE_CASE_SENSITIVE_SEARCH | FILE_UNICODE_ON_DISK;

  const PLUGIN_INFO gPluginInfo = {
    MERGEFS_PLUGIN_INTERFACE_VERSION,
    PLUGIN_TYPE::Source,
    DPluginGUID,
    L"filesystemfs",
    L"filesystem source plugin",
    0x00000001,
    L"0.0.1",
  };

  PLUGIN_INITIALIZE_INFO gPluginInitializeInfo{};
}



BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
  return TRUE;
}



const PLUGIN_INFO* SGetPluginInfoImpl() noexcept {
  return &gPluginInfo;
}


PLUGIN_INITCODE SInitializeImpl(const PLUGIN_INITIALIZE_INFO* InitializeInfo) noexcept {
  gPluginInitializeInfo = *InitializeInfo;
  _SetPluginInitializeInfo(*InitializeInfo);
  return PLUGIN_INITCODE::Success;
}


BOOL SIsSupportedImpl(const PLUGIN_INITIALIZE_MOUNT_INFO* InitializeMountInfo) noexcept {
  return PathIsDirectoryW(InitializeMountInfo->FileName);
}



std::unique_ptr<SourceMountBase> MountImpl(const PLUGIN_INITIALIZE_MOUNT_INFO* InitializeMountInfo, SOURCE_CONTEXT_ID sourceContextId) {
  return std::make_unique<FilesystemSourceMount>(InitializeMountInfo, sourceContextId);
}
