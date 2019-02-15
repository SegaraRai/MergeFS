#pragma once

#include <dokan/dokan.h>

#include "../SDK/Plugin/SourceCppReadonly.hpp"

#include <7z/CPP/Common/Common.h>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <Windows.h>

#include "NanaZ/Archive.hpp"
#include "NanaZ/NanaZ.hpp"


class ArchiveSourceMountFile;


class ArchiveSourceMount : public ReadonlySourceMountBase {
  class ExportPortation {
    static constexpr std::size_t BufferSize = 4096;

  protected:
    ArchiveSourceMount& sourceMount;
    const std::wstring filepath;
    const FILE_CONTEXT_ID fileContextId;
    const bool empty;
    const std::shared_ptr<ArchiveSourceMountFile> sourceMountFile;
    const std::wstring realPath;
    const DirectoryTree* const ptrDirectoryTree;

    UInt32 lastNumberOfBytesWritten;
    bool directory;
    std::unique_ptr<char[]> buffer;

  public:
    ExportPortation(ArchiveSourceMount& sourceMount, PORTATION_INFO* portationInfo);

    NTSTATUS Export(PORTATION_INFO* portationInfo);
    NTSTATUS Finish(PORTATION_INFO* portationInfo, bool success);
  };


  NanaZ& nanaZ;
  std::mutex subMutex;
  std::unordered_map<ExportPortation*, std::unique_ptr<ExportPortation>> portationMap;
  std::wstring absolutePath;
  std::wstring archiveFilepath;
  std::wstring pathPrefix;
  std::wstring pathPrefixWb;
  HANDLE archiveFileHandle;
  BY_HANDLE_FILE_INFORMATION archiveFileInfo;
  std::wstring baseVolumeName;
  DWORD baseVolumeSerialNumber;
  DWORD baseMaximumComponentLength;
  DWORD baseFileSystemFlags;
  std::wstring baseFileSystemName;
  std::wstring volumeName;
  DWORD volumeSerialNumber;
  DWORD maximumComponentLength;
  DWORD fileSystemFlags;
  std::wstring fileSystemName;
  std::optional<Archive> archiveN;

public:
  ArchiveSourceMount(NanaZ& nanaZ, const PLUGIN_INITIALIZE_MOUNT_INFO* InitializeMountInfo, SOURCE_CONTEXT_ID sourceContextId);
  ~ArchiveSourceMount();

  std::wstring GetRealPath(LPCWSTR filepath) const;
  NTSTATUS ReturnPathOrNameNotFoundErrorR(std::wstring_view realPath) const;
  NTSTATUS ReturnPathOrNameNotFoundError(LPCWSTR filepath) const;
  const DirectoryTree* GetDirectoryTreeR(std::wstring_view realPath) const;
  const DirectoryTree* GetDirectoryTree(LPCWSTR filepath) const;
  DWORD GetVolumeSerialNumber() const;

  BOOL GetSourceInfo(SOURCE_INFO* sourceInfo) override;
  NTSTATUS GetFileInfo(LPCWSTR FileName, WIN32_FILE_ATTRIBUTE_DATA* Win32FileAttributeData) override;
  NTSTATUS GetDirectoryInfo(LPCWSTR FileName) override;
  NTSTATUS ListFiles(LPCWSTR FileName, PListFilesCallback Callback, CALLBACK_CONTEXT CallbackContext) override;
  NTSTATUS ListStreams(LPCWSTR FileName, PListStreamsCallback Callback, CALLBACK_CONTEXT CallbackContext) override;
  NTSTATUS DGetDiskFreeSpace(PULONGLONG FreeBytesAvailable, PULONGLONG TotalNumberOfBytes, PULONGLONG TotalNumberOfFreeBytes, PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DGetVolumeInformation(LPWSTR VolumeNameBuffer, DWORD VolumeNameSize, LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength, LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer, DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS ExportStartImpl(PORTATION_INFO* PortationInfo) override;
  NTSTATUS ExportDataImpl(PORTATION_INFO* PortationInfo) override;
  NTSTATUS ExportFinishImpl(PORTATION_INFO* PortationInfo, BOOL Success) override;
  std::unique_ptr<SourceMountFileBase> DZwCreateFileImpl(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId) override;
};
