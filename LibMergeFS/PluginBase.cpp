#include "PluginBase.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace std::literals;



std::shared_ptr<std::unordered_set<GUID, GUIDHash>> PluginBase::gPtrPluginGuidSet = std::make_shared<std::unordered_set<GUID, GUIDHash>>();



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


    PDOKAN_CONTROL WINAPI DokanGetMountPointList(BOOL uncOnly, PULONG nbRead) noexcept {
      return ::DokanGetMountPointList(uncOnly, nbRead);
    }


    void WINAPI DokanReleaseMountPointList(PDOKAN_CONTROL list) noexcept {
      return ::DokanReleaseMountPointList(list);
    }
  }
}



PluginBase::DLL::DLL(LPCWSTR FileName) : hModule(LoadLibraryW(FileName)) {
  if (!hModule) {
    throw std::runtime_error("failed to load library");
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
  std::runtime_error("PluginInitError: "s + std::to_string(static_cast<int>(errorCode))),
  errorCode(errorCode),
  w32Code(errorCode == PLUGIN_INITCODE::W32 ? GetLastError() : ERROR_SUCCESS),
  status(w32Code != ERROR_SUCCESS ? DokanNtStatusFromWin32(w32Code) : STATUS_SUCCESS)
{}


PluginBase::PluginInitError::operator PluginBase::PLUGIN_INITCODE() const noexcept {
  return errorCode;
}



PluginBase::PluginBase(std::wstring_view pluginFilePath, PLUGIN_TYPE pluginType) :
  ptrPluginGuidSet(gPtrPluginGuidSet),
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
      DokanFuncs::DokanReleaseMountPointList,
    },
  }),
  dll(this->pluginFilePath.c_str()),
  SGetPluginInfo(dll.GetProc<PSGetPluginInfo>("SGetPluginInfo")),
  SInitialize(dll.GetProc<PSInitialize>("SInitialize")),
  pluginInfo(*SGetPluginInfo())
{
  if (gPtrPluginGuidSet->count(pluginInfo.guid)) {
    throw PluginInitError(PLUGIN_INITCODE::AlreadyExists);
  }
  if (pluginInfo.pluginType != pluginType) {
    throw PluginInitError(PLUGIN_INITCODE::WrongType);
  }
  try {
    try {
      gPtrPluginGuidSet->emplace(pluginInfo.guid);
    } catch (...) {
      throw PluginInitError(PLUGIN_INITCODE::AllocationFailed);
    }
    if (const auto ret = SInitialize(&initializeInfo); ret != PLUGIN_INITCODE::Success) {
      throw PluginInitError(ret);
    }
  } catch (...) {
    gPtrPluginGuidSet->erase(pluginInfo.guid);
    throw;
  }
}


PluginBase::~PluginBase() {
  gPtrPluginGuidSet->erase(pluginInfo.guid);
}
