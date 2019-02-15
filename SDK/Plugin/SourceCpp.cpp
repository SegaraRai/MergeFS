// for source plugin developers:
// - create SouceCpp.cpp
//   - in SourceCpp.cpp, include dokan.h, then include this file
// - or set include path

#pragma once


#ifndef DOKAN_VERSION
# if __has_include(<dokan/dokan.h>)
#  include <dokan/dokan.h>
# else
#  error include dokan.h
# endif
#endif

#include "SourceCpp.hpp"
#include "../CaseSensitivity.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include <Windows.h>

using namespace std::literals;



namespace {
  template<typename T>
  NTSTATUS WrapException(const T& func) noexcept {
    static_assert(std::is_same_v<decltype(func()), NTSTATUS>);

    try {
      return func();
    } catch (std::invalid_argument&) {
      return STATUS_INVALID_PARAMETER;
    } catch (std::domain_error&) {
      return STATUS_INVALID_PARAMETER;
    } catch (std::length_error&) {
      return STATUS_INVALID_PARAMETER;
    } catch (std::out_of_range&) {
      return STATUS_ACCESS_VIOLATION;
    } catch (std::bad_optional_access&) {
      return STATUS_ACCESS_VIOLATION;
    } catch (std::range_error&) {
      return STATUS_INVALID_PARAMETER;
    } catch (std::ios_base::failure&) {
      return STATUS_IO_DEVICE_ERROR;
    } catch (std::bad_typeid&) {
      return STATUS_ACCESS_VIOLATION;
    } catch (std::bad_cast&) {
      return STATUS_ACCESS_VIOLATION;
    } catch (std::bad_weak_ptr&) {
      return STATUS_ACCESS_VIOLATION;
    } catch (std::bad_function_call&) {
      return STATUS_ACCESS_VIOLATION;
    } catch (std::bad_alloc&) {
      return STATUS_NO_MEMORY;
    } catch (NtstatusError& ntstatusError) {
      return ntstatusError;
    } catch (...) {
      return STATUS_UNSUCCESSFUL;
    }
  }


  template<typename T>
  NTSTATUS WrapExceptionV(const T& func) noexcept {
    static_assert(std::is_same_v<decltype(func()), void>);

    return WrapException([&func]() -> NTSTATUS {
      func();
      return STATUS_SUCCESS;
    });
  }



  PLUGIN_INITIALIZE_INFO gPluginInitializeInfo{};
  std::unordered_map<SOURCE_CONTEXT_ID, std::unique_ptr<SourceMountBase>> gSourceMountBaseMap;
  std::mutex gMutex;



  SourceMountBase& GetSourceMountBase(SOURCE_CONTEXT_ID sourceContextId) {
    std::lock_guard lock(gMutex);
    return *gSourceMountBaseMap.at(sourceContextId);
  }


  std::string UInt32ToHexString(std::uint_fast32_t value) {
    constexpr auto Hex = u8"0123456789ABCDEF";

    const char hexString[] = {
      Hex[(value >> 28) & 0x0F],
      Hex[(value >> 24) & 0x0F],
      Hex[(value >> 20) & 0x0F],
      Hex[(value >> 16) & 0x0F],
      Hex[(value >> 12) & 0x0F],
      Hex[(value >>  8) & 0x0F],
      Hex[(value >>  4) & 0x0F],
      Hex[(value >>  0) & 0x0F],
    };
    return std::string(hexString, 8);
  }
}



// NtstatusError
////////////////////////////////////////////////////////////////////////////////////////////////////

NtstatusError::NtstatusError(NTSTATUS ntstatusErrorCode) :
  // NTSTATUS is usually represented in hexadecimal
  std::runtime_error(u8"NT Error 0x"s + UInt32ToHexString(ntstatusErrorCode)),
  ntstatusErrorCode(ntstatusErrorCode)
{}


NtstatusError::NtstatusError(NTSTATUS ntstatusErrorCode, const std::string& errorMessage) :
  std::runtime_error(errorMessage),
  ntstatusErrorCode(ntstatusErrorCode)
{}


NtstatusError::NtstatusError(NTSTATUS ntstatusErrorCode, const char* errorMessage) :
  std::runtime_error(errorMessage),
  ntstatusErrorCode(ntstatusErrorCode)
{}


NtstatusError::operator NTSTATUS() const noexcept {
  return ntstatusErrorCode;
}



// Win32Error
////////////////////////////////////////////////////////////////////////////////////////////////////

NTSTATUS Win32Error::NTSTATUSFromWin32(DWORD win32ErrorCode) noexcept {
  if (gPluginInitializeInfo.dokanFuncs.DokanNtStatusFromWin32) {
    return gPluginInitializeInfo.dokanFuncs.DokanNtStatusFromWin32(win32ErrorCode);
  }
  // NTSTATUS_FROM_WIN32 can convert an error code from -32k to 32k; see NTSTATUS_FROM_WIN32
  if ((win32ErrorCode & 0xFFFF0000) == 0) {
    return __NTSTATUS_FROM_WIN32(win32ErrorCode);
  }
  return STATUS_UNSUCCESSFUL;
}

Win32Error::Win32Error(DWORD win32ErrorCode) :
  // Win32 error codes are usually represented in decimal
  NtstatusError(NTSTATUSFromWin32(win32ErrorCode), u8"Win32 Error "s + std::to_string(win32ErrorCode)),
  win32ErrorCode(win32ErrorCode)
{}


Win32Error::Win32Error(DWORD win32ErrorCode, const char* errorMessage) :
  NtstatusError(NTSTATUSFromWin32(win32ErrorCode), errorMessage),
  win32ErrorCode(win32ErrorCode)
{}


Win32Error::Win32Error(DWORD win32ErrorCode, const std::string& errorMessage) :
  NtstatusError(NTSTATUSFromWin32(win32ErrorCode), errorMessage),
  win32ErrorCode(win32ErrorCode)
{}


Win32Error::Win32Error() :
  Win32Error(GetLastError())
{}


Win32Error::Win32Error(const char* errorMessage) :
  Win32Error(GetLastError(), errorMessage)
{}


Win32Error::Win32Error(const std::string& errorMessage) :
  Win32Error(GetLastError(), errorMessage)
{}



// SourceMountFileBase
////////////////////////////////////////////////////////////////////////////////////////////////////

SourceMountFileBase::SourceMountFileBase(SourceMountBase& sourceMountBase, LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId) :
  mutex(),
  sourceMountBase(sourceMountBase),
  fileContextId(FileContextId),
  filename(FileName),
  maybeSwitched(MaybeSwitched),
  argSecurityContext(*SecurityContext),
  argDesiredAccess(DesiredAccess),
  argFileAttributes(FileAttributes),
  argShareAccess(ShareAccess),
  argCreateDisposition(CreateDisposition),
  argCreateOptions(CreateOptions)
{}


SourceMountFileBase::SourceMountFileBase(SourceMountBase& sourceMountBase, LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) :
  mutex(),
  sourceMountBase(sourceMountBase),
  fileContextId(FileContextId),
  filename(FileName),
  maybeSwitched(std::nullopt),
  argSecurityContext(*SecurityContext),
  argDesiredAccess(DesiredAccess),
  argFileAttributes(FileAttributes),
  argShareAccess(ShareAccess),
  argCreateDisposition(CreateDisposition),
  argCreateOptions(CreateOptions)
{}


NTSTATUS SourceMountFileBase::ExportStart(PORTATION_INFO* PortationInfo) {
  return sourceMountBase.ExportStartImpl(PortationInfo);
}


NTSTATUS SourceMountFileBase::ExportData(PORTATION_INFO* PortationInfo) {
  return sourceMountBase.ExportDataImpl(PortationInfo);
}


NTSTATUS SourceMountFileBase::ExportFinish(PORTATION_INFO* PortationInfo, BOOL Success) {
  return sourceMountBase.ExportFinishImpl(PortationInfo, Success);
}


NTSTATUS SourceMountFileBase::ImportStart(PORTATION_INFO* PortationInfo) {
  return sourceMountBase.ImportStartImpl(PortationInfo);
}


NTSTATUS SourceMountFileBase::ImportData(PORTATION_INFO* PortationInfo) {
  return sourceMountBase.ImportDataImpl(PortationInfo);
}


NTSTATUS SourceMountFileBase::ImportFinish(PORTATION_INFO* PortationInfo, BOOL Success) {
  return sourceMountBase.ImportFinishImpl(PortationInfo, Success);
}


NTSTATUS SourceMountFileBase::SwitchSourceCloseImpl(PDOKAN_FILE_INFO DokanFileInfo) {
  DCloseFileImpl(DokanFileInfo);
  return STATUS_SUCCESS;
}


bool SourceMountFileBase::IsCleanuped() const {
  return privateCleanuped;
}


bool SourceMountFileBase::IsClosed() const {
  return privateClosed;
}


NTSTATUS SourceMountFileBase::SwitchSourceClose(PDOKAN_FILE_INFO DokanFileInfo) {
  try {
    auto ret = SwitchSourceCloseImpl(DokanFileInfo);
    privateClosed = true;
    return ret;
  } catch (...) {
    privateClosed = true;
    throw;
  }
}


NTSTATUS SourceMountFileBase::SwitchDestinationCleanup(PDOKAN_FILE_INFO DokanFileInfo) {
  try {
    auto ret = SwitchDestinationCleanupImpl(DokanFileInfo);
    privateCleanuped = true;
    return ret;
  } catch (...) {
    privateCleanuped = true;
    throw;
  }
}


NTSTATUS SourceMountFileBase::SwitchDestinationClose(PDOKAN_FILE_INFO DokanFileInfo) {
  try {
    auto ret = SwitchDestinationCloseImpl(DokanFileInfo);
    privateClosed = true;
    return ret;
  } catch (...) {
    privateClosed = true;
    throw;
  }
}


void SourceMountFileBase::DCleanup(PDOKAN_FILE_INFO DokanFileInfo) {
  try {
    DCleanupImpl(DokanFileInfo);
    privateCleanuped = true;
  } catch (...) {
    privateCleanuped = true;
    throw;
  }
}


void SourceMountFileBase::DCloseFile(PDOKAN_FILE_INFO DokanFileInfo) {
  try {
    DCloseFileImpl(DokanFileInfo);
    privateClosed = true;
  } catch (...) {
    privateClosed = true;
    throw;
  }
}



// SourceMountBase
////////////////////////////////////////////////////////////////////////////////////////////////////

SourceMountBase::SourceMountBase(const PLUGIN_INITIALIZE_MOUNT_INFO* InitializeMountInfo, SOURCE_CONTEXT_ID sourceContextId) :
  privateMutex(),
  privateFileMap(),
  sourceContextId(sourceContextId),
  filename(InitializeMountInfo->FileName),
  caseSensitive(InitializeMountInfo->CaseSensitive),
  ciEqualTo(caseSensitive),
  ciHash(caseSensitive)
{}


bool SourceMountBase::SourceMountFileBaseExistsL(FILE_CONTEXT_ID fileContextId) const {
  return privateFileMap.count(fileContextId);
}


bool SourceMountBase::SourceMountFileBaseExists(FILE_CONTEXT_ID fileContextId) {
  std::lock_guard lock(privateMutex);
  return SourceMountFileBaseExistsL(fileContextId);
}


std::shared_ptr<SourceMountFileBase> SourceMountBase::GetSourceMountFileBaseL(FILE_CONTEXT_ID fileContextId) const {
  auto itrChild = privateFileMap.find(fileContextId);
  if (itrChild == privateFileMap.end()) {
    return nullptr;
  }
  return itrChild->second;
}


std::shared_ptr<SourceMountFileBase> SourceMountBase::GetSourceMountFileBase(FILE_CONTEXT_ID fileContextId) {
  std::lock_guard lock(privateMutex);
  return GetSourceMountFileBaseL(fileContextId);
}


NTSTATUS SourceMountBase::ExportStart(PORTATION_INFO* PortationInfo) {
  if (!PortationInfo) {
    return STATUS_INVALID_PARAMETER;
  }
  const FILE_CONTEXT_ID fileContextId = PortationInfo->fileContextId;
  return fileContextId != FILE_CONTEXT_ID_NULL ? GetSourceMountFileBase(fileContextId)->ExportStart(PortationInfo) : ExportStartImpl(PortationInfo);
}


NTSTATUS SourceMountBase::ExportData(PORTATION_INFO* PortationInfo) {
  if (!PortationInfo) {
    return STATUS_INVALID_PARAMETER;
  }
  const FILE_CONTEXT_ID fileContextId = PortationInfo->fileContextId;
  return fileContextId != FILE_CONTEXT_ID_NULL ? GetSourceMountFileBase(fileContextId)->ExportData(PortationInfo) : ExportDataImpl(PortationInfo);
}


NTSTATUS SourceMountBase::ExportFinish(PORTATION_INFO* PortationInfo, BOOL Success) {
  if (!PortationInfo) {
    return STATUS_INVALID_PARAMETER;
  }
  const FILE_CONTEXT_ID fileContextId = PortationInfo->fileContextId;
  return fileContextId != FILE_CONTEXT_ID_NULL ? GetSourceMountFileBase(fileContextId)->ExportFinish(PortationInfo, Success) : ExportFinishImpl(PortationInfo, Success);
}


NTSTATUS SourceMountBase::ImportStart(PORTATION_INFO* PortationInfo) {
  if (!PortationInfo) {
    return STATUS_INVALID_PARAMETER;
  }
  const FILE_CONTEXT_ID fileContextId = PortationInfo->fileContextId;
  return fileContextId != FILE_CONTEXT_ID_NULL ? GetSourceMountFileBase(fileContextId)->ImportStart(PortationInfo) : ImportStartImpl(PortationInfo);
}


NTSTATUS SourceMountBase::ImportData(PORTATION_INFO* PortationInfo) {
  if (!PortationInfo) {
    return STATUS_INVALID_PARAMETER;
  }
  const FILE_CONTEXT_ID fileContextId = PortationInfo->fileContextId;
  return fileContextId != FILE_CONTEXT_ID_NULL ? GetSourceMountFileBase(fileContextId)->ImportData(PortationInfo) : ImportDataImpl(PortationInfo);
}


NTSTATUS SourceMountBase::ImportFinish(PORTATION_INFO* PortationInfo, BOOL Success) {
  if (!PortationInfo) {
    return STATUS_INVALID_PARAMETER;
  }
  const FILE_CONTEXT_ID fileContextId = PortationInfo->fileContextId;
  return fileContextId != FILE_CONTEXT_ID_NULL ? GetSourceMountFileBase(fileContextId)->ImportFinish(PortationInfo, Success) : ImportFinishImpl(PortationInfo, Success);
}


NTSTATUS SourceMountBase::SwitchSourceClose(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  // erase entry as early as possible to ensure erasion
  std::lock_guard lock(privateMutex);
  auto upSourceMountFile = std::move(privateFileMap.at(FileContextId));
  privateFileMap.erase(FileContextId);
  return upSourceMountFile->SwitchSourceClose(DokanFileInfo);
}


NTSTATUS SourceMountBase::SwitchDestinationPrepare(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  return SwitchDestinationPrepareImpl(FileName, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, FileContextId);
}


NTSTATUS SourceMountBase::SwitchDestinationOpen(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  std::lock_guard lock(privateMutex);
  privateFileMap.emplace(FileContextId, SwitchDestinationOpenImpl(FileName, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, FileContextId));
  return STATUS_SUCCESS;
}


NTSTATUS SourceMountBase::SwitchDestinationCleanup(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  std::lock_guard lock(privateMutex);
  if (!SourceMountFileBaseExistsL(FileContextId)) {
    return SwitchDestinationCleanupImpl(FileName, DokanFileInfo, FileContextId);
  }
  return GetSourceMountFileBase(FileContextId)->SwitchDestinationCleanup(DokanFileInfo);
}


NTSTATUS SourceMountBase::SwitchDestinationClose(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  // erase entry as early as possible to ensure erasion
  std::lock_guard lock(privateMutex);
  if (!SourceMountFileBaseExistsL(FileContextId)) {
    return SwitchDestinationCloseImpl(FileName, DokanFileInfo, FileContextId);
  }
  auto spSourceMountFile = privateFileMap.at(FileContextId);
  privateFileMap.erase(FileContextId);
  return spSourceMountFile->SwitchDestinationClose(DokanFileInfo);;
}


NTSTATUS SourceMountBase::DZwCreateFile(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId) {
  std::lock_guard lock(privateMutex);
  privateFileMap.emplace(FileContextId, DZwCreateFileImpl(FileName, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, MaybeSwitched, FileContextId));
  return STATUS_SUCCESS;
}


void SourceMountBase::DCleanup(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  GetSourceMountFileBase(FileContextId)->DCleanup(DokanFileInfo);
}


void SourceMountBase::DCloseFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  // erase entry as early as possible to ensure erasion
  std::lock_guard lock(privateMutex);
  auto spSourceMountFile = privateFileMap.at(FileContextId);
  privateFileMap.erase(FileContextId);
  spSourceMountFile->DCloseFile(DokanFileInfo);
}


NTSTATUS SourceMountBase::DReadFile(LPCWSTR FileName, LPVOID Buffer, DWORD BufferLength, LPDWORD ReadLength, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  return GetSourceMountFileBase(FileContextId)->DReadFile(Buffer, BufferLength, ReadLength, Offset, DokanFileInfo);
}


NTSTATUS SourceMountBase::DWriteFile(LPCWSTR FileName, LPCVOID Buffer, DWORD NumberOfBytesToWrite, LPDWORD NumberOfBytesWritten, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  return GetSourceMountFileBase(FileContextId)->DWriteFile(Buffer, NumberOfBytesToWrite, NumberOfBytesWritten, Offset, DokanFileInfo);
}


NTSTATUS SourceMountBase::DFlushFileBuffers(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  return GetSourceMountFileBase(FileContextId)->DFlushFileBuffers(DokanFileInfo);
}


NTSTATUS SourceMountBase::DGetFileInformation(LPCWSTR FileName, LPBY_HANDLE_FILE_INFORMATION Buffer, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  return GetSourceMountFileBase(FileContextId)->DGetFileInformation(Buffer, DokanFileInfo);
}


NTSTATUS SourceMountBase::DSetFileAttributes(LPCWSTR FileName, DWORD FileAttributes, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  return GetSourceMountFileBase(FileContextId)->DSetFileAttributes(FileAttributes, DokanFileInfo);
}


NTSTATUS SourceMountBase::DSetFileTime(LPCWSTR FileName, const FILETIME* CreationTime, const FILETIME* LastAccessTime, const FILETIME* LastWriteTime, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  return GetSourceMountFileBase(FileContextId)->DSetFileTime(CreationTime, LastAccessTime, LastWriteTime, DokanFileInfo);
}


NTSTATUS SourceMountBase::DDeleteFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  return GetSourceMountFileBase(FileContextId)->DDeleteFile(DokanFileInfo);
}


NTSTATUS SourceMountBase::DDeleteDirectory(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  return GetSourceMountFileBase(FileContextId)->DDeleteDirectory(DokanFileInfo);
}


NTSTATUS SourceMountBase::DMoveFile(LPCWSTR FileName, LPCWSTR NewFileName, BOOL ReplaceIfExisting, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  return GetSourceMountFileBase(FileContextId)->DMoveFile(NewFileName, ReplaceIfExisting, DokanFileInfo);
}


NTSTATUS SourceMountBase::DSetEndOfFile(LPCWSTR FileName, LONGLONG ByteOffset, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  return GetSourceMountFileBase(FileContextId)->DSetEndOfFile(ByteOffset, DokanFileInfo);
}


NTSTATUS SourceMountBase::DSetAllocationSize(LPCWSTR FileName, LONGLONG AllocSize, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  return GetSourceMountFileBase(FileContextId)->DSetAllocationSize(AllocSize, DokanFileInfo);
}


NTSTATUS SourceMountBase::DGetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PULONG LengthNeeded, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  return GetSourceMountFileBase(FileContextId)->DGetFileSecurity(SecurityInformation, SecurityDescriptor, BufferLength, LengthNeeded, DokanFileInfo);
}


NTSTATUS SourceMountBase::DSetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  return GetSourceMountFileBase(FileContextId)->DSetFileSecurity(SecurityInformation, SecurityDescriptor, BufferLength, DokanFileInfo);
}



// Exports
////////////////////////////////////////////////////////////////////////////////////////////////////

const PLUGIN_INFO* WINAPI SGetPluginInfo() MFNOEXCEPT {
  return SGetPluginInfoImpl();
}


PLUGIN_INITCODE WINAPI SInitialize(const PLUGIN_INITIALIZE_INFO* InitializeInfo) MFNOEXCEPT {
  return SInitializeImpl(InitializeInfo);
}


BOOL WINAPI SIsSupported(const PLUGIN_INITIALIZE_MOUNT_INFO* InitializeMountInfo) MFNOEXCEPT {
  return SIsSupportedImpl(InitializeMountInfo);
}


NTSTATUS WINAPI Mount(const PLUGIN_INITIALIZE_MOUNT_INFO* InitializeMountInfo, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    std::unique_lock lock(gMutex);
    if (gSourceMountBaseMap.count(sourceContextId)) {
      return STATUS_INVALID_HANDLE;
    }
    lock.unlock();
    auto sourceMount = MountImpl(InitializeMountInfo, sourceContextId);
    lock.lock();
    gSourceMountBaseMap.emplace(sourceContextId, std::move(sourceMount));
    return STATUS_SUCCESS;
  });
}


BOOL WINAPI Unmount(SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  try {
    std::lock_guard lock(gMutex);
    if (!gSourceMountBaseMap.count(sourceContextId)) {
      return FALSE;
    }
    gSourceMountBaseMap.erase(sourceContextId);
    return TRUE;
  } catch (...) {
    return FALSE;
  }
}


BOOL WINAPI GetSourceInfo(SOURCE_INFO* sourceInfo, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  try {
    return GetSourceMountBase(sourceContextId).GetSourceInfo(sourceInfo);
  } catch (...) {
    return FALSE;
  }
}


NTSTATUS WINAPI GetFileInfo(LPCWSTR FileName, WIN32_FILE_ATTRIBUTE_DATA* Win32FileAttributeData, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).GetFileInfo(FileName, Win32FileAttributeData);
  });
}


NTSTATUS WINAPI GetDirectoryInfo(LPCWSTR FileName, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).GetDirectoryInfo(FileName);
  });
}


NTSTATUS WINAPI RemoveFile(LPCWSTR FileName, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).RemoveFile(FileName);
  });
}


NTSTATUS WINAPI ExportStart(PORTATION_INFO* PortationInfo, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).ExportStart(PortationInfo);
  });
}


NTSTATUS WINAPI ExportData(PORTATION_INFO* PortationInfo, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).ExportData(PortationInfo);
  });
}


NTSTATUS WINAPI ExportFinish(PORTATION_INFO* PortationInfo, BOOL Success, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).ExportFinish(PortationInfo, Success);
  });
}


NTSTATUS WINAPI ImportStart(PORTATION_INFO* PortationInfo, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).ImportStart(PortationInfo);
  });
}


NTSTATUS WINAPI ImportData(PORTATION_INFO* PortationInfo, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).ImportData(PortationInfo);
  });
}


NTSTATUS WINAPI ImportFinish(PORTATION_INFO* PortationInfo, BOOL Success, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).ImportFinish(PortationInfo, Success);
  });
}


NTSTATUS WINAPI SwitchSourceClose(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).SwitchSourceClose(FileName, DokanFileInfo, FileContextId);
  });
}


NTSTATUS WINAPI SwitchDestinationPrepare(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).SwitchDestinationPrepare(FileName, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, FileContextId);
  });
}


NTSTATUS WINAPI SwitchDestinationOpen(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).SwitchDestinationOpen(FileName, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, FileContextId);
  });
}


NTSTATUS WINAPI SwitchDestinationCleanup(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).SwitchDestinationCleanup(FileName, DokanFileInfo, FileContextId);
  });
}


NTSTATUS WINAPI SwitchDestinationClose(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).SwitchDestinationClose(FileName, DokanFileInfo, FileContextId);
  });
}


NTSTATUS WINAPI ListFiles(LPCWSTR FileName, PListFilesCallback Callback, CALLBACK_CONTEXT CallbackContext, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).ListFiles(FileName, Callback, CallbackContext);
  });
}


NTSTATUS WINAPI ListStreams(LPCWSTR FileName, PListStreamsCallback Callback, CALLBACK_CONTEXT CallbackContext, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).ListStreams(FileName, Callback, CallbackContext);
  });
}


NTSTATUS WINAPI DZwCreateFile(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).DZwCreateFile(FileName, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, MaybeSwitched, FileContextId);
  });
}


void WINAPI DCleanup(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  try {
    GetSourceMountBase(sourceContextId).DCleanup(FileName, DokanFileInfo, FileContextId);
  } catch (...) {}
}


void WINAPI DCloseFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  try {
    GetSourceMountBase(sourceContextId).DCloseFile(FileName, DokanFileInfo, FileContextId);
  } catch (...) {}
}


NTSTATUS WINAPI DReadFile(LPCWSTR FileName, LPVOID Buffer, DWORD BufferLength, LPDWORD ReadLength, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  // MUST BE THEAD SAFE
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).DReadFile(FileName, Buffer, BufferLength, ReadLength, Offset, DokanFileInfo, FileContextId);
  });
}


NTSTATUS WINAPI DWriteFile(LPCWSTR FileName, LPCVOID Buffer, DWORD NumberOfBytesToWrite, LPDWORD NumberOfBytesWritten, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  // MUST BE THEAD SAFE
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).DWriteFile(FileName, Buffer, NumberOfBytesToWrite, NumberOfBytesWritten, Offset, DokanFileInfo, FileContextId);
  });
}


NTSTATUS WINAPI DFlushFileBuffers(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).DFlushFileBuffers(FileName, DokanFileInfo, FileContextId);
  });
}


NTSTATUS WINAPI DGetFileInformation(LPCWSTR FileName, LPBY_HANDLE_FILE_INFORMATION Buffer, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).DGetFileInformation(FileName, Buffer, DokanFileInfo, FileContextId);
  });
}


NTSTATUS WINAPI DSetFileAttributes(LPCWSTR FileName, DWORD FileAttributes, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).DSetFileAttributes(FileName, FileAttributes, DokanFileInfo, FileContextId);
  });
}


NTSTATUS WINAPI DSetFileTime(LPCWSTR FileName, const FILETIME* CreationTime, const FILETIME* LastAccessTime, const FILETIME* LastWriteTime, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).DSetFileTime(FileName, CreationTime, LastAccessTime, LastWriteTime, DokanFileInfo, FileContextId);
  });
}


NTSTATUS WINAPI DDeleteFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).DDeleteFile(FileName, DokanFileInfo, FileContextId);
  });
}


NTSTATUS WINAPI DDeleteDirectory(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).DDeleteDirectory(FileName, DokanFileInfo, FileContextId);
  });
}


NTSTATUS WINAPI DMoveFile(LPCWSTR FileName, LPCWSTR NewFileName, BOOL ReplaceIfExisting, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).DMoveFile(FileName, NewFileName, ReplaceIfExisting, DokanFileInfo, FileContextId);
  });
}


NTSTATUS WINAPI DSetEndOfFile(LPCWSTR FileName, LONGLONG ByteOffset, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).DSetEndOfFile(FileName, ByteOffset, DokanFileInfo, FileContextId);
  });
}


NTSTATUS WINAPI DSetAllocationSize(LPCWSTR FileName, LONGLONG AllocSize, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).DSetAllocationSize(FileName, AllocSize, DokanFileInfo, FileContextId);
  });
}


NTSTATUS WINAPI DGetDiskFreeSpace(PULONGLONG FreeBytesAvailable, PULONGLONG TotalNumberOfBytes, PULONGLONG TotalNumberOfFreeBytes, PDOKAN_FILE_INFO DokanFileInfo, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).DGetDiskFreeSpace(FreeBytesAvailable, TotalNumberOfBytes, TotalNumberOfFreeBytes, DokanFileInfo);
  });
}


NTSTATUS WINAPI DGetVolumeInformation(LPWSTR VolumeNameBuffer, DWORD VolumeNameSize, LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength, LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer, DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).DGetVolumeInformation(VolumeNameBuffer, VolumeNameSize, VolumeSerialNumber, MaximumComponentLength, FileSystemFlags, FileSystemNameBuffer, FileSystemNameSize, DokanFileInfo);
  });
}


NTSTATUS WINAPI DGetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PULONG LengthNeeded, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).DGetFileSecurity(FileName, SecurityInformation, SecurityDescriptor, BufferLength, LengthNeeded, DokanFileInfo, FileContextId);
  });
}


NTSTATUS WINAPI DSetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return WrapException([=]() -> NTSTATUS {
    return GetSourceMountBase(sourceContextId).DSetFileSecurity(FileName, SecurityInformation, SecurityDescriptor, BufferLength, DokanFileInfo, FileContextId);
  });
}
