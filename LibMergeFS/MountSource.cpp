#include "MountSource.hpp"
#include "NsError.hpp"

#include <stdexcept>
#include <string>

using namespace std::literals;



MountSource::FileType MountSource::FileAttributesToFileType(DWORD fileAttributes) noexcept {
  switch (fileAttributes) {
    case INVALID_FILE_ATTRIBUTES:
      return FileType::Inexistent;

    case FILE_ATTRIBUTE_NORMAL:
      return FileType::File;

    default:
      return fileAttributes & FILE_ATTRIBUTE_DIRECTORY ? FileType::Directory : FileType::File;
  }
}


MountSource::MountSource(const PLUGIN_INITIALIZE_MOUNT_INFO& initializeMountInfo, SourcePlugin& sourcePlugin) :
  m_sourcePlugin(sourcePlugin)
{
  if (const auto status = m_sourcePlugin.Mount(&initializeMountInfo, m_sourceContextId); status != STATUS_SUCCESS) {
    throw NsError(status);
  }
  m_sourcePlugin.GetSourceInfo(&m_sourceInfo, m_sourceContextId);
}


MountSource::~MountSource() {
  m_sourcePlugin.Unmount(m_sourceContextId);
}


const MountSource::SOURCE_INFO& MountSource::GetSourceInfo() const noexcept {
  return m_sourceInfo;
}


NTSTATUS MountSource::GetFileInfoAttributes(LPCWSTR FileName, DWORD* FileAttributes) const noexcept {
  WIN32_FILE_ATTRIBUTE_DATA win32FileAttributeData;
  const auto status = m_sourcePlugin.GetFileInfo(FileName, &win32FileAttributeData, m_sourceContextId);
  if (FileAttributes) {
    *FileAttributes = win32FileAttributeData.dwFileAttributes;
  }
  return status;
}


MountSource::FileType MountSource::GetFileType(LPCWSTR FileName) const {
  DWORD fileAttributes = INVALID_FILE_ATTRIBUTES;
  const auto status = GetFileInfoAttributes(FileName, &fileAttributes);
  if (status == STATUS_OBJECT_NAME_NOT_FOUND || status == STATUS_OBJECT_PATH_NOT_FOUND) {
    return FileType::Inexistent;
  }
  if (status != STATUS_SUCCESS) {
    throw NsError(status);
  }
  return FileAttributesToFileType(fileAttributes);
}


NTSTATUS MountSource::GetDirectoryInfo(LPCWSTR FileName) const noexcept {
  return m_sourcePlugin.GetDirectoryInfo(FileName, m_sourceContextId);
}


NTSTATUS MountSource::GetFileInfo(LPCWSTR FileName, WIN32_FILE_ATTRIBUTE_DATA* Win32FileAttributeData) const noexcept {
  return m_sourcePlugin.GetFileInfo(FileName, Win32FileAttributeData, m_sourceContextId);
}


NTSTATUS MountSource::RemoveFile(LPCWSTR FileName) noexcept {
  return m_sourcePlugin.RemoveFile(FileName, m_sourceContextId);
}


NTSTATUS MountSource::ExportStart(PORTATION_INFO* PortationInfo) noexcept {
  return m_sourcePlugin.ExportStart(PortationInfo, m_sourceContextId);
}


NTSTATUS MountSource::ExportData(PORTATION_INFO* PortationInfo) noexcept {
  return m_sourcePlugin.ExportData(PortationInfo, m_sourceContextId);
}


NTSTATUS MountSource::ExportFinish(PORTATION_INFO* PortationInfo, bool Success) noexcept {
  return m_sourcePlugin.ExportFinish(PortationInfo, Success ? TRUE : FALSE, m_sourceContextId);
}


NTSTATUS MountSource::ImportStart(PORTATION_INFO* PortationInfo) noexcept {
  return m_sourcePlugin.ImportStart(PortationInfo, m_sourceContextId);
}


NTSTATUS MountSource::ImportData(PORTATION_INFO* PortationInfo) noexcept {
  return m_sourcePlugin.ImportData(PortationInfo, m_sourceContextId);
}


NTSTATUS MountSource::ImportFinish(PORTATION_INFO* PortationInfo, bool Success) noexcept {
  return m_sourcePlugin.ImportFinish(PortationInfo, Success ? TRUE : FALSE, m_sourceContextId);
}


NTSTATUS MountSource::SwitchSourceClose(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept {
  return m_sourcePlugin.SwitchSourceClose(FileName, DokanFileInfo, FileContextId, m_sourceContextId);
}


NTSTATUS MountSource::SwitchDestinationPrepare(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept {
  return m_sourcePlugin.SwitchDestinationPrepare(FileName, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, FileContextId, m_sourceContextId);
}


NTSTATUS MountSource::SwitchDestinationOpen(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept {
  return m_sourcePlugin.SwitchDestinationOpen(FileName, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, FileContextId, m_sourceContextId);
}


NTSTATUS MountSource::SwitchDestinationCleanup(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept {
  return m_sourcePlugin.SwitchDestinationCleanup(FileName, DokanFileInfo, FileContextId, m_sourceContextId);
}


NTSTATUS MountSource::SwitchDestinationClose(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept {
  return m_sourcePlugin.SwitchDestinationClose(FileName, DokanFileInfo, FileContextId, m_sourceContextId);
}


NTSTATUS MountSource::ListFiles(LPCWSTR FileName, ListFilesCallback Callback) const noexcept {
  try {
    return m_sourcePlugin.ListFiles(FileName, Callback, m_sourceContextId);
  } catch (std::bad_alloc&) {
    return STATUS_NO_MEMORY;
  } catch (...) {
    return STATUS_UNSUCCESSFUL;
  }
}


NTSTATUS MountSource::ListStreams(LPCWSTR FileName, ListStreamsCallback Callback) const noexcept {
  try {
    return m_sourcePlugin.ListStreams(FileName, Callback, m_sourceContextId);
  } catch (std::bad_alloc&) {
    return STATUS_NO_MEMORY;
  } catch (...) {
    return STATUS_UNSUCCESSFUL;
  }
}


NTSTATUS MountSource::DZwCreateFile(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, bool MaybeSwitched, FILE_CONTEXT_ID FileContextId) noexcept {
  return m_sourcePlugin.DZwCreateFile(FileName, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, MaybeSwitched ? TRUE : FALSE, FileContextId, m_sourceContextId);
}


void MountSource::DCleanup(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept {
  m_sourcePlugin.DCleanup(FileName, DokanFileInfo, FileContextId, m_sourceContextId);
}


void MountSource::DCloseFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept {
  m_sourcePlugin.DCloseFile(FileName, DokanFileInfo, FileContextId, m_sourceContextId);
}


NTSTATUS MountSource::DReadFile(LPCWSTR FileName, LPVOID Buffer, DWORD BufferLength, LPDWORD ReadLength, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept {
  // MUST BE THEAD SAFE
  return m_sourcePlugin.DReadFile(FileName, Buffer, BufferLength, ReadLength, Offset, DokanFileInfo, FileContextId, m_sourceContextId);
}


NTSTATUS MountSource::DWriteFile(LPCWSTR FileName, LPCVOID Buffer, DWORD NumberOfBytesToWrite, LPDWORD NumberOfBytesWritten, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept {
  // MUST BE THEAD SAFE
  return m_sourcePlugin.DWriteFile(FileName, Buffer, NumberOfBytesToWrite, NumberOfBytesWritten, Offset, DokanFileInfo, FileContextId, m_sourceContextId);
}


NTSTATUS MountSource::DFlushFileBuffers(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept {
  return m_sourcePlugin.DFlushFileBuffers(FileName, DokanFileInfo, FileContextId, m_sourceContextId);
}


NTSTATUS MountSource::DGetFileInformation(LPCWSTR FileName, LPBY_HANDLE_FILE_INFORMATION Buffer, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept {
  return m_sourcePlugin.DGetFileInformation(FileName, Buffer, DokanFileInfo, FileContextId, m_sourceContextId);
}


NTSTATUS MountSource::DSetFileAttributes(LPCWSTR FileName, DWORD FileAttributes, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept {
  return m_sourcePlugin.DSetFileAttributes(FileName, FileAttributes, DokanFileInfo, FileContextId, m_sourceContextId);
}


NTSTATUS MountSource::DSetFileTime(LPCWSTR FileName, const FILETIME* CreationTime, const FILETIME* LastAccessTime, const FILETIME* LastWriteTime, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept {
  return m_sourcePlugin.DSetFileTime(FileName, CreationTime, LastAccessTime, LastWriteTime, DokanFileInfo, FileContextId, m_sourceContextId);
}


NTSTATUS MountSource::DDeleteFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept {
  return m_sourcePlugin.DDeleteFile(FileName, DokanFileInfo, FileContextId, m_sourceContextId);
}


NTSTATUS MountSource::DDeleteDirectory(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept {
  return m_sourcePlugin.DDeleteDirectory(FileName, DokanFileInfo, FileContextId, m_sourceContextId);
}


NTSTATUS MountSource::DMoveFile(LPCWSTR FileName, LPCWSTR NewFileName, bool ReplaceIfExisting, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept {
  return m_sourcePlugin.DMoveFile(FileName, NewFileName, ReplaceIfExisting, DokanFileInfo, FileContextId, m_sourceContextId);
}


NTSTATUS MountSource::DSetEndOfFile(LPCWSTR FileName, LONGLONG ByteOffset, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept {
  return m_sourcePlugin.DSetEndOfFile(FileName, ByteOffset, DokanFileInfo, FileContextId, m_sourceContextId);
}


NTSTATUS MountSource::DSetAllocationSize(LPCWSTR FileName, LONGLONG AllocSize, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept {
  return m_sourcePlugin.DSetAllocationSize(FileName, AllocSize, DokanFileInfo, FileContextId, m_sourceContextId);
}


NTSTATUS MountSource::DGetDiskFreeSpace(PULONGLONG FreeBytesAvailable, PULONGLONG TotalNumberOfBytes, PULONGLONG TotalNumberOfFreeBytes, PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  return m_sourcePlugin.DGetDiskFreeSpace(FreeBytesAvailable, TotalNumberOfBytes, TotalNumberOfFreeBytes, DokanFileInfo, m_sourceContextId);
}


NTSTATUS MountSource::DGetVolumeInformation(LPWSTR VolumeNameBuffer, DWORD VolumeNameSize, LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength, LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer, DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  return m_sourcePlugin.DGetVolumeInformation(VolumeNameBuffer, VolumeNameSize, VolumeSerialNumber, MaximumComponentLength, FileSystemFlags, FileSystemNameBuffer, FileSystemNameSize, DokanFileInfo, m_sourceContextId);
}


NTSTATUS MountSource::DGetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PULONG LengthNeeded, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept {
  return m_sourcePlugin.DGetFileSecurity(FileName, SecurityInformation, SecurityDescriptor, BufferLength, LengthNeeded, DokanFileInfo, FileContextId, m_sourceContextId);
}


NTSTATUS MountSource::DSetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) noexcept {
  return m_sourcePlugin.DSetFileSecurity(FileName, SecurityInformation, SecurityDescriptor, BufferLength, DokanFileInfo, FileContextId, m_sourceContextId);
}
