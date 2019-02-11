#include "../dokan/dokan/dokan.h"

#include "DokanOperations.hpp"
#include "Mount.hpp"



ULONG64 GetGlobalContextFromMount(Mount* mount) {
  return reinterpret_cast<ULONG64>(mount);
}



namespace {
  Mount& GetMountFromFileInfo(PDOKAN_FILE_INFO DokanFileInfo) {
    return *reinterpret_cast<Mount*>(DokanFileInfo->DokanOptions->GlobalContext);
  }



  NTSTATUS DOKAN_CALLBACK DZwCreateFile(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo) {
    auto& mount = GetMountFromFileInfo(DokanFileInfo);
    return mount.DZwCreateFile(FileName, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo);
  }


  void DOKAN_CALLBACK DCleanup(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo) {
    auto& mount = GetMountFromFileInfo(DokanFileInfo);
    return mount.DCleanup(FileName, DokanFileInfo);
  }


  void DOKAN_CALLBACK DCloseFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo) {
    auto& mount = GetMountFromFileInfo(DokanFileInfo);
    return mount.DCloseFile(FileName, DokanFileInfo);
  }


  NTSTATUS DOKAN_CALLBACK DReadFile(LPCWSTR FileName, LPVOID Buffer, DWORD BufferLength, LPDWORD ReadLength, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo) {
    auto& mount = GetMountFromFileInfo(DokanFileInfo);
    return mount.DReadFile(FileName, Buffer, BufferLength, ReadLength, Offset, DokanFileInfo);
  }


  NTSTATUS DOKAN_CALLBACK DWriteFile(LPCWSTR FileName, LPCVOID Buffer, DWORD NumberOfBytesToWrite, LPDWORD NumberOfBytesWritten, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo) {
    auto& mount = GetMountFromFileInfo(DokanFileInfo);
    return mount.DWriteFile(FileName, Buffer, NumberOfBytesToWrite, NumberOfBytesWritten, Offset, DokanFileInfo);
  }


  NTSTATUS DOKAN_CALLBACK DFlushFileBuffers(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo) {
    auto& mount = GetMountFromFileInfo(DokanFileInfo);
    return mount.DFlushFileBuffers(FileName, DokanFileInfo);
  }


  NTSTATUS DOKAN_CALLBACK DGetFileInformation(LPCWSTR FileName, LPBY_HANDLE_FILE_INFORMATION Buffer, PDOKAN_FILE_INFO DokanFileInfo) {
    auto& mount = GetMountFromFileInfo(DokanFileInfo);
    return mount.DGetFileInformation(FileName, Buffer, DokanFileInfo);
  }


  NTSTATUS DOKAN_CALLBACK DFindFiles(LPCWSTR FileName, PFillFindData FillFindData, PDOKAN_FILE_INFO DokanFileInfo) {
    auto& mount = GetMountFromFileInfo(DokanFileInfo);
    return mount.DFindFiles(FileName, FillFindData, DokanFileInfo);
  }


  NTSTATUS DOKAN_CALLBACK DFindFilesWithPattern(LPCWSTR PathName, LPCWSTR SearchPattern, PFillFindData FillFindData, PDOKAN_FILE_INFO DokanFileInfo) {
    /*
    auto& mount = GetMountFromFileInfo(DokanFileInfo);
    return mount.DFindFilesWithPattern(PathName, SearchPattern, FillFindData, DokanFileInfo);
    /*/
    return STATUS_NOT_IMPLEMENTED;
    //*/
  }


  NTSTATUS DOKAN_CALLBACK DSetFileAttributes(LPCWSTR FileName, DWORD FileAttributes, PDOKAN_FILE_INFO DokanFileInfo) {
    auto& mount = GetMountFromFileInfo(DokanFileInfo);
    return mount.DSetFileAttributes(FileName, FileAttributes, DokanFileInfo);
  }


  NTSTATUS DOKAN_CALLBACK DSetFileTime(LPCWSTR FileName, CONST FILETIME *CreationTime, CONST FILETIME *LastAccessTime, CONST FILETIME *LastWriteTime, PDOKAN_FILE_INFO DokanFileInfo) {
    auto& mount = GetMountFromFileInfo(DokanFileInfo);
    return mount.DSetFileTime(FileName, CreationTime, LastAccessTime, LastWriteTime, DokanFileInfo);
  }


  NTSTATUS DOKAN_CALLBACK DDeleteFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo) {
    auto& mount = GetMountFromFileInfo(DokanFileInfo);
    return mount.DDeleteFile(FileName, DokanFileInfo);
  }


  NTSTATUS DOKAN_CALLBACK DDeleteDirectory(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo) {
    auto& mount = GetMountFromFileInfo(DokanFileInfo);
    return mount.DDeleteDirectory(FileName, DokanFileInfo);
  }


  NTSTATUS DOKAN_CALLBACK DMoveFile(LPCWSTR FileName, LPCWSTR NewFileName, BOOL ReplaceIfExisting, PDOKAN_FILE_INFO DokanFileInfo) {
    auto& mount = GetMountFromFileInfo(DokanFileInfo);
    return mount.DMoveFile(FileName, NewFileName, ReplaceIfExisting, DokanFileInfo);
  }


  NTSTATUS DOKAN_CALLBACK DSetEndOfFile(LPCWSTR FileName, LONGLONG ByteOffset, PDOKAN_FILE_INFO DokanFileInfo) {
    auto& mount = GetMountFromFileInfo(DokanFileInfo);
    return mount.DSetEndOfFile(FileName, ByteOffset, DokanFileInfo);
  }


  NTSTATUS DOKAN_CALLBACK DSetAllocationSize(LPCWSTR FileName, LONGLONG AllocSize, PDOKAN_FILE_INFO DokanFileInfo) {
    auto& mount = GetMountFromFileInfo(DokanFileInfo);
    return mount.DSetAllocationSize(FileName, AllocSize, DokanFileInfo);
  }


  NTSTATUS DOKAN_CALLBACK DLockFile(LPCWSTR FileName, LONGLONG ByteOffset, LONGLONG Length, PDOKAN_FILE_INFO DokanFileInfo) {
    /*
    auto& mount = GetMountFromFileInfo(DokanFileInfo);
    return mount.DLockFile(FileName, ByteOffset, Length, DokanFileInfo);
    /*/
    return STATUS_NOT_IMPLEMENTED;
    //*/
  }


  NTSTATUS DOKAN_CALLBACK DUnlockFile(LPCWSTR FileName, LONGLONG ByteOffset, LONGLONG Length, PDOKAN_FILE_INFO DokanFileInfo) {
    /*
    auto& mount = GetMountFromFileInfo(DokanFileInfo);
    return mount.DUnlockFile(FileName, ByteOffset, Length, DokanFileInfo);
    /*/
    return STATUS_NOT_IMPLEMENTED;
    //*/
  }


  NTSTATUS DOKAN_CALLBACK DGetDiskFreeSpace(PULONGLONG FreeBytesAvailable, PULONGLONG TotalNumberOfBytes, PULONGLONG TotalNumberOfFreeBytes, PDOKAN_FILE_INFO DokanFileInfo) {
    auto& mount = GetMountFromFileInfo(DokanFileInfo);
    return mount.DGetDiskFreeSpace(FreeBytesAvailable, TotalNumberOfBytes, TotalNumberOfFreeBytes, DokanFileInfo);
  }


  NTSTATUS DOKAN_CALLBACK DGetVolumeInformation(LPWSTR VolumeNameBuffer, DWORD VolumeNameSize, LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength, LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer, DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo) {
    auto& mount = GetMountFromFileInfo(DokanFileInfo);
    return mount.DGetVolumeInformation(VolumeNameBuffer, VolumeNameSize, VolumeSerialNumber, MaximumComponentLength, FileSystemFlags, FileSystemNameBuffer, FileSystemNameSize, DokanFileInfo);
  }


  NTSTATUS DOKAN_CALLBACK DMounted(PDOKAN_FILE_INFO DokanFileInfo) {
    auto& mount = GetMountFromFileInfo(DokanFileInfo);
    return mount.DMounted(DokanFileInfo);
  }


  NTSTATUS DOKAN_CALLBACK DUnmounted(PDOKAN_FILE_INFO DokanFileInfo) {
    auto& mount = GetMountFromFileInfo(DokanFileInfo);
    return mount.DUnmounted(DokanFileInfo);
  }


  NTSTATUS DOKAN_CALLBACK DGetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PULONG LengthNeeded, PDOKAN_FILE_INFO DokanFileInfo) {
    auto& mount = GetMountFromFileInfo(DokanFileInfo);
    return mount.DGetFileSecurity(FileName, SecurityInformation, SecurityDescriptor, BufferLength, LengthNeeded, DokanFileInfo);
  }


  NTSTATUS DOKAN_CALLBACK DSetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PDOKAN_FILE_INFO DokanFileInfo) {
    auto& mount = GetMountFromFileInfo(DokanFileInfo);
    return mount.DSetFileSecurity(FileName, SecurityInformation, SecurityDescriptor, BufferLength, DokanFileInfo);
  }


  NTSTATUS DOKAN_CALLBACK DFindStreams(LPCWSTR FileName, PFillFindStreamData FillFindStreamData, PDOKAN_FILE_INFO DokanFileInfo) {
    auto& mount = GetMountFromFileInfo(DokanFileInfo);
    return mount.DFindStreams(FileName, FillFindStreamData, DokanFileInfo);
  }
}



const DOKAN_OPERATIONS gDokanOperations = {
  DZwCreateFile,
  DCleanup,
  DCloseFile,
  DReadFile,
  DWriteFile,
  DFlushFileBuffers,
  DGetFileInformation,
  DFindFiles,
  nullptr,  //DFindFilesWithPattern,
  DSetFileAttributes,
  DSetFileTime,
  DDeleteFile,
  DDeleteDirectory,
  DMoveFile,
  DSetEndOfFile,
  DSetAllocationSize,
  nullptr,  //DLockFile,
  nullptr,  //DUnlockFile,
  DGetDiskFreeSpace,
  DGetVolumeInformation,
  DMounted,
  DUnmounted,
  DGetFileSecurity,
  DSetFileSecurity,
  DFindStreams,
};
