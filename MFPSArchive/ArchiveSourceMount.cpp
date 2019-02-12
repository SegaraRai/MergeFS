#include <dokan/dokan.h>

#include <7z/CPP/Common/Common.h>

#include "ArchiveSourceMount.hpp"
#include "ArchiveSourceMountFile.hpp"
#include "Util.hpp"
#include "NanaZ/COMError.hpp"
#include "NanaZ/FileStream.hpp"
#include "NanaZ/NanaZ.hpp"

#include <algorithm>
#include <cassert>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include <Windows.h>
#include <winrt/base.h>

using namespace std::literals;



namespace {
  const std::wstring DefaultFilepath = L"Content"s;
  constexpr UInt64 MaxCheckStartPosition = 1 << 23;
  constexpr DirectoryTree::OnExistingMode OnExistingMode = DirectoryTree::OnExistingMode::RenameNewOne;
  constexpr DirectoryTree::UseOnMemoryExtractionMode UseOnMemoryExtraction = DirectoryTree::UseOnMemoryExtractionMode::Auto;
}



ArchiveSourceMount::ExportPortation::ExportPortation(ArchiveSourceMount& sourceMount, PORTATION_INFO* portationInfo) :
  sourceMount(sourceMount),
  filepath(portationInfo->filepath),
  fileContextId(portationInfo->fileContextId),
  empty(portationInfo->empty),
  sourceMountFile(portationInfo->fileContextId != FILE_CONTEXT_ID_NULL ? dynamic_cast<ArchiveSourceMountFile*>(&sourceMount.GetSourceMountFileBase(portationInfo->fileContextId)) : nullptr),
  realPath(sourceMount.GetRealPath(portationInfo->filepath)),
  ptrDirectoryTree(sourceMount.GetDirectoryTreeR(realPath)),
  lastNumberOfBytesWritten(0)
{
  if (!ptrDirectoryTree) {
    throw NtstatusError(sourceMount.ReturnPathOrNameNotFoundErrorR(realPath));
  }

  directory = ptrDirectoryTree->type != DirectoryTree::Type::File;

  // allocate buffer
  if (!directory) {
    buffer = std::make_unique<char[]>(BufferSize);
  }

  portationInfo->directory = directory ? TRUE : FALSE;
  portationInfo->fileAttributes = DirectoryTree::FilterArchiveFileAttributes(*ptrDirectoryTree);
  portationInfo->creationTime = ptrDirectoryTree->creationTime;
  portationInfo->lastAccessTime = ptrDirectoryTree->lastAccessTime;
  portationInfo->lastWriteTime = ptrDirectoryTree->lastWriteTime;
  portationInfo->fileSize.QuadPart = ptrDirectoryTree->fileSize;

  // TODO: Set Security Information
  portationInfo->securitySize = 0;
  portationInfo->securityData = nullptr;

  portationInfo->currentData = buffer.get();
  portationInfo->currentOffset.QuadPart = 0;
  portationInfo->currentSize = 0;
}


NTSTATUS ArchiveSourceMount::ExportPortation::Export(PORTATION_INFO* portationInfo) {
  if (directory) {
    return STATUS_ALREADY_COMPLETE;
  }

  portationInfo->currentOffset.QuadPart += lastNumberOfBytesWritten;

  const std::size_t size = static_cast<std::size_t>(std::min<ULONGLONG>(portationInfo->fileSize.QuadPart - portationInfo->currentOffset.QuadPart, BufferSize));
  if (size == 0) {
    return STATUS_ALREADY_COMPLETE;
  }

  {
    std::lock_guard lock(*ptrDirectoryTree->streamMutex);
    COMError::CheckHRESULT(ptrDirectoryTree->inStream->Seek(portationInfo->currentOffset.QuadPart, STREAM_SEEK_SET, nullptr));
    COMError::CheckHRESULT(ptrDirectoryTree->inStream->Read(buffer.get(), static_cast<UInt32>(size), &lastNumberOfBytesWritten));
  }

  portationInfo->currentData = buffer.get();
  portationInfo->currentSize = lastNumberOfBytesWritten;

  return STATUS_SUCCESS;
}


NTSTATUS ArchiveSourceMount::ExportPortation::Finish(PORTATION_INFO* portationInfo, bool success) {
  return STATUS_SUCCESS;
}



ArchiveSourceMount::ArchiveSourceMount(NanaZ& nanaZ, const PLUGIN_INITIALIZE_MOUNT_INFO* InitializeMountInfo, SOURCE_CONTEXT_ID sourceContextId) :
  ReadonlySourceMountBase(InitializeMountInfo, sourceContextId),
  nanaZ(nanaZ),
  subMutex(),
  portationMap(),
  archiveFileHandle(NULL)
{
  constexpr std::size_t BufferSize = MAX_PATH + 1;

  try {
    absolutePath = ToAbsoluteFilepath(InitializeMountInfo->FileName);
    const auto archiveFilepathN = FindRootFilepath(absolutePath);
    if (!archiveFilepathN) {
      throw std::runtime_error(u8"archive file not found");
    }

    archiveFilepath = archiveFilepathN.value();
    pathPrefix = archiveFilepath.size() == absolutePath.size() ? L""s : absolutePath.substr(archiveFilepath.size() + 1) + L"\\"s;

    archiveFileHandle = CreateFileW(archiveFilepath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (!IsValidHandle(archiveFileHandle)) {
      throw Win32Error();
    }

    if (!GetFileInformationByHandle(archiveFileHandle, &archiveFileInfo)) {
      CloseHandle(archiveFileHandle);
      throw Win32Error();
    }

    auto baseVolumeNameBuffer = std::make_unique<wchar_t[]>(BufferSize);
    auto baseFileSystemNameBuffer = std::make_unique<wchar_t[]>(BufferSize);
    if (!GetVolumeInformationByHandleW(archiveFileHandle, baseVolumeNameBuffer.get(), BufferSize, &baseVolumeSerialNumber, &baseMaximumComponentLength, &baseFileSystemFlags, baseFileSystemNameBuffer.get(), BufferSize)) {
      throw Win32Error();
    }
    baseVolumeName = std::wstring(baseVolumeNameBuffer.get());
    baseFileSystemName = std::wstring(baseFileSystemNameBuffer.get());

    const auto lastSlashPos = archiveFilepath.find_last_of(L'\\');
    volumeName = (lastSlashPos == std::wstring::npos ? archiveFilepath : archiveFilepath.substr(lastSlashPos + 1)).substr(0, MAX_PATH);
    volumeSerialNumber = baseVolumeSerialNumber ^ archiveFileInfo.nFileIndexHigh ^ archiveFileInfo.nFileIndexLow;

    // TODO: move definition
    maximumComponentLength = baseMaximumComponentLength;
    fileSystemFlags = FILE_CASE_PRESERVED_NAMES | FILE_UNICODE_ON_DISK;
    fileSystemName = L"ARCHIVE"s;

    // open archive
    winrt::com_ptr<InFileStream> inFileStream;
    inFileStream.attach(new InFileStream(archiveFileHandle));
    archiveN.emplace(nanaZ, inFileStream, archiveFileInfo, DefaultFilepath, caseSensitive, MaxCheckStartPosition, OnExistingMode, UseOnMemoryExtraction, [](const std::wstring& originalFilepath, std::size_t count) -> std::optional<std::pair<std::wstring, bool>> {
      const auto lastDelimiterPos = originalFilepath.find_last_of(Archive::DirectorySeparatorFromLibrary);
      const auto parentDirectoryPath = lastDelimiterPos == std::wstring::npos ? L""s : originalFilepath.substr(0, lastDelimiterPos + 1);
      const auto baseFilename = lastDelimiterPos == std::wstring::npos ? originalFilepath : originalFilepath.substr(lastDelimiterPos + 1);
      std::wstring newFilepath = parentDirectoryPath + L"["s + baseFilename + L"]"s;;
      if (count) {
        newFilepath += L"."s + std::to_wstring(count + 1);
      }
      return std::make_optional<std::pair<std::wstring, bool>>(newFilepath, false);
    }, nullptr);
  } catch (...) {
    if (IsValidHandle(archiveFileHandle)) {
      CloseHandle(archiveFileHandle);
      archiveFileHandle = NULL;
    }
    throw;
  }
}


ArchiveSourceMount::~ArchiveSourceMount() {
  archiveN = std::nullopt;
  if (IsValidHandle(archiveFileHandle)) {
    CloseHandle(archiveFileHandle);
    archiveFileHandle = NULL;
  }
}


std::wstring ArchiveSourceMount::GetRealPath(LPCWSTR filepath) const {
  assert(filepath && filepath[0] == L'\\');
  return IsRootDirectory(filepath) ? pathPrefix : pathPrefix + std::wstring(filepath + 1);
}


NTSTATUS ArchiveSourceMount::ReturnPathOrNameNotFoundErrorR(std::wstring_view realPath) const {
  auto& archive = this->archiveN.value();
  return archive.Exists(GetParentPath(realPath)) ? STATUS_OBJECT_NAME_NOT_FOUND : STATUS_OBJECT_PATH_NOT_FOUND;
}


NTSTATUS ArchiveSourceMount::ReturnPathOrNameNotFoundError(LPCWSTR filepath) const {
  return ReturnPathOrNameNotFoundErrorR(GetRealPath(filepath));
}


const DirectoryTree* ArchiveSourceMount::GetDirectoryTreeR(std::wstring_view realPath) const {
  auto& archive = this->archiveN.value();
  return archive.Get(realPath);
}


const DirectoryTree* ArchiveSourceMount::GetDirectoryTree(LPCWSTR filepath) const {
  return GetDirectoryTreeR(GetRealPath(filepath));
}


DWORD ArchiveSourceMount::GetVolumeSerialNumber() const {
  return volumeSerialNumber;
}


BOOL ArchiveSourceMount::GetSourceInfo(SOURCE_INFO* sourceInfo) {
  if (sourceInfo) {
    *sourceInfo = {
      FALSE,
    };
  }
  return TRUE;
}


NTSTATUS ArchiveSourceMount::GetFileInfo(LPCWSTR FileName, WIN32_FILE_ATTRIBUTE_DATA* Win32FileAttributeData) {
  if (!Win32FileAttributeData) {
    return STATUS_SUCCESS;
  }
  auto& archive = this->archiveN.value();
  const auto realPath = GetRealPath(FileName);
  const auto ptrDirectoryTree = archive.Get(realPath);
  if (!ptrDirectoryTree) {
    return ReturnPathOrNameNotFoundErrorR(realPath);
  }
  Win32FileAttributeData->dwFileAttributes = DirectoryTree::FilterArchiveFileAttributes(*ptrDirectoryTree);
  Win32FileAttributeData->ftCreationTime = ptrDirectoryTree->creationTime;
  Win32FileAttributeData->ftLastAccessTime = ptrDirectoryTree->lastAccessTime;
  Win32FileAttributeData->ftLastWriteTime = ptrDirectoryTree->lastWriteTime;
  Win32FileAttributeData->nFileSizeHigh = (ptrDirectoryTree->fileSize >> 32) & 0xFFFFFFFF;
  Win32FileAttributeData->nFileSizeLow = ptrDirectoryTree->fileSize & 0xFFFFFFFF;
  return STATUS_SUCCESS;
}


NTSTATUS ArchiveSourceMount::GetDirectoryInfo(LPCWSTR FileName) {
  auto& archive = this->archiveN.value();
  const auto realPath = GetRealPath(FileName);
  const auto ptrDirectoryTree = archive.Get(realPath);
  if (!ptrDirectoryTree) {
    return ReturnPathOrNameNotFoundErrorR(realPath);
  }
  if (ptrDirectoryTree->type == DirectoryTree::Type::File) {
    return STATUS_NOT_A_DIRECTORY;
  }
  return ptrDirectoryTree->children.empty() ? STATUS_SUCCESS : STATUS_DIRECTORY_NOT_EMPTY;
}


NTSTATUS ArchiveSourceMount::ListFiles(LPCWSTR FileName, PListFilesCallback Callback, CALLBACK_CONTEXT CallbackContext) {
  auto& archive = this->archiveN.value();
  const auto realPath = GetRealPath(FileName);
  const auto ptrDirectoryTree = archive.Get(realPath);
  if (!ptrDirectoryTree) {
    return ReturnPathOrNameNotFoundErrorR(realPath);
  }
  if (ptrDirectoryTree->type == DirectoryTree::Type::File) {
    return STATUS_NOT_A_DIRECTORY;
  }
  for (auto& [key, childDirectoryTree] : ptrDirectoryTree->children) {
    WIN32_FIND_DATAW win32FindDataW{
      DirectoryTree::FilterArchiveFileAttributes(childDirectoryTree),
      childDirectoryTree.creationTime,
      childDirectoryTree.lastAccessTime,
      childDirectoryTree.lastWriteTime,
      (childDirectoryTree.fileSize >> 32) & 0xFFFFFFFF,
      childDirectoryTree.fileSize & 0xFFFFFFFF,
      0,
      0,
    };
    std::size_t copyLength = std::min<std::size_t>(key.size(), MAX_PATH - 1);
    std::memcpy(win32FindDataW.cFileName, key.c_str(), copyLength * sizeof(wchar_t));
    win32FindDataW.cFileName[copyLength] = L'\0';
    Callback(&win32FindDataW, CallbackContext);
  }
  return STATUS_SUCCESS;
}


NTSTATUS ArchiveSourceMount::ListStreams(LPCWSTR FileName, PListStreamsCallback Callback, CALLBACK_CONTEXT CallbackContext) {
  // TODO?
  return STATUS_SUCCESS;
}


NTSTATUS ArchiveSourceMount::DGetDiskFreeSpace(PULONGLONG FreeBytesAvailable, PULONGLONG TotalNumberOfBytes, PULONGLONG TotalNumberOfFreeBytes, PDOKAN_FILE_INFO DokanFileInfo) {
  if (FreeBytesAvailable) {
    *FreeBytesAvailable = 0;
  }
  if (TotalNumberOfBytes) {
    // sloppy
    *TotalNumberOfBytes = (static_cast<ULONGLONG>(archiveFileInfo.nFileSizeHigh) << 32) | archiveFileInfo.nFileIndexLow;
  }
  if (TotalNumberOfFreeBytes) {
    *TotalNumberOfFreeBytes = 0;
  }
  return STATUS_SUCCESS;
}


NTSTATUS ArchiveSourceMount::DGetVolumeInformation(LPWSTR VolumeNameBuffer, DWORD VolumeNameSize, LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength, LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer, DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo) {
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


NTSTATUS ArchiveSourceMount::ExportStartImpl(PORTATION_INFO* PortationInfo) {
  auto upPortation = std::make_unique<ExportPortation>(*this, PortationInfo);
  auto ptrPortation = upPortation.get();
  portationMap.emplace(ptrPortation, std::move(upPortation));
  PortationInfo->exporterContext = ptrPortation;
  return STATUS_SUCCESS;
}


NTSTATUS ArchiveSourceMount::ExportDataImpl(PORTATION_INFO* PortationInfo) {
  auto ptrPortation = static_cast<ExportPortation*>(PortationInfo->exporterContext);
  return ptrPortation->Export(PortationInfo);
}


NTSTATUS ArchiveSourceMount::ExportFinishImpl(PORTATION_INFO* PortationInfo, BOOL Success) {
  auto ptrPortation = static_cast<ExportPortation*>(PortationInfo->exporterContext);
  const auto status = ptrPortation->Finish(PortationInfo, Success);
  portationMap.erase(ptrPortation);
  return status;
}


std::unique_ptr<SourceMountFileBase> ArchiveSourceMount::DZwCreateFileImpl(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId) {
  return std::make_unique<ArchiveSourceMountFile>(*this, FileName, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, MaybeSwitched, FileContextId);
}
