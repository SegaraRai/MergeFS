#define NOMINMAX

#include <dokan/dokan.h>

#include "../SDK/Plugin/Source.h"
#include "../SDK/Plugin/SourceCpp.hpp"

#include <cassert>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include <Windows.h>
#include <shlwapi.h>
#include <pathcch.h>

using namespace std::literals;


namespace {
  // {8E0E5D1A-3FA3-461F-AF4C-B4A2E8D2E3A3}
  constexpr GUID DPluginGUID = {0x8e0e5d1a, 0x3fa3, 0x461f, {0xaf, 0x4c, 0xb4, 0xa2, 0xe8, 0xd2, 0xe3, 0xa3}};

  constexpr DWORD DMaximumComponentLength = MAX_PATH;
  constexpr DWORD DFileSystemFlags = FILE_CASE_PRESERVED_NAMES | FILE_CASE_SENSITIVE_SEARCH | FILE_UNICODE_ON_DISK;

  const PLUGIN_INFO gPluginInfo = {
    MERGEFS_PLUGIN_INTERFACE_VERSION,
    PLUGIN_TYPE::Source,
    DPluginGUID,
    L"filesystemfs",
    L"filesystem source plugin",
    0x00000001,
    L"0.0.1",
  };

  PLUGIN_INITIALIZE_INFO gPluginInitializeInfo{};

  bool IsRootDirectory(LPCWSTR FilePath) noexcept {
    return FilePath[0] == L'\\' && FilePath[1] == L'\0';
  }

  NTSTATUS NtstatusFromWin32(DWORD win32ErrorCode = GetLastError()) {
    return gPluginInitializeInfo.dokanFuncs.DokanNtStatusFromWin32 ? gPluginInitializeInfo.dokanFuncs.DokanNtStatusFromWin32(win32ErrorCode) : STATUS_UNSUCCESSFUL;
  }

  NTSTATUS NtstatusFromWin32Api(BOOL Result) {
    return Result ? STATUS_SUCCESS : NtstatusFromWin32();
  }
}



class SourceMountFile : public SourceMountFileBase {
  const std::wstring realPath;
  bool directory;
  HANDLE hFile = NULL;

  void Constructor() {

  }

public:
  SourceMountFile(SourceMountBase& sourceMount, LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId, std::wstring_view realPath) :
    SourceMountFileBase(sourceMount, FileName, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, MaybeSwitched, FileContextId),
    realPath(realPath)
  {
    Constructor();
  }

  SourceMountFile(SourceMountBase& sourceMount, LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, std::wstring_view realPath) :
    SourceMountFileBase(sourceMount, FileName, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, FileContextId),
    realPath(realPath) 
  {
    Constructor();
  }

  ~SourceMountFile() {
    // in case
    if (hFile != NULL && hFile != INVALID_HANDLE_VALUE) {
      CloseHandle(hFile);
      hFile = NULL;
    }
  }

  NTSTATUS SwitchSourceClose(PDOKAN_FILE_INFO DokanFileInfo) {

  }

  NTSTATUS SwitchDestinationCleanup(PDOKAN_FILE_INFO DokanFileInfo) {

  }

  NTSTATUS SwitchDestinationClose(PDOKAN_FILE_INFO DokanFileInfo) {

  }

  void DCleanup(PDOKAN_FILE_INFO DokanFileInfo) {
    if (hFile == NULL || hFile == INVALID_HANDLE_VALUE) {
      return;
    }
  }

  void DCloseFile(PDOKAN_FILE_INFO DokanFileInfo) {
    if (hFile == NULL || hFile == INVALID_HANDLE_VALUE) {
      return;
    }
    CloseHandle(hFile);
    hFile = NULL;
  }

  NTSTATUS DReadFile(LPVOID Buffer, DWORD BufferLength, LPDWORD ReadLength, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo) {
    if (hFile == NULL || hFile == INVALID_HANDLE_VALUE) {
      return STATUS_INVALID_HANDLE;
    }
  }

  NTSTATUS DWriteFile(LPCVOID Buffer, DWORD NumberOfBytesToWrite, LPDWORD NumberOfBytesWritten, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo) {
    if (hFile == NULL || hFile == INVALID_HANDLE_VALUE) {
      return STATUS_INVALID_HANDLE;
    }
  }

  NTSTATUS DFlushFileBuffers(PDOKAN_FILE_INFO DokanFileInfo) {
    if (hFile == NULL || hFile == INVALID_HANDLE_VALUE) {
      return STATUS_INVALID_HANDLE;
    }
    return NtstatusFromWin32Api(FlushFileBuffers(hFile));
  }

  NTSTATUS DGetFileInformation(LPBY_HANDLE_FILE_INFORMATION Buffer, PDOKAN_FILE_INFO DokanFileInfo) {
    if (hFile == NULL || hFile == INVALID_HANDLE_VALUE) {
      return STATUS_INVALID_HANDLE;
    }
    return NtstatusFromWin32Api(GetFileInformationByHandle(hFile, Buffer));
  }

  NTSTATUS DSetFileAttributes(DWORD FileAttributes, PDOKAN_FILE_INFO DokanFileInfo) {
    if (hFile == NULL || hFile == INVALID_HANDLE_VALUE) {
      return STATUS_INVALID_HANDLE;
    }
    if (!FileAttributes) {
      // see [MS-FSCC]: File Attributes - https://msdn.microsoft.com/en-us/library/cc232110.aspx
      return STATUS_SUCCESS;
    }
    return NtstatusFromWin32Api(SetFileAttributesW(filename.c_str(), FileAttributes));
  }

  NTSTATUS DSetFileTime(const FILETIME* CreationTime, const FILETIME* LastAccessTime, const FILETIME* LastWriteTime, PDOKAN_FILE_INFO DokanFileInfo) {
    if (hFile == NULL || hFile == INVALID_HANDLE_VALUE) {
      return STATUS_INVALID_HANDLE;
    }
    return NtstatusFromWin32Api(SetFileTime(hFile, CreationTime, LastAccessTime, LastWriteTime));
  }

  NTSTATUS DDeleteFile(PDOKAN_FILE_INFO DokanFileInfo) {
    if (hFile == NULL || hFile == INVALID_HANDLE_VALUE) {
      return STATUS_INVALID_HANDLE;
    }
    if (directory) {
      return STATUS_ACCESS_DENIED;
    }
    FILE_DISPOSITION_INFO fileDispositionInfo{
      DokanFileInfo->DeleteOnClose,
    };
    return NtstatusFromWin32Api(SetFileInformationByHandle(hFile, FileDispositionInfo, &fileDispositionInfo, sizeof(fileDispositionInfo)));
  }

  NTSTATUS DDeleteDirectory(PDOKAN_FILE_INFO DokanFileInfo) {
    if (hFile == NULL || hFile == INVALID_HANDLE_VALUE) {
      return STATUS_INVALID_HANDLE;
    }
    if (!DokanFileInfo->DeleteOnClose) {
      return STATUS_SUCCESS;
    }
    return sourceMount.GetDirectoryInfo(filename.c_str());
  }

  NTSTATUS DMoveFile(LPCWSTR NewFileName, BOOL ReplaceIfExisting, PDOKAN_FILE_INFO DokanFileInfo) {
    if (hFile == NULL || hFile == INVALID_HANDLE_VALUE) {
      return STATUS_INVALID_HANDLE;
    }

  }

  NTSTATUS DSetEndOfFile(LONGLONG ByteOffset, PDOKAN_FILE_INFO DokanFileInfo) {
    if (hFile == NULL || hFile == INVALID_HANDLE_VALUE) {
      return STATUS_INVALID_HANDLE;
    }
  }

  NTSTATUS DSetAllocationSize(LONGLONG AllocSize, PDOKAN_FILE_INFO DokanFileInfo) {
    if (hFile == NULL || hFile == INVALID_HANDLE_VALUE) {
      return STATUS_INVALID_HANDLE;
    }
  }

  NTSTATUS DGetFileSecurity(PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PULONG LengthNeeded, PDOKAN_FILE_INFO DokanFileInfo) {
    if (hFile == NULL || hFile == INVALID_HANDLE_VALUE) {
      return STATUS_INVALID_HANDLE;
    }
  }

  NTSTATUS DSetFileSecurity(PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PDOKAN_FILE_INFO DokanFileInfo) {
    if (hFile == NULL || hFile == INVALID_HANDLE_VALUE) {
      return STATUS_INVALID_HANDLE;
    }
  }

  HANDLE GetFileHandle() {
    return hFile;
  }
};



class SourceMount : public SourceMountBase {
  class Portation {
  protected:
    SourceMount& sourceMount;
    const std::wstring filepath;
    const FILE_CONTEXT_ID fileContextId;
    const bool empty;
    SourceMountFile* const sourceMountFile;
    HANDLE hFile = NULL;
    bool needClose = false;

  public:
    Portation(SourceMount& sourceMount, PORTATION_INFO* portationInfo) :
      sourceMount(sourceMount),
      filepath(portationInfo->filepath),
      fileContextId(portationInfo->fileContextId),
      empty(portationInfo->empty),
      sourceMountFile(portationInfo->fileContextId != FILE_CONTEXT_ID_NULL ? dynamic_cast<SourceMountFile*>(&sourceMount.GetSourceMountFile(portationInfo->fileContextId)) : nullptr)
    {
      hFile = sourceMountFile ? sourceMountFile->GetFileHandle() : NULL;
      if (hFile == NULL || hFile == INVALID_HANDLE_VALUE) {
        // TODO:
        hFile = CreateFileW();
        if (hFile == INVALID_HANDLE_VALUE) {
          throw Win32Error();
        }
        needClose = true;
      }
    }

    virtual ~Portation() {
      if (needClose && hFile != NULL && hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
        hFile = NULL;
      }
    }

    virtual NTSTATUS Finish(PORTATION_INFO* portationInfo, bool success) {
      if (needClose && hFile != NULL && hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
        hFile = NULL;
      }
    }
  };

  class ExportPortation : public Portation {
  public:
    ExportPortation(SourceMount& sourceMount, PORTATION_INFO* portationInfo) : Portation(sourceMount, portationInfo) {

    }

    NTSTATUS Export(PORTATION_INFO* portationInfo) {

    }

    NTSTATUS Finish(PORTATION_INFO* portationInfo, bool success) {

    }
  };

  class ImportPortation : public Portation {
  public:
    ImportPortation(SourceMount& sourceMount, PORTATION_INFO* portationInfo) : Portation(sourceMount, portationInfo) {

    }

    NTSTATUS Import(PORTATION_INFO* portationInfo) {

    }

    NTSTATUS Finish(PORTATION_INFO* portationInfo, bool success) {

    }
  };


  const std::wstring realPathPrefix;
  const std::wstring rootPath;  
  std::unordered_map<FILE_CONTEXT_ID, HANDLE> switchPrepareHandleMap;
  std::unordered_map<Portation*, std::unique_ptr<Portation>> portationMap;
  std::mutex subMutex;

  static std::wstring GetRealPathPrefix(const std::wstring& filepath) {
    // convert to absolute path
    const DWORD size = GetFullPathNameW(filepath.c_str(), 0, NULL, NULL);
    if (!size) {
      throw Win32Error();
    }
    auto buffer = std::make_unique<wchar_t[]>(size);
    DWORD size2 = GetFullPathNameW(filepath.c_str(), size, buffer.get(), NULL);
    if (!size2) {
      throw Win32Error();
    }
    // trim trailing backslash
    if (buffer[size2 - 1] == L'\\') {
      buffer[--size2] = L'\0';
    }
    return std::wstring(buffer.get(), size2);
  }

  static std::wstring GetRootPath(const std::wstring& realPathPrefix) {
    // TODO
  }

public:
  SourceMount(LPCWSTR FileName, SOURCE_CONTEXT_ID sourceContextId) :
    SourceMountBase(FileName, sourceContextId),
    realPathPrefix(GetRealPathPrefix(filename)),
    rootPath(GetRootPath(realPathPrefix))
  {}

  std::wstring GetRealPath(LPCWSTR filepath) {
    assert(filepath && filepath[0] == L'\\');
    return realPathPrefix + std::wstring(filepath);
  }

  BOOL GetSourceInfo(SOURCE_INFO* sourceInfo) {
    if (!sourceInfo) {
      return FALSE;
    }
    *sourceInfo = {
      TRUE,
    };
    return TRUE;
  }

  NTSTATUS GetFileInfo(LPCWSTR FileName, DWORD* FileAttributes, FILETIME* CreationTime, FILETIME* LastAccessTime, FILETIME* LastWriteTime) {
    const std::wstring realPath = GetRealPath(FileName);
    WIN32_FILE_ATTRIBUTE_DATA win32FileAttributeData;
    const auto ret = GetFileAttributesExW(realPath.c_str(), GetFileExInfoStandard, &win32FileAttributeData);
    if (!ret) {
      return NtstatusFromWin32();
    }
    if (FileAttributes) {
      *FileAttributes = win32FileAttributeData.dwFileAttributes;
    }
    if (CreationTime) {
      *CreationTime = win32FileAttributeData.ftCreationTime;
    }
    if (LastAccessTime) {
      *LastAccessTime = win32FileAttributeData.ftLastAccessTime;
    }
    if (LastWriteTime) {
      *LastWriteTime = win32FileAttributeData.ftLastWriteTime;
    }
    return STATUS_SUCCESS;
  }

  NTSTATUS GetDirectoryInfo(LPCWSTR FileName) {
    const std::wstring realPath = GetRealPath(FileName);
    const auto csRealPath = realPath.c_str();
    const DWORD fileAttributes = GetFileAttributesW(csRealPath);
    if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
      return NtstatusFromWin32();
    }
    if (!(fileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      return STATUS_NOT_A_DIRECTORY;
    }
    return PathIsDirectoryEmptyW(csRealPath) ? STATUS_SUCCESS : STATUS_DIRECTORY_NOT_EMPTY;
  }

  NTSTATUS RemoveFile(LPCWSTR FileName) {
    const std::wstring realPath = GetRealPath(FileName);
    return NtstatusFromWin32Api(DeleteFileW(realPath.c_str()));
  }

  NTSTATUS ListFiles(LPCWSTR FileName, PListFilesCallback Callback, CALLBACK_CONTEXT CallbackContext) {
    const std::wstring realPath = GetRealPath(FileName);
    WIN32_FIND_DATAW win32FindData;
    HANDLE hFind = FindFirstFileW(realPath.c_str(), &win32FindData);
    if (hFind == INVALID_HANDLE_VALUE) {
      return NtstatusFromWin32();
    }
    do {
      Callback(&win32FindData, CallbackContext);
    } while (FindNextFileW(hFind, &win32FindData));
    const DWORD error = GetLastError();
    return error == ERROR_SUCCESS || error == ERROR_NO_MORE_FILES ? STATUS_SUCCESS : NtstatusFromWin32(error);
  }

  NTSTATUS ListStreams(LPCWSTR FileName, PListStreamsCallback Callback, CALLBACK_CONTEXT CallbackContext) {
    // TODO
    const std::wstring realPath = GetRealPath(FileName);
    return STATUS_SUCCESS;
  }

  NTSTATUS SwitchDestinationPrepareImpl(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
    const std::wstring realPath = GetRealPath(FileName);

    ACCESS_MASK userDesiredAccess;
    DWORD fileAttributesAndFlags;
    DWORD creationDisposition;
    gPluginInitializeInfo.dokanFuncs.DokanMapKernelToUserCreateFileFlags(DesiredAccess, FileAttributes, CreateOptions, CreateDisposition, &userDesiredAccess, &fileAttributesAndFlags, &creationDisposition);

    SECURITY_ATTRIBUTES securityAttributes{
      sizeof(securityAttributes),
      SecurityContext->AccessState.SecurityDescriptor,
      FALSE,
    };

    HANDLE hFile = CreateFileW(realPath.c_str(), DesiredAccess, ShareAccess, &securityAttributes, CREATE_NEW, fileAttributesAndFlags, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
      return NtstatusFromWin32();
    }
    
    std::lock_guard<std::mutex> lock(subMutex);
    switchPrepareHandleMap.emplace(FileContextId, hFile);
  }

  NTSTATUS SwitchDestinationCloseImpl(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
    const std::wstring realPath = GetRealPath(FileName);

    HANDLE hFile = NULL;
    {
      std::lock_guard<std::mutex> lock(subMutex);
      if (!switchPrepareHandleMap.count(FileContextId)) {
        return STATUS_SUCCESS;
      }
      hFile = switchPrepareHandleMap.at(FileContextId);
      switchPrepareHandleMap.erase(FileContextId);
    }
    FILE_DISPOSITION_INFO fileDispositionInfo{
      TRUE,
    };
    const auto setDeleteFlagSuccess = SetFileInformationByHandle(hFile, FileDispositionInfo, &fileDispositionInfo, sizeof(fileDispositionInfo));
    CloseHandle(hFile);
    if (!setDeleteFlagSuccess) {
      DeleteFileW(realPath.c_str());
    }
  }

  NTSTATUS DGetDiskFreeSpace(PULONGLONG FreeBytesAvailable, PULONGLONG TotalNumberOfBytes, PULONGLONG TotalNumberOfFreeBytes, PDOKAN_FILE_INFO DokanFileInfo) {
    /*
    ULARGE_INTEGER uliFreeBytesAvailableToCaller;
    ULARGE_INTEGER uliTotalNumberOfBytes;
    ULARGE_INTEGER uliTotalNumberOfFreeBytes;
    if (!GetDiskFreeSpaceExW(rootPath.c_str(), &uliFreeBytesAvailableToCaller, &uliTotalNumberOfBytes, &uliTotalNumberOfFreeBytes)) {
      return NtstatusFromWin32();
    }
    if (FreeBytesAvailable) {
      *FreeBytesAvailable = uliFreeBytesAvailableToCaller.QuadPart;
    }
    if (TotalNumberOfBytes) {
      *TotalNumberOfBytes = uliTotalNumberOfBytes.QuadPart;
    }
    if (TotalNumberOfFreeBytes) {
      *TotalNumberOfFreeBytes = uliTotalNumberOfFreeBytes.QuadPart;
    }
    return STATUS_SUCCESS;
    /*/
    static_assert(offsetof(ULARGE_INTEGER, QuadPart) == 0);
    static_assert(sizeof(ULARGE_INTEGER) == sizeof(ULONGLONG));
    return NtstatusFromWin32Api(GetDiskFreeSpaceExW(rootPath.c_str(), reinterpret_cast<ULARGE_INTEGER*>(FreeBytesAvailable), reinterpret_cast<ULARGE_INTEGER*>(TotalNumberOfBytes), reinterpret_cast<ULARGE_INTEGER*>(TotalNumberOfFreeBytes)));
    //*/
  }

  NTSTATUS DGetVolumeInformation(LPWSTR VolumeNameBuffer, DWORD VolumeNameSize, LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength, LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer, DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo) {
    return NtstatusFromWin32Api(GetVolumeInformationW(rootPath.c_str(), VolumeNameBuffer, VolumeNameSize, VolumeSerialNumber, MaximumComponentLength, FileSystemFlags, FileSystemNameBuffer, FileSystemNameSize));
  }

  NTSTATUS ExportStartImpl(PORTATION_INFO* PortationInfo) {
    auto upPortation = std::make_unique<ExportPortation>(*this, PortationInfo);
    auto ptrPortation = upPortation.get();
    portationMap.emplace(ptrPortation, std::move(upPortation));
    return STATUS_SUCCESS;
  }

  NTSTATUS ExportDataImpl(PORTATION_INFO* PortationInfo) {
    auto ptrPortation = static_cast<ExportPortation*>(PortationInfo->exporterContext);
    return ptrPortation->Export(PortationInfo);
  }

  NTSTATUS ExportFinishImpl(PORTATION_INFO* PortationInfo, BOOL Success) {
    auto ptrPortation = static_cast<ExportPortation*>(PortationInfo->exporterContext);
    const auto status = ptrPortation->Finish(PortationInfo, Success);
    portationMap.erase(ptrPortation);
    return status;
  }

  NTSTATUS ImportStartImpl(PORTATION_INFO* PortationInfo) {
    auto upPortation = std::make_unique<ImportPortation>(*this, PortationInfo);
    auto ptrPortation = upPortation.get();
    portationMap.emplace(ptrPortation, std::move(upPortation));
    return STATUS_SUCCESS;
  }

  NTSTATUS ImportDataImpl(PORTATION_INFO* PortationInfo) {
    auto ptrPortation = static_cast<ImportPortation*>(PortationInfo->importerContext);
    return ptrPortation->Import(PortationInfo);
  }

  NTSTATUS ImportFinishImpl(PORTATION_INFO* PortationInfo, BOOL Success) {
    auto ptrPortation = static_cast<ImportPortation*>(PortationInfo->importerContext);
    const auto status = ptrPortation->Finish(PortationInfo, Success);
    portationMap.erase(ptrPortation);
    return status;
  }

  std::unique_ptr<SourceMountFileBase> DZwCreateFileImpl(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId) {
    return std::make_unique<SourceMountFile>(FileName, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, MaybeSwitched, FileContextId, GetRealPath(FileName));
  }

  std::unique_ptr<SourceMountFileBase> SwitchDestinationOpenImpl(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
    return std::make_unique<SourceMountFile>(FileName, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, FileContextId, GetRealPath(FileName));
  }
};



BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
  return TRUE;
}


const PLUGIN_INFO* SGetPluginInfoImpl() noexcept {
  return &gPluginInfo;
}


PLUGIN_INITCODE SInitializeImpl(const PLUGIN_INITIALIZE_INFO* InitializeInfo) noexcept {
  gPluginInitializeInfo = *InitializeInfo;
  return PLUGIN_INITCODE::Success;
}


BOOL SIsSupportedFileImpl(LPCWSTR FileName) noexcept {
  return PathIsDirectoryW(FileName);
}


std::unique_ptr<SourceMountBase> MountImpl(LPCWSTR FileName, SOURCE_CONTEXT_ID sourceContextId) {
  return std::make_unique<SourceMount>(FileName, sourceContextId);
}
