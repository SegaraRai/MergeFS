#pragma once

#include <dokan/dokan.h>

#include "../SDK/Plugin/SourceCppReadonly.hpp"

#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include <Windows.h>

#include "CueAudioLoader.hpp"
#include "DirectoryTree.hpp"


class CueSourceMountFile;


class CueSourceMount : public ReadonlySourceMountBase {
  class ExportPortation {
    static constexpr std::size_t BufferSize = 4096;

  protected:
    CueSourceMount& sourceMount;
    const std::wstring filepath;
    const FILE_CONTEXT_ID fileContextId;
    const bool empty;
    const std::shared_ptr<CueSourceMountFile> sourceMountFile;
    const DirectoryTree* const ptrDirectoryTree;

    std::size_t lastNumberOfBytesWritten;
    bool directory;
    std::unique_ptr<std::byte[]> buffer;

  public:
    ExportPortation(CueSourceMount& sourceMount, PORTATION_INFO* portationInfo);

    NTSTATUS Export(PORTATION_INFO* portationInfo);
    NTSTATUS Finish(PORTATION_INFO* portationInfo, bool success);
  };


  std::mutex subMutex;
  std::unordered_map<ExportPortation*, std::unique_ptr<ExportPortation>> portationMap;
  std::optional<CueAudioLoader> cueAudioLoaderN;
  DirectoryTree directoryTree;
  BY_HANDLE_FILE_INFORMATION cueFileInfo;
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

public:
  CueSourceMount(const PLUGIN_INITIALIZE_MOUNT_INFO* InitializeMountInfo, SOURCE_CONTEXT_ID sourceContextId);
  ~CueSourceMount();

  NTSTATUS ReturnPathOrNameNotFoundError(LPCWSTR filepath) const;
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
