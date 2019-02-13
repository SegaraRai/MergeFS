#include <dokan/dokan.h>

#include "CueSourceMount.hpp"
#include "CueSourceMountFile.hpp"
#include "AudioSourceToSourceWAV.hpp"
#include "GeneratePlaylistCue.hpp"
#include "GeneratePlaylistM3U8.hpp"
#include "MemorySource.hpp"
#include "Util.hpp"

#include "../SDK/CaseSensitivity.hpp"

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
  constexpr CueAudioLoader::ExtractToMemory ExtractToMemory = CueAudioLoader::ExtractToMemory::Never;



  std::wstring GetTwoDigit(unsigned int number) {
    if (number < 10) {
      return L"0"s + std::to_wstring(number);
    }
    return std::to_wstring(number);
  }


  void Insert(DirectoryTree& directoryTree, std::wstring_view filepath, std::shared_ptr<Source> source, ULONGLONG& fileIndexCount) {
    if (filepath.empty()) {
      throw std::runtime_error(u8"invalid function call");
    }

    const auto firstDelimiterPos = filepath.find_first_of(L'\\');
    const std::wstring rootDirectoryName(filepath.substr(0, firstDelimiterPos));

    if (firstDelimiterPos == std::wstring_view::npos) {
      // leaf
      assert(!directoryTree.children.count(rootDirectoryName));
      directoryTree.children.emplace(rootDirectoryName, DirectoryTree{
        directoryTree.caseSensitive,
        false,
        fileIndexCount++,
        source,
        {0, CaseSensitivity::CiHash(directoryTree.caseSensitive), CaseSensitivity::CiEqualTo(directoryTree.caseSensitive)},
      });
      return;
    }

    if (!directoryTree.children.count(rootDirectoryName)) {
      directoryTree.children.emplace(rootDirectoryName, DirectoryTree{
        directoryTree.caseSensitive,
        true,
        fileIndexCount++,
        nullptr,
        {0, CaseSensitivity::CiHash(directoryTree.caseSensitive), CaseSensitivity::CiEqualTo(directoryTree.caseSensitive)},
      });
    }

    auto& child = directoryTree.children.at(rootDirectoryName);
    Insert(child, filepath.substr(firstDelimiterPos + 1), source, fileIndexCount);
  }
}



CueSourceMount::ExportPortation::ExportPortation(CueSourceMount& sourceMount, PORTATION_INFO* portationInfo) :
  sourceMount(sourceMount),
  filepath(portationInfo->filepath),
  fileContextId(portationInfo->fileContextId),
  empty(portationInfo->empty),
  sourceMountFile(portationInfo->fileContextId != FILE_CONTEXT_ID_NULL ? dynamic_cast<CueSourceMountFile*>(&sourceMount.GetSourceMountFileBase(portationInfo->fileContextId)) : nullptr),
  ptrDirectoryTree(sourceMount.GetDirectoryTree(portationInfo->filepath)),
  lastNumberOfBytesWritten(0)
{
  if (!ptrDirectoryTree) {
    throw NtstatusError(sourceMount.ReturnPathOrNameNotFoundError(portationInfo->filepath));
  }

  
  // allocate buffer
  if (!ptrDirectoryTree->directory) {
    buffer = std::make_unique<std::byte[]>(BufferSize);
  }

  portationInfo->directory = ptrDirectoryTree->directory ? TRUE : FALSE;
  portationInfo->fileAttributes = ptrDirectoryTree->directory ? DirectoryTree::DirectoryFileAttributes : DirectoryTree::FileFileAttributes;
  portationInfo->creationTime = sourceMount.cueFileInfo.ftCreationTime;
  portationInfo->lastAccessTime = sourceMount.cueFileInfo.ftLastAccessTime;
  portationInfo->lastWriteTime = sourceMount.cueFileInfo.ftLastWriteTime;
  portationInfo->fileSize.QuadPart = ptrDirectoryTree->source ? ptrDirectoryTree->source->GetSize() : 0;

  // TODO: Set Security Information
  portationInfo->securitySize = 0;
  portationInfo->securityData = nullptr;

  portationInfo->currentData = reinterpret_cast<const char*>(buffer.get());
  portationInfo->currentOffset.QuadPart = 0;
  portationInfo->currentSize = 0;
}


NTSTATUS CueSourceMount::ExportPortation::Export(PORTATION_INFO* portationInfo) {
  if (directory) {
    return STATUS_ALREADY_COMPLETE;
  }

  portationInfo->currentOffset.QuadPart += lastNumberOfBytesWritten;

  const std::size_t size = static_cast<std::size_t>(std::min<ULONGLONG>(portationInfo->fileSize.QuadPart - portationInfo->currentOffset.QuadPart, BufferSize));
  if (size == 0) {
    return STATUS_ALREADY_COMPLETE;
  }

  if (const auto status = ptrDirectoryTree->source->Read(portationInfo->currentOffset.QuadPart, buffer.get(), size, &lastNumberOfBytesWritten); status != STATUS_SUCCESS) {
    return status;
  }

  portationInfo->currentData = reinterpret_cast<const char*>(buffer.get());
  portationInfo->currentSize = static_cast<DWORD>(lastNumberOfBytesWritten);

  return STATUS_SUCCESS;
}


NTSTATUS CueSourceMount::ExportPortation::Finish(PORTATION_INFO* portationInfo, bool success) {
  return STATUS_SUCCESS;
}



CueSourceMount::CueSourceMount(const PLUGIN_INITIALIZE_MOUNT_INFO* InitializeMountInfo, SOURCE_CONTEXT_ID sourceContextId) :
  ReadonlySourceMountBase(InitializeMountInfo, sourceContextId),
  subMutex(),
  portationMap(),
  cueAudioLoader(InitializeMountInfo->FileName, ExtractToMemory),
  directoryTree{
    caseSensitive,
    true,
    1,
    nullptr,
    {0, CaseSensitivity::CiHash(caseSensitive), CaseSensitivity::CiEqualTo(caseSensitive)},
  }
{
  constexpr std::size_t BufferSize = MAX_PATH + 1;

  HANDLE cueFileHandle = NULL;

  try {
    const auto cueFilePath = ToAbsoluteFilepath(InitializeMountInfo->FileName);

    cueFileHandle = CreateFileW(cueFilePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (!IsValidHandle(cueFileHandle)) {
      throw Win32Error();
    }

    if (!GetFileInformationByHandle(cueFileHandle, &cueFileInfo)) {
      throw Win32Error();
    }

    auto baseVolumeNameBuffer = std::make_unique<wchar_t[]>(BufferSize);
    auto baseFileSystemNameBuffer = std::make_unique<wchar_t[]>(BufferSize);
    if (!GetVolumeInformationByHandleW(cueFileHandle, baseVolumeNameBuffer.get(), BufferSize, &baseVolumeSerialNumber, &baseMaximumComponentLength, &baseFileSystemFlags, baseFileSystemNameBuffer.get(), BufferSize)) {
      throw Win32Error();
    }
    baseVolumeName = std::wstring(baseVolumeNameBuffer.get());
    baseFileSystemName = std::wstring(baseFileSystemNameBuffer.get());

    const auto lastSlashPos = cueFilePath.find_last_of(L'\\');
    const auto cueFilename = (lastSlashPos == std::wstring::npos ? cueFilePath : cueFilePath.substr(lastSlashPos + 1)).substr(0, MAX_PATH);
    const auto lastDotPos = cueFilename.find_last_of(L'.');
    const auto cueBasename = cueFilename.substr(0, lastDotPos);

    volumeName = cueFilename;
    volumeSerialNumber = baseVolumeSerialNumber ^ cueFileInfo.nFileIndexHigh ^ cueFileInfo.nFileIndexLow;

    // TODO: move definition
    maximumComponentLength = baseMaximumComponentLength;
    fileSystemFlags = FILE_CASE_PRESERVED_NAMES | FILE_UNICODE_ON_DISK;
    fileSystemName = L"CUESHEET"s;


    // add files
    const auto firstTrackNumber = cueAudioLoader.GetFirstTrackNumber();
    const auto lastTrackNumber = cueAudioLoader.GetLastTrackNumber();

    ULONGLONG fileIndexCount = 2;

    {
      const auto topAudioFilename = cueBasename + L".wav"s;

      Insert(directoryTree, topAudioFilename, std::make_shared<AudioSourceToSourceWAV>(cueAudioLoader.GetFullAudioSource()), fileIndexCount);

      {
        const auto data = GeneratePlaylistCue(cueAudioLoader.GetCueSheet(), topAudioFilename);
        Insert(directoryTree, cueBasename + L".cue"s, std::make_shared<MemorySource>(data.data(), data.size()), fileIndexCount);
      }
    }


    for (const auto& offsetNumber : cueAudioLoader.GetOffsetNumberSet()) {
      // TODO: make customizable
      std::wstring directoryPrefix = L"INDEX "s + GetTwoDigit(offsetNumber) + L"\\"s;
      
      std::vector<std::wstring> audioFilenames;
      std::vector<unsigned long long> audioSizes;
      for (auto trackNumber = firstTrackNumber; trackNumber <= lastTrackNumber; trackNumber++) {
        auto track = cueAudioLoader.GetTrack(trackNumber);
        if (!track) {
          continue;
        }

        // TODO: make customizable
        const auto audioFilename = GetTwoDigit(trackNumber) + L" "s + track->title.value_or(L"Unkown Title"s) + L".wav"s;

        auto trackAudioSource = cueAudioLoader.GetTrackAudioSource(trackNumber, offsetNumber);

        Insert(directoryTree, directoryPrefix + audioFilename, std::make_shared<AudioSourceToSourceWAV>(trackAudioSource, &cueAudioLoader.GetCueSheet(), trackNumber), fileIndexCount);

        audioFilenames.emplace_back(audioFilename);
        audioSizes.emplace_back(trackAudioSource->GetSize());
      }

      {
        const auto data = GeneratePlaylistCue(cueAudioLoader.GetCueSheet(), offsetNumber, audioFilenames);
        Insert(directoryTree, directoryPrefix + cueBasename + L".cue"s, std::make_shared<MemorySource>(data.data(), data.size()), fileIndexCount);
      }

      {
        const auto data = GeneratePlaylistM3U8(cueAudioLoader.GetCueSheet(), audioSizes, audioFilenames);
        Insert(directoryTree, directoryPrefix + cueBasename + L".m3u8"s, std::make_shared<MemorySource>(data.data(), data.size()), fileIndexCount);
      }
    }
  } catch (...) {
    if (IsValidHandle(cueFileHandle)) {
      CloseHandle(cueFileHandle);
      cueFileHandle = NULL;
    }
    throw;
  }
}


CueSourceMount::~CueSourceMount() {}


NTSTATUS CueSourceMount::ReturnPathOrNameNotFoundError(LPCWSTR filepath) const {
  assert(!IsRootDirectory(filepath));
  return directoryTree.Exists(GetParentPath(filepath).substr(1)) ? STATUS_OBJECT_NAME_NOT_FOUND : STATUS_OBJECT_PATH_NOT_FOUND;
}


const DirectoryTree* CueSourceMount::GetDirectoryTree(LPCWSTR filepath) const {
  return directoryTree.Get(filepath + 1);
}


DWORD CueSourceMount::GetVolumeSerialNumber() const {
  return volumeSerialNumber;
}


BOOL CueSourceMount::GetSourceInfo(SOURCE_INFO* sourceInfo) {
  if (sourceInfo) {
    *sourceInfo = {
      FALSE,
    };
  }
  return TRUE;
}


NTSTATUS CueSourceMount::GetFileInfo(LPCWSTR FileName, WIN32_FILE_ATTRIBUTE_DATA* Win32FileAttributeData) {
  if (!Win32FileAttributeData) {
    return STATUS_SUCCESS;
  }
  const auto ptrDirectoryTree = GetDirectoryTree(FileName);
  if (!ptrDirectoryTree) {
    return ReturnPathOrNameNotFoundError(FileName);
  }
  const auto fileSize = ptrDirectoryTree->source ? ptrDirectoryTree->source->GetSize() : 0;
  Win32FileAttributeData->dwFileAttributes = ptrDirectoryTree->directory ? DirectoryTree::DirectoryFileAttributes : DirectoryTree::FileFileAttributes;
  Win32FileAttributeData->ftCreationTime = cueFileInfo.ftCreationTime;
  Win32FileAttributeData->ftLastAccessTime = cueFileInfo.ftLastAccessTime;
  Win32FileAttributeData->ftLastWriteTime = cueFileInfo.ftLastWriteTime;
  Win32FileAttributeData->nFileSizeHigh = (fileSize >> 32) & 0xFFFFFFFF;
  Win32FileAttributeData->nFileSizeLow = fileSize & 0xFFFFFFFF;
  return STATUS_SUCCESS;
}


NTSTATUS CueSourceMount::GetDirectoryInfo(LPCWSTR FileName) {
  const auto ptrDirectoryTree = GetDirectoryTree(FileName);
  if (!ptrDirectoryTree) {
    return ReturnPathOrNameNotFoundError(FileName);
  }
  if (!ptrDirectoryTree->directory) {
    return STATUS_NOT_A_DIRECTORY;
  }
  return ptrDirectoryTree->children.empty() ? STATUS_SUCCESS : STATUS_DIRECTORY_NOT_EMPTY;
}


NTSTATUS CueSourceMount::ListFiles(LPCWSTR FileName, PListFilesCallback Callback, CALLBACK_CONTEXT CallbackContext) {
  const auto ptrDirectoryTree = GetDirectoryTree(FileName);
  if (!ptrDirectoryTree) {
    return ReturnPathOrNameNotFoundError(FileName);
  }
  if (!ptrDirectoryTree->directory) {
    return STATUS_NOT_A_DIRECTORY;
  }
  for (auto& [key, childDirectoryTree] : ptrDirectoryTree->children) {
    const auto fileSize = childDirectoryTree.source ? childDirectoryTree.source->GetSize() : 0;
    WIN32_FIND_DATAW win32FindDataW{
      childDirectoryTree.directory ? DirectoryTree::DirectoryFileAttributes : DirectoryTree::FileFileAttributes,
      cueFileInfo.ftCreationTime,
      cueFileInfo.ftLastAccessTime,
      cueFileInfo.ftLastWriteTime,
      static_cast<DWORD>((fileSize >> 32) & 0xFFFFFFFF),
      static_cast<DWORD>(fileSize & 0xFFFFFFFF),
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


NTSTATUS CueSourceMount::ListStreams(LPCWSTR FileName, PListStreamsCallback Callback, CALLBACK_CONTEXT CallbackContext) {
  return STATUS_SUCCESS;
}


NTSTATUS CueSourceMount::DGetDiskFreeSpace(PULONGLONG FreeBytesAvailable, PULONGLONG TotalNumberOfBytes, PULONGLONG TotalNumberOfFreeBytes, PDOKAN_FILE_INFO DokanFileInfo) {
  if (FreeBytesAvailable) {
    *FreeBytesAvailable = 0;
  }
  if (TotalNumberOfBytes) {
    // sloppy
    *TotalNumberOfBytes = cueAudioLoader.GetFullAudioSource()->GetSize();
  }
  if (TotalNumberOfFreeBytes) {
    *TotalNumberOfFreeBytes = 0;
  }
  return STATUS_SUCCESS;
}


NTSTATUS CueSourceMount::DGetVolumeInformation(LPWSTR VolumeNameBuffer, DWORD VolumeNameSize, LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength, LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer, DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo) {
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


NTSTATUS CueSourceMount::ExportStartImpl(PORTATION_INFO* PortationInfo) {
  auto upPortation = std::make_unique<ExportPortation>(*this, PortationInfo);
  auto ptrPortation = upPortation.get();
  portationMap.emplace(ptrPortation, std::move(upPortation));
  PortationInfo->exporterContext = ptrPortation;
  return STATUS_SUCCESS;
}


NTSTATUS CueSourceMount::ExportDataImpl(PORTATION_INFO* PortationInfo) {
  auto ptrPortation = static_cast<ExportPortation*>(PortationInfo->exporterContext);
  return ptrPortation->Export(PortationInfo);
}


NTSTATUS CueSourceMount::ExportFinishImpl(PORTATION_INFO* PortationInfo, BOOL Success) {
  auto ptrPortation = static_cast<ExportPortation*>(PortationInfo->exporterContext);
  const auto status = ptrPortation->Finish(PortationInfo, Success);
  portationMap.erase(ptrPortation);
  return status;
}


std::unique_ptr<SourceMountFileBase> CueSourceMount::DZwCreateFileImpl(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId) {
  return std::make_unique<CueSourceMountFile>(*this, FileName, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, MaybeSwitched, FileContextId, cueFileInfo.ftCreationTime, cueFileInfo.ftLastAccessTime, cueFileInfo.ftLastWriteTime);
}
