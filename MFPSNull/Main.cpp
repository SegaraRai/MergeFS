#define NOMINMAX

#include "../dokan/dokan/dokan.h"

#include "../SDK/Plugin/Source.h"

#include <algorithm>
#include <string_view>

#include <Windows.h>

using namespace std;



namespace {
  // {8F3ACF43-D9DD-0000-1010-100000000000}
  constexpr GUID DPluginGUID = {0x8F3ACF43, 0xD9DD, 0x0000, {0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00}};

  constexpr WIN32_FILE_ATTRIBUTE_DATA DRootWin32FileAttributeData = {
    FILE_ATTRIBUTE_DIRECTORY,
    {0, 0},
    {0, 0},
    {0, 0},
    0,
    0,
  };
  constexpr DWORD DVolumeSerialNumber = 0x10000001;
  constexpr wchar_t DVolumeName[] = L"NULLFS";
  constexpr DWORD DMaximumComponentLength = MAX_PATH;
  constexpr DWORD DFileSystemFlags = FILE_CASE_PRESERVED_NAMES | FILE_CASE_SENSITIVE_SEARCH | FILE_READ_ONLY_VOLUME | FILE_UNICODE_ON_DISK;
  constexpr wchar_t DFileSystemName[] = L"NULLFS";
  constexpr SOURCE_INFO DSourceInfo = {
    FALSE,
  };

  const PLUGIN_INFO gPluginInfo = {
    MERGEFS_PLUGIN_INTERFACE_VERSION,
    PLUGIN_TYPE::Source,
    DPluginGUID,
    L"nullfs",
    L"a null filesystem",
    0x00000001,
    L"0.0.1",
  };



  bool IsSupported(wstring_view wstr) noexcept {
    return wstr == L"NULLFS";
  }


  bool IsRootDirectory(LPCWSTR FilePath) noexcept {
    if (FilePath[0] == L'\0') {
      return true;
    }
    if (FilePath[1] == L'\0' && (FilePath[0] == L'\\' || FilePath[0] == L'/')) {
      return true;
    }
    return false;
  }
}



BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
  return TRUE;
}



const PLUGIN_INFO* WINAPI SGetPluginInfo() MFNOEXCEPT {
  return &gPluginInfo;
}


PLUGIN_INITCODE WINAPI SInitialize(const PLUGIN_INITIALIZE_INFO* InitializeInfo) MFNOEXCEPT {
  return PLUGIN_INITCODE::Success;
}


BOOL WINAPI SIsSupported(const PLUGIN_INITIALIZE_MOUNT_INFO* InitializeMountInfo) MFNOEXCEPT {
  return IsSupported(InitializeMountInfo->FileName);
}



NTSTATUS WINAPI Mount(const PLUGIN_INITIALIZE_MOUNT_INFO* InitializeMountInfo, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return STATUS_SUCCESS;
}


BOOL WINAPI Unmount(SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return TRUE;
}


BOOL WINAPI GetSourceInfo(SOURCE_INFO* sourceInfo, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  if (!sourceInfo) {
    return TRUE;
  }
  *sourceInfo = DSourceInfo;
  return TRUE;
}


NTSTATUS WINAPI GetFileInfo(LPCWSTR FileName, WIN32_FILE_ATTRIBUTE_DATA* Win32FileAttributeData, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  if (IsRootDirectory(FileName)) {
    if (Win32FileAttributeData) {
      *Win32FileAttributeData = DRootWin32FileAttributeData;
    }
    return STATUS_SUCCESS;
  }
  // TODO: STATUS_OBJECT_NAME_NOT_FOUNDとSTATUS_OBJECT_PATH_NOT_FOUNDの使い分け
  return STATUS_OBJECT_NAME_NOT_FOUND;
}


NTSTATUS WINAPI GetDirectoryInfo(LPCWSTR FileName, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  if (IsRootDirectory(FileName)) {
    return STATUS_SUCCESS;
  }
  return STATUS_NOT_A_DIRECTORY;
}


NTSTATUS WINAPI RemoveFile(LPCWSTR FileName, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  if (IsRootDirectory(FileName)) {
    return STATUS_ACCESS_DENIED;
  }
  // TODO: STATUS_OBJECT_NAME_NOT_FOUNDとSTATUS_OBJECT_PATH_NOT_FOUNDの使い分け
  return STATUS_OBJECT_NAME_NOT_FOUND;
}


NTSTATUS WINAPI ExportStart(PORTATION_INFO* PortationInfo, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  if (IsRootDirectory(PortationInfo->filepath)) {
    return STATUS_ACCESS_DENIED;
  }
  // TODO: STATUS_OBJECT_NAME_NOT_FOUNDとSTATUS_OBJECT_PATH_NOT_FOUNDの使い分け
  return STATUS_OBJECT_NAME_NOT_FOUND;
}


NTSTATUS WINAPI ExportData(PORTATION_INFO* PortationInfo, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return STATUS_INVALID_HANDLE;
}


NTSTATUS WINAPI ExportFinish(PORTATION_INFO* PortationInfo, BOOL Success, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return STATUS_INVALID_HANDLE;
}


NTSTATUS WINAPI ImportStart(PORTATION_INFO* PortationInfo, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return STATUS_ACCESS_DENIED;
}


NTSTATUS WINAPI ImportData(PORTATION_INFO* PortationInfo, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return STATUS_INVALID_HANDLE;
}


NTSTATUS WINAPI ImportFinish(PORTATION_INFO* PortationInfo, BOOL Success, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return STATUS_INVALID_HANDLE;
}


NTSTATUS WINAPI SwitchSourceClose(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return STATUS_ACCESS_DENIED;
}


NTSTATUS WINAPI SwitchDestinationPrepare(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return STATUS_ACCESS_DENIED;
}


NTSTATUS WINAPI SwitchDestinationOpen(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return STATUS_ACCESS_DENIED;
}


NTSTATUS WINAPI SwitchDestinationCleanup(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return STATUS_ACCESS_DENIED;
}


NTSTATUS WINAPI SwitchDestinationClose(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return STATUS_ACCESS_DENIED;
}


NTSTATUS WINAPI ListFiles(LPCWSTR FileName, PListFilesCallback Callback, CALLBACK_CONTEXT CallbackContext, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  if (IsRootDirectory(FileName)) {
    return STATUS_SUCCESS;
  }
  // TODO: STATUS_OBJECT_NAME_NOT_FOUNDとSTATUS_OBJECT_PATH_NOT_FOUNDの使い分け
  return STATUS_OBJECT_NAME_NOT_FOUND;
}


NTSTATUS WINAPI ListStreams(LPCWSTR FileName, PListStreamsCallback Callback, CALLBACK_CONTEXT CallbackContext, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  if (IsRootDirectory(FileName)) {
    return STATUS_SUCCESS;
  }
  // TODO: STATUS_OBJECT_NAME_NOT_FOUNDとSTATUS_OBJECT_PATH_NOT_FOUNDの使い分け
  return STATUS_OBJECT_NAME_NOT_FOUND;
}



NTSTATUS WINAPI DZwCreateFile(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  if (IsRootDirectory(FileName)) {
    if (CreateOptions & FILE_NON_DIRECTORY_FILE) {
      return STATUS_FILE_IS_A_DIRECTORY;
    }
    if (CreateDisposition == FILE_CREATE) {
      return STATUS_OBJECT_NAME_COLLISION;
    }
    if (CreateDisposition == FILE_OVERWRITE || CreateDisposition == FILE_OVERWRITE_IF || CreateDisposition == FILE_SUPERSEDE) {
      return STATUS_ACCESS_DENIED;
    }
    if (CreateOptions & FILE_DELETE_ON_CLOSE) {
      return STATUS_CANNOT_DELETE;
    }
    // success
    if (CreateDisposition == FILE_OPEN_IF || CreateDisposition == FILE_OVERWRITE_IF || CreateDisposition == FILE_SUPERSEDE) {
      return STATUS_OBJECT_NAME_COLLISION;
    }
    return STATUS_SUCCESS;
  }
  // TODO: STATUS_OBJECT_NAME_NOT_FOUNDとSTATUS_OBJECT_PATH_NOT_FOUNDの使い分け
  return CreateDisposition == FILE_OPEN || CreateDisposition == FILE_OVERWRITE ? STATUS_OBJECT_NAME_NOT_FOUND : STATUS_ACCESS_DENIED;
}


void WINAPI DCleanup(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return;
}


void WINAPI DCloseFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return;
}


NTSTATUS WINAPI DReadFile(LPCWSTR FileName, LPVOID Buffer, DWORD BufferLength, LPDWORD ReadLength, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  // MUST BE THEAD SAFE
  return STATUS_ACCESS_DENIED;
}


NTSTATUS WINAPI DWriteFile(LPCWSTR FileName, LPCVOID Buffer, DWORD NumberOfBytesToWrite, LPDWORD NumberOfBytesWritten, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  // MUST BE THEAD SAFE
  return STATUS_ACCESS_DENIED;
}


NTSTATUS WINAPI DFlushFileBuffers(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return STATUS_ACCESS_DENIED;
}


NTSTATUS WINAPI DGetFileInformation(LPCWSTR FileName, LPBY_HANDLE_FILE_INFORMATION Buffer, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  if (Buffer) {
    *Buffer = {
      DRootWin32FileAttributeData.dwFileAttributes,
      DRootWin32FileAttributeData.ftCreationTime,
      DRootWin32FileAttributeData.ftLastAccessTime,
      DRootWin32FileAttributeData.ftLastWriteTime,
      DVolumeSerialNumber,
      0,
      0,
      0,
      0,
      0,
    };
  }
  return STATUS_SUCCESS;
}


NTSTATUS WINAPI DSetFileAttributes(LPCWSTR FileName, DWORD FileAttributes, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return STATUS_ACCESS_DENIED;
}


NTSTATUS WINAPI DSetFileTime(LPCWSTR FileName, const FILETIME* CreationTime, const FILETIME* LastAccessTime, const FILETIME* LastWriteTime, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return STATUS_ACCESS_DENIED;
}


NTSTATUS WINAPI DDeleteFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return STATUS_ACCESS_DENIED;
}


NTSTATUS WINAPI DDeleteDirectory(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return STATUS_ACCESS_DENIED;
}


NTSTATUS WINAPI DMoveFile(LPCWSTR FileName, LPCWSTR NewFileName, BOOL ReplaceIfExisting, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return STATUS_ACCESS_DENIED;
}


NTSTATUS WINAPI DSetEndOfFile(LPCWSTR FileName, LONGLONG ByteOffset, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return STATUS_ACCESS_DENIED;
}


NTSTATUS WINAPI DSetAllocationSize(LPCWSTR FileName, LONGLONG AllocSize, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return STATUS_ACCESS_DENIED;
}


NTSTATUS WINAPI DGetDiskFreeSpace(PULONGLONG FreeBytesAvailable, PULONGLONG TotalNumberOfBytes, PULONGLONG TotalNumberOfFreeBytes, PDOKAN_FILE_INFO DokanFileInfo, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  if (FreeBytesAvailable) {
    *FreeBytesAvailable = 0;
  }
  if (TotalNumberOfBytes) {
    *TotalNumberOfBytes = 1;
  }
  if (TotalNumberOfFreeBytes) {
    *TotalNumberOfFreeBytes = 0;
  }
  return STATUS_SUCCESS;
}


NTSTATUS WINAPI DGetVolumeInformation(LPWSTR VolumeNameBuffer, DWORD VolumeNameSize, LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength, LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer, DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  if (VolumeNameBuffer) {
    memcpy(VolumeNameBuffer, DVolumeName, min<std::size_t>(VolumeNameSize * sizeof(wchar_t), sizeof(DVolumeName)));
  }
  if (VolumeSerialNumber) {
    *VolumeSerialNumber = DVolumeSerialNumber;
  }
  if (MaximumComponentLength) {
    *MaximumComponentLength = DMaximumComponentLength;
  }
  if (FileSystemFlags) {
    *FileSystemFlags = DFileSystemFlags;
  }
  if (FileSystemNameBuffer) {
    memcpy(FileSystemNameBuffer, DFileSystemName, min<std::size_t>(FileSystemNameSize * sizeof(wchar_t), sizeof(DFileSystemName)));
  }
  return STATUS_SUCCESS;
}


NTSTATUS WINAPI DGetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PULONG LengthNeeded, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return STATUS_NOT_IMPLEMENTED;
}


NTSTATUS WINAPI DSetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT {
  return STATUS_ACCESS_DENIED;
}
