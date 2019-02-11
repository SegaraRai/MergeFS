#include "PluginBase.hpp"

#include <stdexcept>
#include <string>
#include <string_view>

using namespace std::literals;



std::unordered_set<GUID, GUIDHash> PluginBase::gPluginGuidSet{};



namespace {
  namespace DokanFuncs {
    void WINAPI DokanMapKernelToUserCreateFileFlags(ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG CreateOptions, ULONG CreateDisposition, ACCESS_MASK* outDesiredAccess, DWORD* outFileAttributesAndFlags, DWORD* outCreationDisposition) noexcept {
      return ::DokanMapKernelToUserCreateFileFlags(DesiredAccess, FileAttributes, CreateOptions, CreateDisposition, outDesiredAccess, outFileAttributesAndFlags, outCreationDisposition);
    }


    NTSTATUS WINAPI DokanNtStatusFromWin32(DWORD Error) noexcept {
      return ::DokanNtStatusFromWin32(Error);
    }


    BOOL WINAPI DokanIsNameInExpression(LPCWSTR Expression, LPCWSTR Name, BOOL IgnoreCase) noexcept {
      return ::DokanIsNameInExpression(Expression, Name, IgnoreCase);
    }


    BOOL WINAPI DokanResetTimeout(ULONG Timeout, PDOKAN_FILE_INFO DokanFileInfo) noexcept {
      return ::DokanResetTimeout(Timeout, DokanFileInfo);
    }


    HANDLE WINAPI DokanOpenRequestorToken(PDOKAN_FILE_INFO DokanFileInfo) noexcept {
      return ::DokanOpenRequestorToken(DokanFileInfo);
    }


    BOOL WINAPI DokanGetMountPointList(PDOKAN_CONTROL list, ULONG length, BOOL uncOnly, PULONG nbRead) noexcept {
      return ::DokanGetMountPointList(list, length, uncOnly, nbRead);
    }
  }
}



PluginBase::DLL::DLL(LPCWSTR FileName) : hModule(LoadLibraryW(FileName)) {
  if (!hModule) {
    throw std::runtime_error(u8"failed to load library");
  }
}


PluginBase::DLL::~DLL() {
  FreeLibrary(hModule);
}


HMODULE PluginBase::DLL::GetHandle() const noexcept {
  return hModule;
}


PluginBase::DLL::operator HMODULE() const noexcept {
  return hModule;
}



PluginBase::PluginInitError::PluginInitError(PLUGIN_INITCODE errorCode) :
  std::runtime_error(u8"PluginInitError: "s + std::to_string(static_cast<int>(errorCode))),
  errorCode(errorCode),
  w32Code(errorCode == PLUGIN_INITCODE::W32 ? GetLastError() : ERROR_SUCCESS),
  status(w32Code != ERROR_SUCCESS ? DokanNtStatusFromWin32(w32Code) : STATUS_SUCCESS)
{}


PluginBase::PluginInitError::operator PluginBase::PLUGIN_INITCODE() const noexcept {
  return errorCode;
}



PluginBase::PluginBase(std::wstring_view pluginFilePath, PLUGIN_TYPE pluginType) :
  pluginFilePath(pluginFilePath),
  pluginType(pluginType),
  initializeInfo({
    MERGEFS_VERSION,
    DokanVersion(),
    DOKAN_MINIMUM_COMPATIBLE_VERSION,
    DokanDriverVersion(),
    {
      DokanFuncs::DokanMapKernelToUserCreateFileFlags,
      DokanFuncs::DokanNtStatusFromWin32,
      DokanFuncs::DokanIsNameInExpression,
      DokanFuncs::DokanResetTimeout,
      DokanFuncs::DokanOpenRequestorToken,
      DokanFuncs::DokanGetMountPointList,
    },
  }),
  dll(this->pluginFilePath.c_str()),
  SGetPluginInfo(dll.GetProc<PSGetPluginInfo>(u8"SGetPluginInfo")),
  SInitialize(dll.GetProc<PSInitialize>(u8"SInitialize")),
  pluginInfo(*SGetPluginInfo())
{
  if (gPluginGuidSet.count(pluginInfo.guid)) {
    throw PluginInitError(PLUGIN_INITCODE::AlreadyExists);
  }
  if (pluginInfo.pluginType != pluginType) {
    throw PluginInitError(PLUGIN_INITCODE::WrongType);
  }
  try {
    try {
      gPluginGuidSet.emplace(pluginInfo.guid);
    } catch (...) {
      throw PluginInitError(PLUGIN_INITCODE::AllocationFailed);
    }
    if (const auto ret = SInitialize(&initializeInfo); ret != PLUGIN_INITCODE::Success) {
      throw PluginInitError(ret);
    }
  } catch (...) {
    gPluginGuidSet.erase(pluginInfo.guid);
    throw;
  }
}


PluginBase::~PluginBase() {
  gPluginGuidSet.erase(pluginInfo.guid);
}
