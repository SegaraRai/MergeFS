#pragma once

#include "../dokan/dokan/dokan.h"

#include "SourcePlugin.hpp"

#include <functional>
#include <string_view>



class MountSource {
public:
  using SOURCE_CONTEXT_ID = SourcePlugin::SOURCE_CONTEXT_ID;
  using FILE_CONTEXT_ID = SourcePlugin::FILE_CONTEXT_ID;
  using SOURCE_INFO = SourcePlugin::SOURCE_INFO;
  using PORTATION_INFO = SourcePlugin::PORTATION_INFO;

  using ListFilesCallback = std::function<void(PWIN32_FIND_DATAW)>;
  using ListStreamsCallback = std::function<void(PWIN32_FIND_STREAM_DATA)>;
  
  enum class FileType {
    Inexistent,
    Directory,
    File,
  };

  static constexpr SOURCE_CONTEXT_ID SOURCE_CONTEXT_ID_NULL = SourcePlugin::SOURCE_CONTEXT_ID_NULL;
  static constexpr FILE_CONTEXT_ID FILE_CONTEXT_ID_NULL = SourcePlugin::FILE_CONTEXT_ID_NULL;

  /*
  class ExportedFileRAII : public SourcePlugin::ExportedFileRAII {
  public:
    ExportedFileRAII(MountSource& mountSource, LPCWSTR FileName, bool Empty);
  };
  //*/

private:
  SourcePlugin& m_sourcePlugin;
  SOURCE_CONTEXT_ID m_sourceContextId;
  SOURCE_INFO m_sourceInfo;

public:
  static FileType FileAttributesToFileType(DWORD fileAttributes) noexcept;

  MountSource(std::wstring_view sourceFileName, SourcePlugin& sourcePlugin);
  ~MountSource();

  const SOURCE_INFO& GetSourceInfo() const noexcept;
  NTSTATUS GetFileInfoAttributes(LPCWSTR FileName, DWORD* FileAttributes) const noexcept;
  FileType GetFileType(LPCWSTR FileName) const;
  FileType GetFileTypeNe(LPCWSTR FileName, FileType fallback = FileType::Inexistent) const noexcept;
  NTSTATUS IsDirectoryEmptyNe(LPCWSTR FileName) const noexcept;
  bool IsDirectoryEmpty(LPCWSTR FileName) const;
  NTSTATUS GetFileInfo(LPCWSTR FileName, WIN32_FILE_ATTRIBUTE_DATA* Win32FileAttributeData) const noexcept;
  NTSTATUS RemoveFile(LPCWSTR FileName) noexcept;
  NTSTATUS ExportStart(PORTATION_INFO* PortationInfo) noexcept;
  NTSTATUS ExportData(PORTATION_INFO* PortationInfo) noexcept;
  NTSTATUS ExportFinish(PORTATION_INFO* PortationInfo, bool Success) noexcept;
  NTSTATUS ImportStart(PORTATION_INFO* PortationInfo) noexcept;
  NTSTATUS ImportData(PORTATION_INFO* PortationInfo) noexcept;
  NTSTATUS ImportFinish(PORTATION_INFO* PortationInfo, bool Success) noexcept;
  NTSTATUS SwitchSourceClose(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept;
  NTSTATUS SwitchDestinationPrepare(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept;
  NTSTATUS SwitchDestinationOpen(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept;
  NTSTATUS SwitchDestinationCleanup(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept;
  NTSTATUS SwitchDestinationClose(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept;
  NTSTATUS ListFiles(LPCWSTR FileName, ListFilesCallback Callback) const noexcept;
  NTSTATUS ListStreams(LPCWSTR FileName, ListStreamsCallback Callback) const noexcept;

  NTSTATUS DZwCreateFile(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, bool MaybeSwitched, FILE_CONTEXT_ID FileContextId) noexcept;
  void DCleanup(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept;
  void DCloseFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept;
  NTSTATUS DReadFile(LPCWSTR FileName, LPVOID Buffer, DWORD BufferLength, LPDWORD ReadLength, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept;
  NTSTATUS DWriteFile(LPCWSTR FileName, LPCVOID Buffer, DWORD NumberOfBytesToWrite, LPDWORD NumberOfBytesWritten, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept;
  NTSTATUS DFlushFileBuffers(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept;
  NTSTATUS DGetFileInformation(LPCWSTR FileName, LPBY_HANDLE_FILE_INFORMATION Buffer, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept;
  NTSTATUS DSetFileAttributes(LPCWSTR FileName, DWORD FileAttributes, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept;
  NTSTATUS DSetFileTime(LPCWSTR FileName, const FILETIME* CreationTime, const FILETIME* LastAccessTime, const FILETIME* LastWriteTime, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept;
  NTSTATUS DDeleteFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept;
  NTSTATUS DDeleteDirectory(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept;
  NTSTATUS DMoveFile(LPCWSTR FileName, LPCWSTR NewFileName, bool ReplaceIfExisting, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept;
  NTSTATUS DSetEndOfFile(LPCWSTR FileName, LONGLONG ByteOffset, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept;
  NTSTATUS DSetAllocationSize(LPCWSTR FileName, LONGLONG AllocSize, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept;
  NTSTATUS DGetDiskFreeSpace(PULONGLONG FreeBytesAvailable, PULONGLONG TotalNumberOfBytes, PULONGLONG TotalNumberOfFreeBytes, PDOKAN_FILE_INFO DokanFileInfo) noexcept;
  NTSTATUS DGetVolumeInformation(LPWSTR VolumeNameBuffer, DWORD VolumeNameSize, LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength, LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer, DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo) noexcept;
  NTSTATUS DGetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PULONG LengthNeeded, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept;
  NTSTATUS DSetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept;
};
