#pragma once

#define USE_SHARED_PTR_FOR_FILE_CONTEXT

#include "../dokan/dokan/dokan.h"

#include "MountSource.hpp"
#include "MetadataStore.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>



class Mount {
  using FILE_CONTEXT_ID = MountSource::FILE_CONTEXT_ID;
  using PORTATION_INFO = MountSource::PORTATION_INFO;
  using FileType = MountSource::FileType;

  static constexpr FILE_CONTEXT_ID FILE_CONTEXT_ID_NULL = MountSource::FILE_CONTEXT_ID_NULL;

  static constexpr std::size_t TopSourceIndex = 0;
  static constexpr FILE_CONTEXT_ID FileContextIdStart = FILE_CONTEXT_ID_NULL + 1;

  struct FileContext {
    std::shared_mutex mutex;
    FILE_CONTEXT_ID id;
    Mount& mount;
    std::reference_wrapper<MountSource> mountSource;
    std::wstring filename;
    std::wstring resolvedFilename;
    bool directory;
    std::atomic<bool> writable;
    std::atomic<bool> copyDeferred;
    std::atomic<bool> autoUpdateLastAccessTime;
    std::atomic<bool> autoUpdateLastWriteTime;
    ULONGLONG fileIndexBase;
    DOKAN_IO_SECURITY_CONTEXT SecurityContext;
    ACCESS_MASK DesiredAccess;
    ULONG FileAttributes;
    ULONG ShareAccess;
    ULONG CreateDisposition;
    ULONG CreateOptions;

    FileContext(const FileContext&) = delete;

    NTSTATUS UpdateLastAccessTime();
    NTSTATUS UpdateLastWriteTime();
  };

  static std::shared_mutex gFileContextMapMutex;
  static std::unordered_map<Mount::FileContext*, std::shared_ptr<Mount::FileContext>> gFileContextPtrToSharedPtrMap;

  std::mutex m_mutex;
  std::shared_mutex m_metadataMutex;
  const std::wstring m_mountPoint;
  std::vector<std::unique_ptr<MountSource>> m_mountSources;
  MountSource& m_topSource;
  const bool m_writable;
  const std::wstring m_metadataFileName;
  const bool m_deferCopyEnabled;
  const bool m_caseSensitive;
  MetadataStore m_metadataStore;
  std::atomic<bool> m_mounted;
#ifdef USE_SHARED_PTR_FOR_FILE_CONTEXT
  std::unordered_map<FILE_CONTEXT_ID, std::shared_ptr<FileContext>> m_fileContextMap;
#else
  std::unordered_map<FILE_CONTEXT_ID, std::unique_ptr<FileContext>> m_fileContextMap;
#endif
  FILE_CONTEXT_ID m_minimumUnusedFileContextId;
  std::vector<ULONGLONG> m_fileIndexBases;
  std::thread m_thread;

  static bool HasFileContext(PDOKAN_FILE_INFO DokanFileInfo) noexcept;
  static FileContext* GetFileContextPtr(PDOKAN_FILE_INFO DokanFileInfo) noexcept;
#ifdef USE_SHARED_PTR_FOR_FILE_CONTEXT
  static std::shared_ptr<FileContext> GetFileContextSharedPtr(PDOKAN_FILE_INFO DokanFileInfo);
#else
  static FileContext* GetFileContextSharedPtr(PDOKAN_FILE_INFO DokanFileInfo);
#endif
  //static FileContext& GetFileContext(PDOKAN_FILE_INFO DokanFileInfo);
  static NTSTATUS TransportR(std::wstring_view path, bool empty, FILE_CONTEXT_ID fileContextId, MountSource& source, MountSource& destination);

  std::wstring FilenameToKey(std::wstring_view filename) const;
  std::optional<std::wstring> ResolveFilepathN(std::wstring_view filename);
  std::wstring ResolveFilepath(std::wstring_view filename);
  std::optional<std::size_t> GetMountSourceIndexR(std::wstring_view resolvedFilename);
  std::optional<std::size_t> GetMountSourceIndex(std::wstring_view filename);
  bool FileExists(std::wstring_view filename);
  FileType GetFileTypeR(std::wstring_view resolvedFilename);
  FileType GetFileType(std::wstring_view filename);
  void CopyFileToTopSourceR(std::wstring_view filename, bool empty = false, FILE_CONTEXT_ID fileContextId = FILE_CONTEXT_ID_NULL);
  void CopyFileToTopSource(std::wstring_view filename, bool empty = false, FILE_CONTEXT_ID fileContextId = FILE_CONTEXT_ID_NULL);
  void RemoveFile(std::wstring_view filename);
  FILE_CONTEXT_ID AssignFileContextId(std::wstring_view FileName, std::wstring_view ResolvedFileName, PDOKAN_FILE_INFO DokanFileInfo, std::size_t mountSourceIndex, bool isDirectory, bool deferCopy, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions);
  bool ReleaseFileContextId(FILE_CONTEXT_ID FileContextId) noexcept;
  bool ReleaseFileContextId(PDOKAN_FILE_INFO DokanFileInfo) noexcept;
  NTSTATUS TransportIfNeeded(PDOKAN_FILE_INFO DokanFileInfo);

public:
  Mount(std::wstring_view mountPoint, bool writable, std::wstring_view metadataFileName, bool deferCopyEnabled, bool caseSensitive, std::vector<std::unique_ptr<MountSource>>&& sources, std::function<void(int)> callback);
  ~Mount();

  bool IsWritable() const;

  bool SafeUnmount();

  NTSTATUS DZwCreateFile(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo) noexcept;
  void DCleanup(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo) noexcept;
  void DCloseFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo) noexcept;
  NTSTATUS DReadFile(LPCWSTR FileName, LPVOID Buffer, DWORD BufferLength, LPDWORD ReadLength, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo) noexcept;
  NTSTATUS DWriteFile(LPCWSTR FileName, LPCVOID Buffer, DWORD NumberOfBytesToWrite, LPDWORD NumberOfBytesWritten, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo) noexcept;
  NTSTATUS DFlushFileBuffers(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo) noexcept;
  NTSTATUS DGetFileInformation(LPCWSTR FileName, LPBY_HANDLE_FILE_INFORMATION Buffer, PDOKAN_FILE_INFO DokanFileInfo) noexcept;
  NTSTATUS DFindFiles(LPCWSTR FileName, PFillFindData FillFindData, PDOKAN_FILE_INFO DokanFileInfo) noexcept;
  NTSTATUS DSetFileAttributes(LPCWSTR FileName, DWORD FileAttributes, PDOKAN_FILE_INFO DokanFileInfo) noexcept;
  NTSTATUS DSetFileTime(LPCWSTR FileName, const FILETIME* CreationTime, const FILETIME* LastAccessTime, const FILETIME* LastWriteTime, PDOKAN_FILE_INFO DokanFileInfo) noexcept;
  NTSTATUS DDeleteFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo) noexcept;
  NTSTATUS DDeleteDirectory(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo) noexcept;
  NTSTATUS DMoveFile(LPCWSTR FileName, LPCWSTR NewFileName, BOOL ReplaceIfExisting, PDOKAN_FILE_INFO DokanFileInfo) noexcept;
  NTSTATUS DSetEndOfFile(LPCWSTR FileName, LONGLONG ByteOffset, PDOKAN_FILE_INFO DokanFileInfo) noexcept;
  NTSTATUS DSetAllocationSize(LPCWSTR FileName, LONGLONG AllocSize, PDOKAN_FILE_INFO DokanFileInfo) noexcept;
  NTSTATUS DGetDiskFreeSpace(PULONGLONG FreeBytesAvailable, PULONGLONG TotalNumberOfBytes, PULONGLONG TotalNumberOfFreeBytes, PDOKAN_FILE_INFO DokanFileInfo) noexcept;
  NTSTATUS DGetVolumeInformation(LPWSTR VolumeNameBuffer, DWORD VolumeNameSize, LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength, LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer, DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo) noexcept;
  NTSTATUS DMounted(PDOKAN_FILE_INFO DokanFileInfo) noexcept;
  NTSTATUS DUnmounted(PDOKAN_FILE_INFO DokanFileInfo) noexcept;
  NTSTATUS DGetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PULONG LengthNeeded, PDOKAN_FILE_INFO DokanFileInfo) noexcept;
  NTSTATUS DSetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PDOKAN_FILE_INFO DokanFileInfo) noexcept;
  NTSTATUS DFindStreams(LPCWSTR FileName, PFillFindStreamData FillFindStreamData, PDOKAN_FILE_INFO DokanFileInfo) noexcept;
};
