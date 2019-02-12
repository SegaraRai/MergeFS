#pragma once

#if !defined(FROMLIBMERGEFS) && !defined(FROMPLUGIN) && !defined(FROMCLIENT)
# define FROMPLUGIN
#endif

#include "../LibMergeFS.h"
#include "Common.h"

#ifndef DOKAN_VERSION
# error include dokan.h before including this
#endif


typedef DWORD SOURCE_CONTEXT_ID;
typedef DWORD FILE_CONTEXT_ID;
typedef void* CALLBACK_CONTEXT;


#ifdef __cplusplus
constexpr SOURCE_CONTEXT_ID SOURCE_CONTEXT_ID_NULL = 0;
constexpr FILE_CONTEXT_ID FILE_CONTEXT_ID_NULL = 0;
#else
# define SOURCE_CONTEXT_ID_NULL ((SOURCE_CONTEXT_ID)0)
# define FILE_CONTEXT_ID_NULL ((FILE_CONTEXT_ID)0)
#endif


typedef void(WINAPI *PListFilesCallback)(PWIN32_FIND_DATAW FindDataW, CALLBACK_CONTEXT CallbackContext) MFNOEXCEPT;
typedef void(WINAPI *PListStreamsCallback)(PWIN32_FIND_STREAM_DATA FindStreamData, CALLBACK_CONTEXT CallbackContext) MFNOEXCEPT;


#pragma pack(push, 1)


typedef struct {
  BOOL writable;
} SOURCE_INFO;


typedef struct {
  // set by libmergefs, read by both exporter and importer
  const LPCWSTR filepath;
  const FILE_CONTEXT_ID fileContextId;    // maybe FILE_CONTEXT_ID_NULL
  const BOOL empty;

  // set by exporter, read by exporter
  void* exporterContext;

  // set by importer, read by importer
  void* importerContext;

  // set by exporter, read by importer
  BOOL directory;
  DWORD fileAttributes;
  FILETIME creationTime;
  FILETIME lastAccessTime;
  FILETIME lastWriteTime;
  DWORD securitySize;
  LPCSTR securityData;
  LARGE_INTEGER fileSize;
  LARGE_INTEGER currentOffset;
  DWORD currentSize;
  LPCSTR currentData;
} PORTATION_INFO;


#ifdef FROMLIBMERGEFS
static_assert(sizeof(SOURCE_INFO) == 1 * 4);
static_assert(sizeof(PORTATION_INFO) == 6 * 4 + 5 * 8 + 5 * sizeof(void*));
#endif


#pragma pack(pop)


#ifdef FROMLIBMERGEFS
namespace External::Plugin::Source {
#endif

MFEXTERNC MFPEXPORT BOOL WINAPI SIsSupported(LPCWSTR FileName) MFNOEXCEPT;

MFEXTERNC MFPEXPORT NTSTATUS WINAPI Mount(LPCWSTR FileName, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT BOOL WINAPI Unmount(SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;

MFEXTERNC MFPEXPORT BOOL WINAPI GetSourceInfo(SOURCE_INFO* sourceInfo, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI GetFileInfo(LPCWSTR FileName, WIN32_FILE_ATTRIBUTE_DATA* Win32FileAttributeData, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI GetDirectoryInfo(LPCWSTR FileName, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI RemoveFile(LPCWSTR FileName, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI ExportStart(PORTATION_INFO* PortationInfo, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI ExportData(PORTATION_INFO* PortationInfo, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI ExportFinish(PORTATION_INFO* PortationInfo, BOOL Success, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI ImportStart(PORTATION_INFO* PortationInfo, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI ImportData(PORTATION_INFO* PortationInfo, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI ImportFinish(PORTATION_INFO* PortationInfo, BOOL Success, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI SwitchSourceClose(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI SwitchDestinationPrepare(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI SwitchDestinationOpen(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI SwitchDestinationCleanup(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI SwitchDestinationClose(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI ListFiles(LPCWSTR FileName, PListFilesCallback Callback, CALLBACK_CONTEXT CallbackContext, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI ListStreams(LPCWSTR FileName, PListStreamsCallback Callback, CALLBACK_CONTEXT CallbackContext, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;

MFEXTERNC MFPEXPORT NTSTATUS WINAPI DZwCreateFile(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT void WINAPI DCleanup(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT void WINAPI DCloseFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI DReadFile(LPCWSTR FileName, LPVOID Buffer, DWORD BufferLength, LPDWORD ReadLength, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI DWriteFile(LPCWSTR FileName, LPCVOID Buffer, DWORD NumberOfBytesToWrite, LPDWORD NumberOfBytesWritten, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI DFlushFileBuffers(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI DGetFileInformation(LPCWSTR FileName, LPBY_HANDLE_FILE_INFORMATION Buffer, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI DSetFileAttributes(LPCWSTR FileName, DWORD FileAttributes, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI DSetFileTime(LPCWSTR FileName, const FILETIME* CreationTime, const FILETIME* LastAccessTime, const FILETIME* LastWriteTime, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI DDeleteFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI DDeleteDirectory(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI DMoveFile(LPCWSTR FileName, LPCWSTR NewFileName, BOOL ReplaceIfExisting, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI DSetEndOfFile(LPCWSTR FileName, LONGLONG ByteOffset, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI DSetAllocationSize(LPCWSTR FileName, LONGLONG AllocSize, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI DGetDiskFreeSpace(PULONGLONG FreeBytesAvailable, PULONGLONG TotalNumberOfBytes, PULONGLONG TotalNumberOfFreeBytes, PDOKAN_FILE_INFO DokanFileInfo, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI DGetVolumeInformation(LPWSTR VolumeNameBuffer, DWORD VolumeNameSize, LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength, LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer, DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI DGetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PULONG LengthNeeded, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;
MFEXTERNC MFPEXPORT NTSTATUS WINAPI DSetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, SOURCE_CONTEXT_ID sourceContextId) MFNOEXCEPT;

#ifdef FROMLIBMERGEFS
}
#endif
