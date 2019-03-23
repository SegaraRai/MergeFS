#include <dokan/dokan.h>

#include <algorithm>
#include <cassert>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include <Windows.h>
#include <Shlwapi.h>

#include "../Util/Common.hpp"
#include "../Util/RealFs.hpp"
#include "../Util/VirtualFs.hpp"

#include "FilesystemSourceMount.hpp"
#include "FilesystemSourceMountFile.hpp"
#include "Util.hpp"

using namespace std::literals;



FilesystemSourceMount::Portation::Portation(FilesystemSourceMount& sourceMount, PORTATION_INFO* portationInfo) :
  sourceMount(sourceMount),
  filepath(portationInfo->filepath),
  fileContextId(portationInfo->fileContextId),
  empty(portationInfo->empty),
  sourceMountFile(portationInfo->fileContextId != FILE_CONTEXT_ID_NULL ? std::static_pointer_cast<FilesystemSourceMountFile>(sourceMount.GetSourceMountFileBase(portationInfo->fileContextId)) : nullptr),
  realPath(sourceMount.GetRealPath(portationInfo->filepath)),
  hFile(sourceMountFile ? sourceMountFile->GetFileHandle() : NULL),
  needClose(false)
{}


FilesystemSourceMount::Portation::~Portation() {
  // in case
  if (needClose && util::IsValidHandle(hFile)) {
    CloseHandle(hFile);
    hFile = NULL;
  }
}


NTSTATUS FilesystemSourceMount::Portation::Finish(PORTATION_INFO* portationInfo, bool success) {
  if (needClose && util::IsValidHandle(hFile)) {
    CloseHandle(hFile);
    hFile = NULL;
  }
  return STATUS_SUCCESS;
}



FilesystemSourceMount::ExportPortation::ExportPortation(FilesystemSourceMount& sourceMount, PORTATION_INFO* portationInfo) :
  Portation(sourceMount, portationInfo)
{
  lastNumberOfBytesWritten = 0;

  const DWORD fileAttributes = GetFileAttributesW(realPath.c_str());
  if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
    throw Win32Error();
  }

  directory = fileAttributes != FILE_ATTRIBUTE_NORMAL && (fileAttributes & FILE_ATTRIBUTE_DIRECTORY);

  // allocate buffer
  if (!directory) {
    buffer = std::make_unique<char[]>(BufferSize);
  }

  // create file handle if needed
  if (!util::IsValidHandle(hFile)) {
    hFile = directory
      ? CreateFileW(realPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, fileAttributes | FILE_FLAG_BACKUP_SEMANTICS, NULL)
      : CreateFileW(realPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, fileAttributes, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
      throw Win32Error();
    }

    needClose = true;
  }

  // retrieve file information
  if (!GetFileInformationByHandle(hFile, &byHandleFileInformation)) {
    if (needClose) {
      CloseHandle(hFile);
      hFile = NULL;
    }
    throw Win32Error();
  }

  portationInfo->directory = directory ? TRUE : FALSE;
  portationInfo->fileAttributes = byHandleFileInformation.dwFileAttributes;
  portationInfo->creationTime = byHandleFileInformation.ftCreationTime;
  portationInfo->lastAccessTime = byHandleFileInformation.ftLastAccessTime;
  portationInfo->lastWriteTime = byHandleFileInformation.ftLastWriteTime;
  portationInfo->fileSize.HighPart = byHandleFileInformation.nFileSizeHigh;
  portationInfo->fileSize.LowPart = byHandleFileInformation.nFileSizeLow;

  // TODO: Set Security Information
  portationInfo->securitySize = 0;
  portationInfo->securityData = nullptr;

  portationInfo->currentData = buffer.get();
  portationInfo->currentOffset.QuadPart = 0;
  portationInfo->currentSize = 0;
}


NTSTATUS FilesystemSourceMount::ExportPortation::Export(PORTATION_INFO* portationInfo) {
  if (directory) {
    return STATUS_ALREADY_COMPLETE;
  }

  portationInfo->currentOffset.QuadPart += lastNumberOfBytesWritten;

  const std::size_t size = static_cast<std::size_t>(std::min<ULONGLONG>(portationInfo->fileSize.QuadPart - portationInfo->currentOffset.QuadPart, BufferSize));
  if (size == 0) {
    return STATUS_ALREADY_COMPLETE;
  }

  if (!ReadFile(hFile, buffer.get(), static_cast<DWORD>(size), &lastNumberOfBytesWritten, NULL)) {
    lastNumberOfBytesWritten = 0;
    return NtstatusFromWin32();
  }

  portationInfo->currentData = buffer.get();
  portationInfo->currentSize = lastNumberOfBytesWritten;

  return STATUS_SUCCESS;
}


NTSTATUS FilesystemSourceMount::ExportPortation::Finish(PORTATION_INFO* portationInfo, bool success) {
  return Portation::Finish(portationInfo, success);
}



FilesystemSourceMount::ImportPortation::ImportPortation(FilesystemSourceMount& sourceMount, PORTATION_INFO* portationInfo) :
  Portation(sourceMount, portationInfo),
  directory(portationInfo->directory),
  fileAttributes(portationInfo->fileAttributes),
  creationTime(portationInfo->creationTime),
  lastAccessTime(portationInfo->lastAccessTime),
  lastWriteTime(portationInfo->lastWriteTime),
  securitySize(portationInfo->securitySize),
  fileSize(portationInfo->fileSize)
{
  if (securitySize) {
    securityData = std::make_unique<char[]>(securitySize);
    memcpy(securityData.get(), portationInfo->securityData, securitySize);
  }

  if (!util::IsValidHandle(hFile)) {
    const DWORD existingFileAttributes = GetFileAttributesW(realPath.c_str());
    if (existingFileAttributes != INVALID_FILE_ATTRIBUTES) {
      throw Win32Error();
    }

    if (directory) {
      if (!CreateDirectoryW(realPath.c_str(), NULL)) {
        throw Win32Error();
      }

      hFile = CreateFileW(realPath.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, fileAttributes | FILE_FLAG_BACKUP_SEMANTICS, NULL);
      if (hFile == INVALID_HANDLE_VALUE) {
        RemoveDirectoryW(realPath.c_str());
        throw Win32Error();
      }
    } else {
      hFile = CreateFileW(realPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_NEW, fileAttributes, NULL);
      if (hFile == INVALID_HANDLE_VALUE) {
        throw Win32Error();
      }
    }

    needClose = true;
  }

  if (!SetFileTime(hFile, &creationTime, &lastAccessTime, &lastWriteTime)) {
    const DWORD error = GetLastError();
    if (!directory) {
      FILE_DISPOSITION_INFO fileDispositionInfo{
        TRUE,
      };
      SetFileInformationByHandle(hFile, FileDispositionInfo, &fileDispositionInfo, sizeof(fileDispositionInfo));
    }
    CloseHandle(hFile);
    hFile = NULL;
    if (directory) {
      RemoveDirectoryW(realPath.c_str());
    } else {
      DeleteFileW(realPath.c_str());
    }
    throw Win32Error(error);
  }

  // TODO: Set Security Information
}


NTSTATUS FilesystemSourceMount::ImportPortation::Import(PORTATION_INFO* portationInfo) {
  if (directory) {
    return STATUS_SUCCESS;
  }

  DWORD numberOfBytesWritten;
  if (!WriteFile(hFile, portationInfo->currentData, portationInfo->currentSize, &numberOfBytesWritten, NULL)) {
    return NtstatusFromWin32();
  }

  if (numberOfBytesWritten != portationInfo->currentSize) {
    return STATUS_IO_DEVICE_ERROR;
  }

  return STATUS_SUCCESS;
}


NTSTATUS FilesystemSourceMount::ImportPortation::Finish(PORTATION_INFO* portationInfo, bool success) {
  if (util::IsValidHandle(hFile)) {
    if (!success) {
      if (!directory) {
        FILE_DISPOSITION_INFO fileDispositionInfo{
          TRUE,
        };
        SetFileInformationByHandle(hFile, FileDispositionInfo, &fileDispositionInfo, sizeof(fileDispositionInfo));
      }
    }
    if (needClose) {
      CloseHandle(hFile);
      hFile = NULL;
    }
  }
  if (!success && needClose) {
    if (directory) {
      RemoveDirectoryW(realPath.c_str());
    } else {
      DeleteFileW(realPath.c_str());
    }
  }
  return Portation::Finish(portationInfo, success);
}



std::wstring FilesystemSourceMount::GetRealPathPrefix(const std::wstring& filepath) {
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


std::wstring FilesystemSourceMount::GetRootPath(const std::wstring& realPathPrefix) {
  auto tempBuffer = std::make_unique<wchar_t[]>(realPathPrefix.size() + 1);
  memcpy(tempBuffer.get(), realPathPrefix.c_str(), (realPathPrefix.size() + 1) * sizeof(wchar_t));
  PathStripToRootW(tempBuffer.get());
  return std::wstring(tempBuffer.get());
}


FilesystemSourceMount::FilesystemSourceMount(const PLUGIN_INITIALIZE_MOUNT_INFO* InitializeMountInfo, SOURCE_CONTEXT_ID sourceContextId) :
  SourceMountBase(InitializeMountInfo, sourceContextId),
  subMutex(),
  switchPrepareHandleMap(),
  portationMap(),
  realPathPrefix(GetRealPathPrefix(filename)),
  rootPath(GetRootPath(realPathPrefix))
{
  constexpr std::size_t BufferSize = MAX_PATH + 1;

  rootDirectoryFileHandle = CreateFileW(realPathPrefix.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_DIRECTORY | FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (!util::IsValidHandle(rootDirectoryFileHandle)) {
    throw Win32Error();
  }

  if (!GetFileInformationByHandle(rootDirectoryFileHandle, &rootDirectoryInfo)) {
    throw Win32Error();
  }

  auto baseVolumeNameBuffer = std::make_unique<wchar_t[]>(BufferSize);
  auto baseFileSystemNameBuffer = std::make_unique<wchar_t[]>(BufferSize);
  if (!GetVolumeInformationByHandleW(rootDirectoryFileHandle, baseVolumeNameBuffer.get(), BufferSize, &baseVolumeSerialNumber, &baseMaximumComponentLength, &baseFileSystemFlags, baseFileSystemNameBuffer.get(), BufferSize)) {
    throw Win32Error();
  }
  baseVolumeName = std::wstring(baseVolumeNameBuffer.get());
  baseFileSystemName = std::wstring(baseFileSystemNameBuffer.get());

  volumeName = util::rfs::GetBaseName(filename).substr(0, MAX_PATH);
  volumeSerialNumber = baseVolumeSerialNumber ^ rootDirectoryInfo.nFileIndexHigh ^ rootDirectoryInfo.nFileIndexLow;
  maximumComponentLength = static_cast<DWORD>(baseMaximumComponentLength - realPathPrefix.size() - 1);
  fileSystemFlags = baseFileSystemFlags & ~static_cast<DWORD>(FILE_SUPPORTS_TRANSACTIONS | FILE_SUPPORTS_HARD_LINKS | FILE_SUPPORTS_REPARSE_POINTS | FILE_SUPPORTS_USN_JOURNAL | FILE_VOLUME_QUOTAS);
  fileSystemName = baseFileSystemName;
}


FilesystemSourceMount::~FilesystemSourceMount() {
  if (util::IsValidHandle(rootDirectoryFileHandle)) {
    CloseHandle(rootDirectoryFileHandle);
    rootDirectoryFileHandle = NULL;
  }
}


std::wstring FilesystemSourceMount::GetRealPath(LPCWSTR filepath) {
  assert(filepath && filepath[0] == L'\\');
  return util::vfs::IsRootDirectory(filepath) ? realPathPrefix : realPathPrefix + std::wstring(filepath);
}


BOOL FilesystemSourceMount::GetSourceInfo(SOURCE_INFO* sourceInfo) {
  if (sourceInfo) {
    *sourceInfo = {
      TRUE,
    };
  }
  return TRUE;
}


NTSTATUS FilesystemSourceMount::GetFileInfo(LPCWSTR FileName, WIN32_FILE_ATTRIBUTE_DATA* Win32FileAttributeData) {
  if (!Win32FileAttributeData) {
    return STATUS_SUCCESS;
  }
  const std::wstring realPath = GetRealPath(FileName);
  return NtstatusFromWin32Api(GetFileAttributesExW(realPath.c_str(), GetFileExInfoStandard, Win32FileAttributeData));
}


NTSTATUS FilesystemSourceMount::GetDirectoryInfo(LPCWSTR FileName) {
  const std::wstring realPath = GetRealPath(FileName);
  const auto csRealPath = realPath.c_str();
  const DWORD fileAttributes = GetFileAttributesW(csRealPath);
  if (fileAttributes == INVALID_FILE_ATTRIBUTES) {
    //return STATUS_OBJECT_NAME_NOT_FOUND;
    return NtstatusFromWin32();
  }
  if (fileAttributes == FILE_ATTRIBUTE_NORMAL || !(fileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
    return STATUS_NOT_A_DIRECTORY;
  }
  return PathIsDirectoryEmptyW(csRealPath) ? STATUS_SUCCESS : STATUS_DIRECTORY_NOT_EMPTY;
}


NTSTATUS FilesystemSourceMount::RemoveFile(LPCWSTR FileName) {
  const std::wstring realPath = GetRealPath(FileName);
  return NtstatusFromWin32Api(DeleteFileW(realPath.c_str()));
}


NTSTATUS FilesystemSourceMount::ListFiles(LPCWSTR FileName, PListFilesCallback Callback, CALLBACK_CONTEXT CallbackContext) {
  const std::wstring realPath = GetRealPath(FileName);
  const std::wstring filter = realPath + L"\\*"s;
  WIN32_FIND_DATAW win32FindData;
  HANDLE hFind = FindFirstFileW(filter.c_str(), &win32FindData);
  if (hFind == INVALID_HANDLE_VALUE) {
    return NtstatusFromWin32();
  }
  do {
    Callback(&win32FindData, CallbackContext);
  } while (FindNextFileW(hFind, &win32FindData));
  const DWORD error = GetLastError();
  FindClose(hFind);
  return error == ERROR_SUCCESS || error == ERROR_NO_MORE_FILES ? STATUS_SUCCESS : NtstatusFromWin32(error);
}


NTSTATUS FilesystemSourceMount::ListStreams(LPCWSTR FileName, PListStreamsCallback Callback, CALLBACK_CONTEXT CallbackContext) {
  const std::wstring realPath = GetRealPath(FileName);
  WIN32_FIND_STREAM_DATA win32FindStreamData;
  HANDLE hFind = FindFirstStreamW(realPath.c_str(), FindStreamInfoStandard, &win32FindStreamData, 0);
  if (hFind == INVALID_HANDLE_VALUE) {
    return NtstatusFromWin32();
  }
  do {
    Callback(&win32FindStreamData, CallbackContext);
  } while (FindNextStreamW(hFind, &win32FindStreamData));
  const DWORD error = GetLastError();
  FindClose(hFind);
  return error == ERROR_SUCCESS || error == ERROR_HANDLE_EOF ? STATUS_SUCCESS : NtstatusFromWin32(error);
}


NTSTATUS FilesystemSourceMount::SwitchDestinationPrepareImpl(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  /*
  const std::wstring realPath = GetRealPath(FileName);

  ACCESS_MASK userDesiredAccess;
  DWORD fileAttributesAndFlags;
  DWORD creationDisposition;
  GetPluginInitializeInfo().dokanFuncs.DokanMapKernelToUserCreateFileFlags(DesiredAccess, FileAttributes, CreateOptions, CreateDisposition, &userDesiredAccess, &fileAttributesAndFlags, &creationDisposition);

  SECURITY_ATTRIBUTES securityAttributes{
    sizeof(securityAttributes),
    SecurityContext->AccessState.SecurityDescriptor,
    FALSE,
  };

  HANDLE hFile = CreateFileW(realPath.c_str(), userDesiredAccess, ShareAccess, &securityAttributes, CREATE_NEW, fileAttributesAndFlags, NULL);
  if (hFile == INVALID_HANDLE_VALUE) {
    return STATUS_SUCCESS;
    //return NtstatusFromWin32();
  }

  std::lock_guard lock(subMutex);
  switchPrepareHandleMap.emplace(FileContextId, hFile);

  return STATUS_SUCCESS;
  /*/
  // TODO:
  // CopyFileToTopSourceRにおいて、既にTopSourceにファイルが存在しているとどのソースから持ってくれば良いか分からないためエラーにしている
  // この時点でファイルを確保しておくというアイディアは活かしたいが、上記への対応を思いつくまで保留
  // なお、対応としてはLibMergeFS側で処理するものとこちら側で処理する（Prepare中のものは無いものとして扱う）ものが考えられるが、どちらにも難点がある
  return STATUS_SUCCESS;
  //*/
}


NTSTATUS FilesystemSourceMount::SwitchDestinationCleanupImpl(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  return STATUS_SUCCESS;
}


NTSTATUS FilesystemSourceMount::SwitchDestinationCloseImpl(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  /*
  const std::wstring realPath = GetRealPath(FileName);

  HANDLE hFile = NULL;
  {
    std::lock_guard lock(subMutex);
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

  return STATUS_SUCCESS;
  /*/
  return STATUS_SUCCESS;
  //*/
}


NTSTATUS FilesystemSourceMount::DGetDiskFreeSpace(PULONGLONG FreeBytesAvailable, PULONGLONG TotalNumberOfBytes, PULONGLONG TotalNumberOfFreeBytes, PDOKAN_FILE_INFO DokanFileInfo) {
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


NTSTATUS FilesystemSourceMount::DGetVolumeInformation(LPWSTR VolumeNameBuffer, DWORD VolumeNameSize, LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength, LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer, DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo) {
  if (VolumeNameBuffer) {
    if (VolumeNameSize < volumeName.size() + 1) {
      return STATUS_BUFFER_TOO_SMALL;
    }
    std::memcpy(VolumeNameBuffer, volumeName.c_str(), (volumeName.size() + 1) * sizeof(wchar_t));
  }
  if (VolumeSerialNumber) {
    *VolumeSerialNumber = volumeSerialNumber;
  }
  if (MaximumComponentLength) {
    *MaximumComponentLength = maximumComponentLength;
  }
  if (FileSystemFlags) {
    *FileSystemFlags = fileSystemFlags;
  }
  if (FileSystemNameBuffer) {
    if (FileSystemNameSize < fileSystemName.size() + 1) {
      return STATUS_BUFFER_TOO_SMALL;
    }
    std::memcpy(FileSystemNameBuffer, fileSystemName.c_str(), (fileSystemName.size() + 1) * sizeof(wchar_t));
  }
  return STATUS_SUCCESS;
}


NTSTATUS FilesystemSourceMount::ExportStartImpl(PORTATION_INFO* PortationInfo) {
  auto upPortation = std::make_unique<ExportPortation>(*this, PortationInfo);
  auto ptrPortation = upPortation.get();
  portationMap.emplace(ptrPortation, std::move(upPortation));
  PortationInfo->exporterContext = ptrPortation;
  return STATUS_SUCCESS;
}


NTSTATUS FilesystemSourceMount::ExportDataImpl(PORTATION_INFO* PortationInfo) {
  auto ptrPortation = static_cast<ExportPortation*>(PortationInfo->exporterContext);
  return ptrPortation->Export(PortationInfo);
}


NTSTATUS FilesystemSourceMount::ExportFinishImpl(PORTATION_INFO* PortationInfo, BOOL Success) {
  auto ptrPortation = static_cast<ExportPortation*>(PortationInfo->exporterContext);
  const auto status = ptrPortation->Finish(PortationInfo, Success);
  portationMap.erase(ptrPortation);
  return status;
}


NTSTATUS FilesystemSourceMount::ImportStartImpl(PORTATION_INFO* PortationInfo) {
  auto upPortation = std::make_unique<ImportPortation>(*this, PortationInfo);
  auto ptrPortation = upPortation.get();
  portationMap.emplace(ptrPortation, std::move(upPortation));
  PortationInfo->importerContext = ptrPortation;
  return STATUS_SUCCESS;
}


NTSTATUS FilesystemSourceMount::ImportDataImpl(PORTATION_INFO* PortationInfo) {
  auto ptrPortation = static_cast<ImportPortation*>(PortationInfo->importerContext);
  return ptrPortation->Import(PortationInfo);
}


NTSTATUS FilesystemSourceMount::ImportFinishImpl(PORTATION_INFO* PortationInfo, BOOL Success) {
  auto ptrPortation = static_cast<ImportPortation*>(PortationInfo->importerContext);
  const auto status = ptrPortation->Finish(PortationInfo, Success);
  portationMap.erase(ptrPortation);
  return status;
}


std::unique_ptr<SourceMountFileBase> FilesystemSourceMount::DZwCreateFileImpl(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId) {
  return std::make_unique<FilesystemSourceMountFile>(*this, FileName, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, MaybeSwitched, FileContextId);
}


std::unique_ptr<SourceMountFileBase> FilesystemSourceMount::SwitchDestinationOpenImpl(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  return std::make_unique<FilesystemSourceMountFile>(*this, FileName, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, FileContextId);
}


HANDLE FilesystemSourceMount::TransferSwitchDestinationHandle(FILE_CONTEXT_ID fileContextId) {
  /*
  std::lock_guard lock(subMutex);
  if (!switchPrepareHandleMap.count(fileContextId)) {
    return NULL;
  }
  HANDLE hFile = switchPrepareHandleMap.at(fileContextId);
  switchPrepareHandleMap.erase(fileContextId);
  return hFile;
  /*/
  return NULL;
  //*/
}
