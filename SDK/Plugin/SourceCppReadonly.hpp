#pragma once

#include "SourceCpp.hpp"

#include <memory>

#include <Windows.h>



class ReadonlySourceMountFileBase : public SourceMountFileBase {
protected:
  ReadonlySourceMountFileBase(SourceMountBase& sourceMountBase, LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId);

public:
  virtual ~ReadonlySourceMountFileBase() = default;

  NTSTATUS ImportStart(PORTATION_INFO* PortationInfo) override;
  NTSTATUS ImportData(PORTATION_INFO* PortationInfo) override;
  NTSTATUS ImportFinish(PORTATION_INFO* PortationInfo, BOOL Success) override;
  NTSTATUS SwitchDestinationCleanup(PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS SwitchDestinationClose(PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DWriteFile(LPCVOID Buffer, DWORD NumberOfBytesToWrite, LPDWORD NumberOfBytesWritten, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DFlushFileBuffers(PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DSetFileAttributes(DWORD FileAttributes, PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DSetFileTime(const FILETIME* CreationTime, const FILETIME* LastAccessTime, const FILETIME* LastWriteTime, PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DDeleteFile(PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DDeleteDirectory(PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DMoveFile(LPCWSTR NewFileName, BOOL ReplaceIfExisting, PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DSetEndOfFile(LONGLONG ByteOffset, PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DSetAllocationSize(LONGLONG AllocSize, PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DSetFileSecurity(PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PDOKAN_FILE_INFO DokanFileInfo) override;

  /*
  virtual NTSTATUS ExportStart(PORTATION_INFO* PortationInfo);
  virtual NTSTATUS ExportData(PORTATION_INFO* PortationInfo);
  virtual NTSTATUS ExportFinish(PORTATION_INFO* PortationInfo, BOOL Success);

  virtual NTSTATUS SwitchSourceClose(PDOKAN_FILE_INFO DokanFileInfo) = 0;
  virtual void DCleanup(PDOKAN_FILE_INFO DokanFileInfo) = 0;
  virtual void DCloseFile(PDOKAN_FILE_INFO DokanFileInfo) = 0;
  virtual NTSTATUS DReadFile(LPVOID Buffer, DWORD BufferLength, LPDWORD ReadLength, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo) = 0;
  virtual NTSTATUS DGetFileInformation(LPBY_HANDLE_FILE_INFORMATION Buffer, PDOKAN_FILE_INFO DokanFileInfo) = 0;
  virtual NTSTATUS DGetFileSecurity(PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PULONG LengthNeeded, PDOKAN_FILE_INFO DokanFileInfo) = 0;
  */
};


class ReadonlySourceMountBase : public SourceMountBase {
protected:
  ReadonlySourceMountBase(LPCWSTR FileName, SOURCE_CONTEXT_ID sourceContextId);

public:
  virtual ~ReadonlySourceMountBase() = default;

  NTSTATUS RemoveFile(LPCWSTR FileName) override;
  NTSTATUS SwitchDestinationPrepareImpl(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) override;
  NTSTATUS SwitchDestinationCloseImpl(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) override;
  NTSTATUS ImportStartImpl(PORTATION_INFO* PortationInfo) override;
  NTSTATUS ImportDataImpl(PORTATION_INFO* PortationInfo) override;
  NTSTATUS ImportFinishImpl(PORTATION_INFO* PortationInfo, BOOL Success) override;
  std::unique_ptr<SourceMountFileBase> SwitchDestinationOpenImpl(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) override;

  /*
  virtual BOOL GetSourceInfo(SOURCE_INFO* sourceInfo) = 0;
  virtual NTSTATUS GetFileInfo(LPCWSTR FileName, WIN32_FILE_ATTRIBUTE_DATA* Win32FileAttributeData) = 0;
  virtual NTSTATUS GetDirectoryInfo(LPCWSTR FileName) = 0;
  virtual NTSTATUS ListFiles(LPCWSTR FileName, PListFilesCallback Callback, CALLBACK_CONTEXT CallbackContext) = 0;
  virtual NTSTATUS ListStreams(LPCWSTR FileName, PListStreamsCallback Callback, CALLBACK_CONTEXT CallbackContext) = 0;
  virtual NTSTATUS DGetDiskFreeSpace(PULONGLONG FreeBytesAvailable, PULONGLONG TotalNumberOfBytes, PULONGLONG TotalNumberOfFreeBytes, PDOKAN_FILE_INFO DokanFileInfo) = 0;
  virtual NTSTATUS DGetVolumeInformation(LPWSTR VolumeNameBuffer, DWORD VolumeNameSize, LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength, LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer, DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo) = 0;
  virtual NTSTATUS ExportStartImpl(PORTATION_INFO* PortationInfo) = 0;
  virtual NTSTATUS ExportDataImpl(PORTATION_INFO* PortationInfo) = 0;
  virtual NTSTATUS ExportFinishImpl(PORTATION_INFO* PortationInfo, BOOL Success) = 0;
  virtual std::unique_ptr<SourceMountFileBase> DZwCreateFileImpl(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId) = 0;
  */
};
