#define NOMINMAX

#include <dokan/dokan.h>

#include "CueSourceMount.hpp"
#include "CueSourceMountFile.hpp"
#include "Util.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <string>

#include <Windows.h>

using namespace std::literals;



CueSourceMountFile::CueSourceMountFile(CueSourceMount& sourceMount, LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId, const FILETIME& creationTime, const FILETIME& lastAccessTime, const FILETIME& lastWriteTime) :
  ReadonlySourceMountFileBase(sourceMount, FileName, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, MaybeSwitched, FileContextId),
  sourceMount(sourceMount),
  ptrDirectoryTree(sourceMount.GetDirectoryTree(FileName)),
  creationTime(creationTime),
  lastAccessTime(lastAccessTime),
  lastWriteTime(lastWriteTime)
{
  if (!ptrDirectoryTree) {
    throw NtstatusError(sourceMount.ReturnPathOrNameNotFoundError(FileName));
  }

  fileAttributes = ptrDirectoryTree->directory ? DirectoryTree::DirectoryFileAttributes : DirectoryTree::FileFileAttributes;
  fileSize = ptrDirectoryTree->source ? ptrDirectoryTree->source->GetSize() : 0;
  volumeSerialNumber = sourceMount.GetVolumeSerialNumber();
}


NTSTATUS CueSourceMountFile::DReadFile(LPVOID Buffer, DWORD BufferLength, LPDWORD ReadLength, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo) {
  if (!ptrDirectoryTree->source) {
    return STATUS_UNSUCCESSFUL;
  }
  std::size_t readSize = 0;
  if (const auto status = ptrDirectoryTree->source->Read(Offset, static_cast<std::byte*>(Buffer), BufferLength, &readSize); status != STATUS_SUCCESS) {
    return status;
  }
  if (ReadLength) {
    *ReadLength = static_cast<DWORD>(readSize);
  }
  return STATUS_SUCCESS;
}


NTSTATUS CueSourceMountFile::DGetFileInformation(LPBY_HANDLE_FILE_INFORMATION Buffer, PDOKAN_FILE_INFO DokanFileInfo) {
  if (!Buffer) {
    return STATUS_SUCCESS;
  }
  Buffer->dwFileAttributes = fileAttributes;
  Buffer->ftCreationTime = creationTime;
  Buffer->ftLastAccessTime = lastAccessTime;
  Buffer->ftLastWriteTime = lastWriteTime;
  Buffer->dwVolumeSerialNumber = volumeSerialNumber;
  Buffer->nFileSizeHigh = (fileSize >> 32) & 0xFFFFFFFF;
  Buffer->nFileSizeLow = fileSize & 0xFFFFFFFF;
  Buffer->nNumberOfLinks = 1;
  Buffer->nFileIndexHigh = (ptrDirectoryTree->fileIndex >> 32) & 0xFFFFFFFF;
  Buffer->nFileIndexLow = ptrDirectoryTree->fileIndex & 0xFFFFFFFF;
  return STATUS_SUCCESS;
}


NTSTATUS CueSourceMountFile::DGetFileSecurity(PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PULONG LengthNeeded, PDOKAN_FILE_INFO DokanFileInfo) {
  // TODO
  return STATUS_NOT_IMPLEMENTED;
}
