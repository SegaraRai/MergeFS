// Main.h
//

#ifndef LZZ_Main_h
#define LZZ_Main_h
#define LZZ_INLINE inline
class SourceMountFile : public SourceMountFileBase
{
  std::wstring const realPath;
  bool directory;
  HANDLE hFile;
  void Constructor ();
public:
  SourceMountFile (SourceMountBase & sourceMount, LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId, std::wstring_view realPath);
  SourceMountFile (SourceMountBase & sourceMount, LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId, std::wstring_view realPath);
  ~ SourceMountFile ();
  NTSTATUS SwitchSourceClose (PDOKAN_FILE_INFO DokanFileInfo);
  NTSTATUS SwitchDestinationCleanup (PDOKAN_FILE_INFO DokanFileInfo);
  NTSTATUS SwitchDestinationClose (PDOKAN_FILE_INFO DokanFileInfo);
  void DCleanup (PDOKAN_FILE_INFO DokanFileInfo);
  void DCloseFile (PDOKAN_FILE_INFO DokanFileInfo);
  NTSTATUS DReadFile (LPVOID Buffer, DWORD BufferLength, LPDWORD ReadLength, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo);
  NTSTATUS DWriteFile (LPCVOID Buffer, DWORD NumberOfBytesToWrite, LPDWORD NumberOfBytesWritten, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo);
  NTSTATUS DFlushFileBuffers (PDOKAN_FILE_INFO DokanFileInfo);
  NTSTATUS DGetFileInformation (LPBY_HANDLE_FILE_INFORMATION Buffer, PDOKAN_FILE_INFO DokanFileInfo);
  NTSTATUS DSetFileAttributes (DWORD FileAttributes, PDOKAN_FILE_INFO DokanFileInfo);
  NTSTATUS DSetFileTime (FILETIME const * CreationTime, FILETIME const * LastAccessTime, FILETIME const * LastWriteTime, PDOKAN_FILE_INFO DokanFileInfo);
  NTSTATUS DDeleteFile (PDOKAN_FILE_INFO DokanFileInfo);
  NTSTATUS DDeleteDirectory (PDOKAN_FILE_INFO DokanFileInfo);
  NTSTATUS DMoveFile (LPCWSTR NewFileName, BOOL ReplaceIfExisting, PDOKAN_FILE_INFO DokanFileInfo);
  NTSTATUS DSetEndOfFile (LONGLONG ByteOffset, PDOKAN_FILE_INFO DokanFileInfo);
  NTSTATUS DSetAllocationSize (LONGLONG AllocSize, PDOKAN_FILE_INFO DokanFileInfo);
  NTSTATUS DGetFileSecurity (PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PULONG LengthNeeded, PDOKAN_FILE_INFO DokanFileInfo);
  NTSTATUS DSetFileSecurity (PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PDOKAN_FILE_INFO DokanFileInfo);
  HANDLE GetFileHandle ();
};
class SourceMount : public SourceMountBase
{
  static std::wstring GetRealPathPrefix (std::wstring const & filepath);
  static std::wstring GetRootPath (std::wstring const & realPathPrefix);
public:
  SourceMount (LPCWSTR FileName, SOURCE_CONTEXT_ID sourceContextId);
  std::wstring GetRealPath (LPCWSTR filepath);
  BOOL GetSourceInfo (SOURCE_INFO * sourceInfo);
  NTSTATUS GetFileInfo (LPCWSTR FileName, DWORD * FileAttributes, FILETIME * CreationTime, FILETIME * LastAccessTime, FILETIME * LastWriteTime);
  NTSTATUS GetDirectoryInfo (LPCWSTR FileName);
  NTSTATUS RemoveFile (LPCWSTR FileName);
  NTSTATUS ListFiles (LPCWSTR FileName, PListFilesCallback Callback, CALLBACK_CONTEXT CallbackContext);
  NTSTATUS ListStreams (LPCWSTR FileName, PListStreamsCallback Callback, CALLBACK_CONTEXT CallbackContext);
  NTSTATUS SwitchDestinationPrepareImpl (LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId);
  NTSTATUS SwitchDestinationCloseImpl (LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId);
  NTSTATUS DGetDiskFreeSpace (PULONGLONG FreeBytesAvailable, PULONGLONG TotalNumberOfBytes, PULONGLONG TotalNumberOfFreeBytes, PDOKAN_FILE_INFO DokanFileInfo);
  NTSTATUS DGetVolumeInformation (LPWSTR VolumeNameBuffer, DWORD VolumeNameSize, LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength, LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer, DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo);
  NTSTATUS ExportStartImpl (PORTATION_INFO * PortationInfo);
  NTSTATUS ExportDataImpl (PORTATION_INFO * PortationInfo);
  NTSTATUS ExportFinishImpl (PORTATION_INFO * PortationInfo, BOOL Success);
  NTSTATUS ImportStartImpl (PORTATION_INFO * PortationInfo);
  NTSTATUS ImportDataImpl (PORTATION_INFO * PortationInfo);
  NTSTATUS ImportFinishImpl (PORTATION_INFO * PortationInfo, BOOL Success);
  std::unique_ptr <SourceMountFileBase> DZwCreateFileImpl (LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId);
  std::unique_ptr <SourceMountFileBase> SwitchDestinationOpenImpl (LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId);
};
#undef LZZ_INLINE
#endif
