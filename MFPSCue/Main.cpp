#define NOMINMAX

#include <dokan/dokan.h>

#include "../SDK/Plugin/SourceCpp.hpp"

#include "CueSourceMount.hpp"
#include "Source.hpp"
#include "Util.hpp"

#include <memory>

#include <Windows.h>
#include <winrt/base.h>

using namespace std::literals;



namespace {
  // {8F3ACF43-D9DD-0000-1010-500000000000}
  constexpr GUID DPluginGUID = {0x8F3ACF43, 0xD9DD, 0x0000, {0x10, 0x10, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}};

  constexpr DWORD DMaximumComponentLength = MAX_PATH;
  constexpr DWORD DFileSystemFlags = FILE_CASE_PRESERVED_NAMES | FILE_CASE_SENSITIVE_SEARCH | FILE_UNICODE_ON_DISK;

  const PLUGIN_INFO gPluginInfo = {
    MERGEFS_PLUGIN_INTERFACE_VERSION,
    PLUGIN_TYPE::Source,
    DPluginGUID,
    L"Cuesheet",
    L"Cuesheet source plugin",
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


BOOL SIsSupportedImpl(LPCWSTR FileName) noexcept {
  try {
    const std::wstring_view filepath(FileName);
    if (filepath.size() < 4) {
      return FALSE;
    }
    const wchar_t* ptrEndOfFilepath = filepath.data() + filepath.size();
    if (ptrEndOfFilepath[-4] != L'.' || (ptrEndOfFilepath[-3] != L'C' && ptrEndOfFilepath[-3] != L'c') || (ptrEndOfFilepath[-2] != L'U' && ptrEndOfFilepath[-2] != L'u') || (ptrEndOfFilepath[-1] != L'E' && ptrEndOfFilepath[-1] != L'e')) {
      return FALSE;
    }
    const auto fileAttributes = GetFileAttributesW(FileName);
    if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
      return FALSE;
    }
    if (fileAttributes != FILE_ATTRIBUTE_NORMAL && (fileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      return FALSE;
    }
    return TRUE;
  } catch (...) {
    return FALSE;
  }
}



std::unique_ptr<SourceMountBase> MountImpl(LPCWSTR FileName, SOURCE_CONTEXT_ID sourceContextId) {
  return std::make_unique<CueSourceMount>(FileName, sourceContextId);
}
