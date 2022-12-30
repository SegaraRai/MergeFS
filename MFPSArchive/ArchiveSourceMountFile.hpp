#pragma once

#include <dokan/dokan.h>

#include "../SDK/Plugin/SourceCppReadonly.hpp"

#include <string>

#include <Windows.h>

#include "NanaZ/Archive.hpp"


class ArchiveSourceMount;


class ArchiveSourceMountFile : public ReadonlySourceMountFileBase {
  ArchiveSourceMount& sourceMount;
  std::wstring realPath;
  const DirectoryTree* ptrDirectoryTree;
  DWORD fileAttributes;
  DWORD volumeSerialNumber;

public:
  ArchiveSourceMountFile(ArchiveSourceMount& sourceMount, LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId);

  NTSTATUS DReadFile(LPVOID Buffer, DWORD BufferLength, LPDWORD ReadLength, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DGetFileInformation(LPBY_HANDLE_FILE_INFORMATION Buffer, PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DGetFileSecurity(PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PULONG LengthNeeded, PDOKAN_FILE_INFO DokanFileInfo) override;
};
