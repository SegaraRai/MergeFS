#pragma once

#include <dokan/dokan.h>

#include "../SDK/Plugin/SourceCpp.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <Windows.h>



class FilesystemSourceMountFile;


class FilesystemSourceMount : public SourceMountBase {
  class Portation {
  protected:
    FilesystemSourceMount& sourceMount;
    const std::wstring filepath;
    const FILE_CONTEXT_ID fileContextId;
    const bool empty;
    const std::shared_ptr<FilesystemSourceMountFile> sourceMountFile;
    const std::wstring realPath;

    HANDLE hFile;
    bool needClose;

  public:
    Portation(FilesystemSourceMount& sourceMount, PORTATION_INFO* portationInfo);
    virtual ~Portation();

    virtual NTSTATUS Finish(PORTATION_INFO* portationInfo, bool success);
  };


  class ExportPortation : public Portation {
    static constexpr std::size_t BufferSize = 4096;

    bool directory;
    BY_HANDLE_FILE_INFORMATION byHandleFileInformation;
    std::unique_ptr<char[]> buffer;
    DWORD lastNumberOfBytesWritten;

  public:
    ExportPortation(FilesystemSourceMount& sourceMount, PORTATION_INFO* portationInfo);

    NTSTATUS Export(PORTATION_INFO* portationInfo);
    NTSTATUS Finish(PORTATION_INFO* portationInfo, bool success) override;
  };


  class ImportPortation : public Portation {
    const bool directory;
    const DWORD fileAttributes;
    const FILETIME creationTime;
    const FILETIME lastAccessTime;
    const FILETIME lastWriteTime;
    const DWORD securitySize;
    std::unique_ptr<char[]> securityData;
    const LARGE_INTEGER fileSize;

  public:
    ImportPortation(FilesystemSourceMount& sourceMount, PORTATION_INFO* portationInfo);

    NTSTATUS Import(PORTATION_INFO* portationInfo);
    NTSTATUS Finish(PORTATION_INFO* portationInfo, bool success) override;
  };


  static std::wstring GetRealPathPrefix(const std::wstring& filepath);
  static std::wstring GetRootPath(const std::wstring& realPathPrefix);

  std::mutex subMutex;
  std::unordered_map<FILE_CONTEXT_ID, HANDLE> switchPrepareHandleMap;
  std::unordered_map<Portation*, std::unique_ptr<Portation>> portationMap;
  const std::wstring realPathPrefix;
  const std::wstring rootPath;
  HANDLE rootDirectoryFileHandle;
  BY_HANDLE_FILE_INFORMATION rootDirectoryInfo;
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
  FilesystemSourceMount(const PLUGIN_INITIALIZE_MOUNT_INFO* InitializeMountInfo, SOURCE_CONTEXT_ID sourceContextId);
  ~FilesystemSourceMount();
  std::wstring GetRealPath(LPCWSTR filepath);
  BOOL GetSourceInfo(SOURCE_INFO* sourceInfo) override;
  NTSTATUS GetFileInfo(LPCWSTR FileName, WIN32_FILE_ATTRIBUTE_DATA* Win32FileAttributeData) override;
  NTSTATUS GetDirectoryInfo(LPCWSTR FileName) override;
  NTSTATUS RemoveFile(LPCWSTR FileName) override;
  NTSTATUS ListFiles(LPCWSTR FileName, PListFilesCallback Callback, CALLBACK_CONTEXT CallbackContext) override;
  NTSTATUS ListStreams(LPCWSTR FileName, PListStreamsCallback Callback, CALLBACK_CONTEXT CallbackContext) override;
  NTSTATUS SwitchDestinationPrepareImpl(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) override;
  NTSTATUS SwitchDestinationCleanupImpl(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) override;
  NTSTATUS SwitchDestinationCloseImpl(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) override;
  NTSTATUS DGetDiskFreeSpace(PULONGLONG FreeBytesAvailable, PULONGLONG TotalNumberOfBytes, PULONGLONG TotalNumberOfFreeBytes, PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS DGetVolumeInformation(LPWSTR VolumeNameBuffer, DWORD VolumeNameSize, LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength, LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer, DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo) override;
  NTSTATUS ExportStartImpl(PORTATION_INFO* PortationInfo) override;
  NTSTATUS ExportDataImpl(PORTATION_INFO* PortationInfo) override;
  NTSTATUS ExportFinishImpl(PORTATION_INFO* PortationInfo, BOOL Success) override;
  NTSTATUS ImportStartImpl(PORTATION_INFO* PortationInfo) override;
  NTSTATUS ImportDataImpl(PORTATION_INFO* PortationInfo) override;
  NTSTATUS ImportFinishImpl(PORTATION_INFO* PortationInfo, BOOL Success) override;
  std::unique_ptr<SourceMountFileBase> DZwCreateFileImpl(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId) override;
  std::unique_ptr<SourceMountFileBase> SwitchDestinationOpenImpl(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) override;
  HANDLE TransferSwitchDestinationHandle(FILE_CONTEXT_ID fileContextId);
};
