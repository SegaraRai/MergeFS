// for source plugin developers:
// - include dokan.h, then include this file
// - implement a class which inherits SourceMountFileBase
// - implement a class which inherits SourceMountBase
//   - the body of DZwCreateFileImpl will be like `return std::make_unique<SourceMountFile>(FileName, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, MaybeSwitched, FileContextId);`
//   - the body of SwitchDestinationOpenImpl will be like `return std::make_unique<SourceMountFile>(FileName, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, FileContextId);`
// - implement `DllMain` function
// - implement `const PLUGIN_INFO* SGetPluginInfo()` function
// - implement `PLUGIN_INITCODE SInitializeImpl()` function
// - implement `BOOL SIsSupportedImpl(const PLUGIN_INITIALIZE_MOUNT_INFO* InitializeMountInfo)` function
// - implement `std::unique_ptr<SourceMountBase> MountImpl(const PLUGIN_INITIALIZE_MOUNT_INFO* InitializeMountInfo, SOURCE_CONTEXT_ID sourceContextId)` function
//   - the body of MountImpl function will be like `return std::make_unique<SourceMount>(InitializeMountInfo, sourceContextId);`
// - functions which is not noexcept can throw NtstatusError exception and Win33Error exception

#pragma once

#include "Source.h"
#include "../CaseSensitivity.hpp"

#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <Windows.h>


class SourceMountBase;


const PLUGIN_INFO* SGetPluginInfoImpl() noexcept;
PLUGIN_INITCODE SInitializeImpl(const PLUGIN_INITIALIZE_INFO* InitializeInfo) noexcept;
BOOL SIsSupportedImpl(const PLUGIN_INITIALIZE_MOUNT_INFO* InitializeMountInfo) noexcept;
std::unique_ptr<SourceMountBase> MountImpl(const PLUGIN_INITIALIZE_MOUNT_INFO* InitializeMountInfo, SOURCE_CONTEXT_ID sourceContextId);


class NtstatusError : public std::runtime_error {
public:
  const NTSTATUS ntstatusErrorCode;

  NtstatusError(NTSTATUS ntstatusErrorCode);
  NtstatusError(NTSTATUS ntstatusErrorCode, const std::string& errorMessage);
  NtstatusError(NTSTATUS ntstatusErrorCode, const char* errorMessage);

  operator NTSTATUS() const noexcept;
};


class Win32Error : public NtstatusError {
public:
  static NTSTATUS NTSTATUSFromWin32(DWORD win32ErrorCode) noexcept;

  const DWORD win32ErrorCode;

  Win32Error(DWORD win32ErrorCode);
  Win32Error(DWORD win32ErrorCode, const std::string& errorMessage);
  Win32Error(DWORD win32ErrorCode, const char* errorMessage);

  // they will retrieve error code from GetLastError()
  Win32Error();
  Win32Error(const std::string& errorMessage);
  Win32Error(const char* errorMessage);
};


class SourceMountFileBase {
  bool privateCleanuped;
  bool privateClosed;

protected:
  std::mutex mutex;
  SourceMountBase& sourceMountBase;
  const FILE_CONTEXT_ID fileContextId;
  const std::wstring filename;
  const std::optional<BOOL> maybeSwitchedN;
  const DOKAN_IO_SECURITY_CONTEXT argSecurityContext;
  const ACCESS_MASK argDesiredAccess;
  const ULONG argFileAttributes;
  const ULONG argShareAccess;
  const ULONG argCreateDisposition;
  const ULONG argCreateOptions;

  SourceMountFileBase(SourceMountBase& sourceMountBase, LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, std::optional<BOOL> MaybeSwitchedN);
  SourceMountFileBase(SourceMountBase& sourceMountBase, LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId);
  SourceMountFileBase(SourceMountBase& sourceMountBase, LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId);

public:
  virtual ~SourceMountFileBase() = default;

  virtual NTSTATUS ExportStart(PORTATION_INFO* PortationInfo);
  virtual NTSTATUS ExportData(PORTATION_INFO* PortationInfo);
  virtual NTSTATUS ExportFinish(PORTATION_INFO* PortationInfo, BOOL Success);
  virtual NTSTATUS ImportStart(PORTATION_INFO* PortationInfo);
  virtual NTSTATUS ImportData(PORTATION_INFO* PortationInfo);
  virtual NTSTATUS ImportFinish(PORTATION_INFO* PortationInfo, BOOL Success);
  virtual NTSTATUS SwitchSourceCloseImpl(PDOKAN_FILE_INFO DokanFileInfo);
  virtual NTSTATUS SwitchDestinationCleanupImpl(PDOKAN_FILE_INFO DokanFileInfo);
  virtual NTSTATUS SwitchDestinationCloseImpl(PDOKAN_FILE_INFO DokanFileInfo);
  virtual void DCleanupImpl(PDOKAN_FILE_INFO DokanFileInfo);
  virtual void DCloseFileImpl(PDOKAN_FILE_INFO DokanFileInfo);

  virtual NTSTATUS DReadFile(LPVOID Buffer, DWORD BufferLength, LPDWORD ReadLength, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo) = 0;
  virtual NTSTATUS DWriteFile(LPCVOID Buffer, DWORD NumberOfBytesToWrite, LPDWORD NumberOfBytesWritten, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo) = 0;
  virtual NTSTATUS DFlushFileBuffers(PDOKAN_FILE_INFO DokanFileInfo) = 0;
  virtual NTSTATUS DGetFileInformation(LPBY_HANDLE_FILE_INFORMATION Buffer, PDOKAN_FILE_INFO DokanFileInfo) = 0;
  virtual NTSTATUS DSetFileAttributes(DWORD FileAttributes, PDOKAN_FILE_INFO DokanFileInfo) = 0;
  virtual NTSTATUS DSetFileTime(const FILETIME* CreationTime, const FILETIME* LastAccessTime, const FILETIME* LastWriteTime, PDOKAN_FILE_INFO DokanFileInfo) = 0;
  virtual NTSTATUS DDeleteFile(PDOKAN_FILE_INFO DokanFileInfo) = 0;
  virtual NTSTATUS DDeleteDirectory(PDOKAN_FILE_INFO DokanFileInfo) = 0;
  virtual NTSTATUS DMoveFile(LPCWSTR NewFileName, BOOL ReplaceIfExisting, PDOKAN_FILE_INFO DokanFileInfo) = 0;
  virtual NTSTATUS DSetEndOfFile(LONGLONG ByteOffset, PDOKAN_FILE_INFO DokanFileInfo) = 0;
  virtual NTSTATUS DSetAllocationSize(LONGLONG AllocSize, PDOKAN_FILE_INFO DokanFileInfo) = 0;
  virtual NTSTATUS DGetFileSecurity(PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PULONG LengthNeeded, PDOKAN_FILE_INFO DokanFileInfo) = 0;
  virtual NTSTATUS DSetFileSecurity(PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PDOKAN_FILE_INFO DokanFileInfo) = 0;

  bool IsCleanuped() const;
  bool IsClosed() const;
  NTSTATUS SwitchSourceClose(PDOKAN_FILE_INFO DokanFileInfo);
  NTSTATUS SwitchDestinationCleanup(PDOKAN_FILE_INFO DokanFileInfo);
  NTSTATUS SwitchDestinationClose(PDOKAN_FILE_INFO DokanFileInfo);
  void DCleanup(PDOKAN_FILE_INFO DokanFileInfo);
  void DCloseFile(PDOKAN_FILE_INFO DokanFileInfo);
};


class SourceMountBase {
  std::mutex privateMutex;
  std::unordered_map<FILE_CONTEXT_ID, std::shared_ptr<SourceMountFileBase>> privateFileMap;

  bool SourceMountFileBaseExistsL(FILE_CONTEXT_ID fileContextId) const;
  std::shared_ptr<SourceMountFileBase> GetSourceMountFileBaseL(FILE_CONTEXT_ID fileContextId) const;

protected:
  const SOURCE_CONTEXT_ID sourceContextId;
  const std::wstring filename;
  const bool caseSensitive;
  const CaseSensitivity::CiEqualTo ciEqualTo;
  const CaseSensitivity::CiHash ciHash;

  SourceMountBase(const PLUGIN_INITIALIZE_MOUNT_INFO* InitializeMountInfo, SOURCE_CONTEXT_ID sourceContextId);

  bool SourceMountFileBaseExists(FILE_CONTEXT_ID fileContextId);
  std::shared_ptr<SourceMountFileBase> GetSourceMountFileBase(FILE_CONTEXT_ID fileContextId);

public:
  virtual ~SourceMountBase() = default;

  virtual BOOL GetSourceInfo(SOURCE_INFO* sourceInfo) = 0;
  virtual NTSTATUS GetFileInfo(LPCWSTR FileName, WIN32_FILE_ATTRIBUTE_DATA* Win32FileAttributeData) = 0;
  virtual NTSTATUS GetDirectoryInfo(LPCWSTR FileName) = 0;
  virtual NTSTATUS RemoveFile(LPCWSTR FileName) = 0;
  virtual NTSTATUS ListFiles(LPCWSTR FileName, PListFilesCallback Callback, CALLBACK_CONTEXT CallbackContext) = 0;
  virtual NTSTATUS ListStreams(LPCWSTR FileName, PListStreamsCallback Callback, CALLBACK_CONTEXT CallbackContext) = 0;
  virtual NTSTATUS SwitchDestinationPrepareImpl(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) = 0;
  virtual NTSTATUS SwitchDestinationCleanupImpl(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) = 0;
  virtual NTSTATUS SwitchDestinationCloseImpl(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) = 0;
  virtual NTSTATUS DGetDiskFreeSpace(PULONGLONG FreeBytesAvailable, PULONGLONG TotalNumberOfBytes, PULONGLONG TotalNumberOfFreeBytes, PDOKAN_FILE_INFO DokanFileInfo) = 0;
  virtual NTSTATUS DGetVolumeInformation(LPWSTR VolumeNameBuffer, DWORD VolumeNameSize, LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength, LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer, DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo) = 0;
  virtual NTSTATUS ExportStartImpl(PORTATION_INFO* PortationInfo) = 0;
  virtual NTSTATUS ExportDataImpl(PORTATION_INFO* PortationInfo) = 0;
  virtual NTSTATUS ExportFinishImpl(PORTATION_INFO* PortationInfo, BOOL Success) = 0;
  virtual NTSTATUS ImportStartImpl(PORTATION_INFO* PortationInfo) = 0;
  virtual NTSTATUS ImportDataImpl(PORTATION_INFO* PortationInfo) = 0;
  virtual NTSTATUS ImportFinishImpl(PORTATION_INFO* PortationInfo, BOOL Success) = 0;
  virtual std::unique_ptr<SourceMountFileBase> DZwCreateFileImpl(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId) = 0;
  virtual std::unique_ptr<SourceMountFileBase> SwitchDestinationOpenImpl(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) = 0;

  NTSTATUS ExportStart(PORTATION_INFO* PortationInfo);
  NTSTATUS ExportData(PORTATION_INFO* PortationInfo);
  NTSTATUS ExportFinish(PORTATION_INFO* PortationInfo, BOOL Success);
  NTSTATUS ImportStart(PORTATION_INFO* PortationInfo);
  NTSTATUS ImportData(PORTATION_INFO* PortationInfo);
  NTSTATUS ImportFinish(PORTATION_INFO* PortationInfo, BOOL Success);
  NTSTATUS SwitchSourceClose(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId);
  NTSTATUS SwitchDestinationPrepare(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId);
  NTSTATUS SwitchDestinationOpen(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId);
  NTSTATUS SwitchDestinationCleanup(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId);
  NTSTATUS SwitchDestinationClose(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId);
  NTSTATUS DZwCreateFile(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId);
  void DCleanup(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId);
  void DCloseFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId);
  NTSTATUS DReadFile(LPCWSTR FileName, LPVOID Buffer, DWORD BufferLength, LPDWORD ReadLength, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId);
  NTSTATUS DWriteFile(LPCWSTR FileName, LPCVOID Buffer, DWORD NumberOfBytesToWrite, LPDWORD NumberOfBytesWritten, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId);
  NTSTATUS DFlushFileBuffers(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId);
  NTSTATUS DGetFileInformation(LPCWSTR FileName, LPBY_HANDLE_FILE_INFORMATION Buffer, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId);
  NTSTATUS DSetFileAttributes(LPCWSTR FileName, DWORD FileAttributes, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId);
  NTSTATUS DSetFileTime(LPCWSTR FileName, const FILETIME* CreationTime, const FILETIME* LastAccessTime, const FILETIME* LastWriteTime, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId);
  NTSTATUS DDeleteFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId);
  NTSTATUS DDeleteDirectory(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId);
  NTSTATUS DMoveFile(LPCWSTR FileName, LPCWSTR NewFileName, BOOL ReplaceIfExisting, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId);
  NTSTATUS DSetEndOfFile(LPCWSTR FileName, LONGLONG ByteOffset, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId);
  NTSTATUS DSetAllocationSize(LPCWSTR FileName, LONGLONG AllocSize, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId);
  NTSTATUS DGetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PULONG LengthNeeded, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId);
  NTSTATUS DSetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId);
};
