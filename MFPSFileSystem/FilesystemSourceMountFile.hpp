#pragma once

#include <dokan/dokan.h>

#include "../SDK/Plugin/SourceCpp.hpp"

#include <optional>
#include <string>

#include <Windows.h>



class FilesystemSourceMount;


class FilesystemSourceMountFile : public SourceMountFileBase {
  FilesystemSourceMount& sourceMount;
  const std::wstring realPath;
  bool directory;
  DWORD existingFileAttributes;
  HANDLE hFile;

  FilesystemSourceMountFile(FilesystemSourceMount& sourceMount, LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, std::optional<BOOL> MaybeSwitchedN);

public:
  FilesystemSourceMountFile(FilesystemSourceMount& sourceMount, LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId);
  FilesystemSourceMountFile(FilesystemSourceMount& sourceMount, LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId);
  ~FilesystemSourceMountFile();

  HANDLE GetFileHandle();

  NTSTATUS SwitchDestinationCleanupImpl(PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS SwitchDestinationCloseImpl(PDOKAN_FILE_INFO DokanFileInfo) override;
  void DCleanupImpl(PDOKAN_FILE_INFO DokanFileInfo) override;
  void DCloseFileImpl(PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DReadFile(LPVOID Buffer, DWORD BufferLength, LPDWORD ReadLength, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DWriteFile(LPCVOID Buffer, DWORD NumberOfBytesToWrite, LPDWORD NumberOfBytesWritten, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DFlushFileBuffers(PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DGetFileInformation(LPBY_HANDLE_FILE_INFORMATION Buffer, PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DSetFileAttributes(DWORD FileAttributes, PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DSetFileTime(const FILETIME* CreationTime, const FILETIME* LastAccessTime, const FILETIME* LastWriteTime, PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DDeleteFile(PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DDeleteDirectory(PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DMoveFile(LPCWSTR NewFileName, BOOL ReplaceIfExisting, PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DSetEndOfFile(LONGLONG ByteOffset, PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DSetAllocationSize(LONGLONG AllocSize, PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DGetFileSecurity(PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PULONG LengthNeeded, PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DSetFileSecurity(PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PDOKAN_FILE_INFO DokanFileInfo) override;
};
