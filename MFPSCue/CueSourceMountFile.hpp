#pragma once

#include <dokan/dokan.h>

#include "../SDK/Plugin/SourceCppReadonly.hpp"

#include <string>

#include <Windows.h>

#include "DirectoryTree.hpp"
#include "Source.hpp"


class CueSourceMount;


class CueSourceMountFile : public ReadonlySourceMountFileBase {
  CueSourceMount& sourceMount;
  const DirectoryTree* ptrDirectoryTree;
  FILETIME creationTime;
  FILETIME lastAccessTime;
  FILETIME lastWriteTime;
  DWORD fileAttributes;
  Source::SourceSize fileSize;
  DWORD volumeSerialNumber;

public:
  CueSourceMountFile(CueSourceMount& sourceMount, LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId, const FILETIME& creationTime, const FILETIME& lastAccessTime, const FILETIME& lastWriteTime);
  void DCleanup(PDOKAN_FILE_INFO DokanFileInfo) override;
  void DCloseFile(PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DReadFile(LPVOID Buffer, DWORD BufferLength, LPDWORD ReadLength, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DGetFileInformation(LPBY_HANDLE_FILE_INFORMATION Buffer, PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DGetFileSecurity(PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PULONG LengthNeeded, PDOKAN_FILE_INFO DokanFileInfo) override;
};
