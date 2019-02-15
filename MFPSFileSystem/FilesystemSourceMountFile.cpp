#define NOMINMAX

#include "FilesystemSourceMount.hpp"
#include "FilesystemSourceMountFile.hpp"
#include "Util.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <string>

#include <Windows.h>

using namespace std::literals;



NTSTATUS FilesystemSourceMountFile::Constructor(PDOKAN_FILE_INFO DokanFileInfo) {
  const auto csRealPath = realPath.c_str();

  ULONG shareAccess = this->argShareAccess;

  ACCESS_MASK userDesiredAccess;
  DWORD fileAttributesAndFlags;
  DWORD creationDisposition;
  GetPluginInitializeInfo().dokanFuncs.DokanMapKernelToUserCreateFileFlags(argDesiredAccess, argFileAttributes, argCreateOptions, argCreateDisposition, &userDesiredAccess, &fileAttributesAndFlags, &creationDisposition);

  SECURITY_ATTRIBUTES securityAttributes{
    sizeof(securityAttributes),
    argSecurityContext.AccessState.SecurityDescriptor,
    FALSE,
  };

  // When filePath is a directory, needs to change the flag so that the file can be opened.
  const DWORD fileAttributes = this->existingFileAttributes = GetFileAttributesW(csRealPath);

  if (DokanFileInfo->IsDirectory) {
    this->directory = true;

    // TODO: use ReOpenFile in MirrorFindFiles to set share read temporary
    shareAccess |= FILE_SHARE_READ;

    // It is a create directory request
    if (creationDisposition == CREATE_NEW || creationDisposition == OPEN_ALWAYS) {
      if (!CreateDirectoryW(csRealPath, &securityAttributes)) {
        if (GetLastError() != ERROR_ALREADY_EXISTS || creationDisposition == CREATE_NEW) {
          return NtstatusFromWin32();
        }
      }
    }

    // FILE_FLAG_BACKUP_SEMANTICS is required for opening directory handles
    this->hFile = CreateFileW(csRealPath, userDesiredAccess, shareAccess, &securityAttributes, OPEN_EXISTING, fileAttributesAndFlags | FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (this->hFile == INVALID_HANDLE_VALUE) {
      return NtstatusFromWin32();
    }
  } else {
    // It is a create file request
    this->directory = false;

    // Truncate should always be used with write access
    if (creationDisposition == TRUNCATE_EXISTING) {
      userDesiredAccess |= GENERIC_WRITE;
    }

    this->hFile = CreateFileW(csRealPath, userDesiredAccess, shareAccess, &securityAttributes, creationDisposition, fileAttributesAndFlags, NULL);
    if (this->hFile == INVALID_HANDLE_VALUE) {
      return NtstatusFromWin32();
    }

    // Need to update FileAttributes with previous when Overwrite file
    if (fileAttributes != INVALID_FILE_ATTRIBUTES && creationDisposition == TRUNCATE_EXISTING) {
      SetFileAttributesW(csRealPath, fileAttributesAndFlags | fileAttributes);
    }
  }

  return STATUS_SUCCESS;
}


HANDLE FilesystemSourceMountFile::GetFileHandle() {
  return hFile;
}


FilesystemSourceMountFile::FilesystemSourceMountFile(FilesystemSourceMount& sourceMount, LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId) :
  SourceMountFileBase(sourceMount, FileName, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, MaybeSwitched, FileContextId),
  sourceMount(sourceMount),
  realPath(sourceMount.GetRealPath(FileName))
{
  if (const NTSTATUS status = Constructor(DokanFileInfo); status != STATUS_SUCCESS) {
    throw NtstatusError(status);
  }
}


FilesystemSourceMountFile::FilesystemSourceMountFile(FilesystemSourceMount& sourceMount, LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) :
  SourceMountFileBase(sourceMount, FileName, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, FileContextId),
  sourceMount(sourceMount),
  realPath(sourceMount.GetRealPath(FileName))
{
  HANDLE preparedHandle = sourceMount.TransferSwitchDestinationHandle(FileContextId);
  if (IsValidHandle(preparedHandle)) {
    this->hFile = preparedHandle;

    BY_HANDLE_FILE_INFORMATION byHandleFileInformation;
    if (!GetFileInformationByHandle(this->hFile, &byHandleFileInformation)) {
      CloseHandle(this->hFile);
      this->hFile = NULL;
      throw Win32Error();
    }

    this->existingFileAttributes = byHandleFileInformation.dwFileAttributes;
    this->directory = byHandleFileInformation.dwFileAttributes != FILE_ATTRIBUTE_NORMAL && (byHandleFileInformation.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
  } else {
    if (const NTSTATUS status = Constructor(DokanFileInfo); status != STATUS_SUCCESS) {
      throw NtstatusError(status);
    }
  }
}


FilesystemSourceMountFile::~FilesystemSourceMountFile() {
  // in case
  if (IsValidHandle(hFile)) {
    CloseHandle(hFile);
    hFile = NULL;
  }
}


NTSTATUS FilesystemSourceMountFile::SwitchDestinationCleanupImpl(PDOKAN_FILE_INFO DokanFileInfo) {
  if (IsValidHandle(hFile)) {
    DokanFileInfo->DeleteOnClose = TRUE;
    DDeleteFile(DokanFileInfo);
  }
  DCleanupImpl(DokanFileInfo);
  return STATUS_SUCCESS;
}


NTSTATUS FilesystemSourceMountFile::SwitchDestinationCloseImpl(PDOKAN_FILE_INFO DokanFileInfo) {
  if (IsValidHandle(hFile)) {
    DokanFileInfo->DeleteOnClose = TRUE;
    DDeleteFile(DokanFileInfo);
  }
  DCloseFileImpl(DokanFileInfo);
  return STATUS_SUCCESS;
}


void FilesystemSourceMountFile::DCleanupImpl(PDOKAN_FILE_INFO DokanFileInfo) {
  // Cleanupの後にもGetFileInformationが呼ばれることがあったため、この時点ではCloseHandleしないこととした
  // そもそもCleanupとCloseFileのそれぞれでやるべき処理が分からない…
  /*
  if (IsValidHandle(hFile)) {
    CloseHandle(hFile);
    hFile = NULL;
  }
  */
}


void FilesystemSourceMountFile::DCloseFileImpl(PDOKAN_FILE_INFO DokanFileInfo) {
  if (IsValidHandle(hFile)) {
    CloseHandle(hFile);
    hFile = NULL;
    if (DokanFileInfo->DeleteOnClose) {
      // Should already be deleted by CloseHandle
      // if open with FILE_FLAG_DELETE_ON_CLOSE
      if (DokanFileInfo->IsDirectory) {
        RemoveDirectoryW(realPath.c_str());
      } else {
        DeleteFileW(realPath.c_str());
      }
    }
  }
}


NTSTATUS FilesystemSourceMountFile::DReadFile(LPVOID Buffer, DWORD BufferLength, LPDWORD ReadLength, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo) {
  if (!IsValidHandle(hFile)) {
    return STATUS_INVALID_HANDLE;
  }
  if (!SetFilePointerEx(hFile, CreateLargeInteger(Offset), NULL, FILE_BEGIN)) {
    return NtstatusFromWin32();
  }
  return NtstatusFromWin32Api(ReadFile(hFile, Buffer, BufferLength, ReadLength, NULL));
}


NTSTATUS FilesystemSourceMountFile::DWriteFile(LPCVOID Buffer, DWORD NumberOfBytesToWrite, LPDWORD NumberOfBytesWritten, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo) {
  if (!IsValidHandle(hFile)) {
    return STATUS_INVALID_HANDLE;
  }
  // seek
  if (DokanFileInfo->WriteToEndOfFile) {
    if (!SetFilePointerEx(hFile, CreateLargeInteger(0), NULL, FILE_END)) {
      return NtstatusFromWin32();
    }
  } else {
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
      return NtstatusFromWin32();
    }
    if (DokanFileInfo->PagingIo) {
      // Paging IO cannot write after allocate file size.
      const auto ullOffset = static_cast<ULONGLONG>(Offset);
      const auto ullFileSize = static_cast<ULONGLONG>(fileSize.QuadPart);
      if (ullOffset >= ullFileSize) {
        *NumberOfBytesWritten = 0;
        return STATUS_SUCCESS;
      }
      if (ullOffset + NumberOfBytesToWrite > ullFileSize) {
        const auto writableBytes = fileSize.QuadPart - ullOffset;
        NumberOfBytesToWrite = static_cast<DWORD>(std::min<ULONGLONG>(writableBytes, static_cast<ULONGLONG>(std::numeric_limits<DWORD>::max())));
      }
    }
    if (!SetFilePointerEx(hFile, CreateLargeInteger(Offset), NULL, FILE_BEGIN)) {
      return NtstatusFromWin32();
    }
  }
  // write
  return NtstatusFromWin32Api(WriteFile(hFile, Buffer, NumberOfBytesToWrite, NumberOfBytesWritten, NULL));
}


NTSTATUS FilesystemSourceMountFile::DFlushFileBuffers(PDOKAN_FILE_INFO DokanFileInfo) {
  if (!IsValidHandle(hFile)) {
    return STATUS_INVALID_HANDLE;
  }
  return NtstatusFromWin32Api(FlushFileBuffers(hFile));
}


NTSTATUS FilesystemSourceMountFile::DGetFileInformation(LPBY_HANDLE_FILE_INFORMATION Buffer, PDOKAN_FILE_INFO DokanFileInfo) {
  if (!IsValidHandle(hFile)) {
    return STATUS_INVALID_HANDLE;
  }
  return NtstatusFromWin32Api(GetFileInformationByHandle(hFile, Buffer));
}


NTSTATUS FilesystemSourceMountFile::DSetFileAttributes(DWORD FileAttributes, PDOKAN_FILE_INFO DokanFileInfo) {
  if (!IsValidHandle(hFile)) {
    return STATUS_INVALID_HANDLE;
  }
  if (!FileAttributes) {
    // see [MS-FSCC]: File Attributes - https://msdn.microsoft.com/en-us/library/cc232110.aspx
    return STATUS_SUCCESS;
  }
  return NtstatusFromWin32Api(SetFileAttributesW(realPath.c_str(), FileAttributes));
}


NTSTATUS FilesystemSourceMountFile::DSetFileTime(const FILETIME* CreationTime, const FILETIME* LastAccessTime, const FILETIME* LastWriteTime, PDOKAN_FILE_INFO DokanFileInfo) {
  if (!IsValidHandle(hFile)) {
    return STATUS_INVALID_HANDLE;
  }
  return NtstatusFromWin32Api(SetFileTime(hFile, CreationTime, LastAccessTime, LastWriteTime));
}


NTSTATUS FilesystemSourceMountFile::DDeleteFile(PDOKAN_FILE_INFO DokanFileInfo) {
  if (!IsValidHandle(hFile)) {
    return STATUS_INVALID_HANDLE;
  }
  if (directory) {
    return STATUS_ACCESS_DENIED;
  }
  FILE_DISPOSITION_INFO fileDispositionInfo{
    DokanFileInfo->DeleteOnClose,
  };
  return NtstatusFromWin32Api(SetFileInformationByHandle(hFile, FileDispositionInfo, &fileDispositionInfo, sizeof(fileDispositionInfo)));
}


NTSTATUS FilesystemSourceMountFile::DDeleteDirectory(PDOKAN_FILE_INFO DokanFileInfo) {
  if (!IsValidHandle(hFile)) {
    return STATUS_INVALID_HANDLE;
  }
  if (!DokanFileInfo->DeleteOnClose) {
    return STATUS_SUCCESS;
  }
  return sourceMount.GetDirectoryInfo(filename.c_str());
}


NTSTATUS FilesystemSourceMountFile::DMoveFile(LPCWSTR NewFileName, BOOL ReplaceIfExisting, PDOKAN_FILE_INFO DokanFileInfo) {
  if (!IsValidHandle(hFile)) {
    return STATUS_INVALID_HANDLE;
  }

  const std::wstring newRealPath = sourceMount.GetRealPath(NewFileName);

  const std::size_t fileRenameInfoBufferSize = sizeof(FILE_RENAME_INFO) + newRealPath.size() * sizeof(wchar_t);

  auto fileRenameInfoBuffer = std::make_unique<char[]>(fileRenameInfoBufferSize);
  auto ptrFileRenameInfo = reinterpret_cast<FILE_RENAME_INFO*>(fileRenameInfoBuffer.get());

  *ptrFileRenameInfo = {
    static_cast<BOOLEAN>(ReplaceIfExisting),
    NULL,
    static_cast<DWORD>(newRealPath.size() * sizeof(wchar_t)),
  };

  std::memcpy(&ptrFileRenameInfo->FileName, newRealPath.c_str(), (newRealPath.size() + 1) * sizeof(wchar_t));

  return NtstatusFromWin32Api(SetFileInformationByHandle(hFile, FileRenameInfo, ptrFileRenameInfo, static_cast<DWORD>(fileRenameInfoBufferSize)));
}


NTSTATUS FilesystemSourceMountFile::DSetEndOfFile(LONGLONG ByteOffset, PDOKAN_FILE_INFO DokanFileInfo) {
  if (!IsValidHandle(hFile)) {
    return STATUS_INVALID_HANDLE;
  }
  if (!SetFilePointerEx(hFile, CreateLargeInteger(ByteOffset), NULL, FILE_BEGIN)) {
    return NtstatusFromWin32();
  }
  return NtstatusFromWin32Api(SetEndOfFile(hFile));
}


NTSTATUS FilesystemSourceMountFile::DSetAllocationSize(LONGLONG AllocSize, PDOKAN_FILE_INFO DokanFileInfo) {
  if (!IsValidHandle(hFile)) {
    return STATUS_INVALID_HANDLE;
  }
  // check if AllocSize if smaller than the file size
  LARGE_INTEGER fileSize;
  if (!GetFileSizeEx(hFile, &fileSize)) {
    return NtstatusFromWin32();
  }
  if (AllocSize >= fileSize.QuadPart) {
    // Do nothing
    return STATUS_SUCCESS;
  }
  //
  if (!SetFilePointerEx(hFile, CreateLargeInteger(AllocSize), NULL, FILE_BEGIN)) {
    return NtstatusFromWin32();
  }
  return NtstatusFromWin32Api(SetEndOfFile(hFile));
}


NTSTATUS FilesystemSourceMountFile::DGetFileSecurity(PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PULONG LengthNeeded, PDOKAN_FILE_INFO DokanFileInfo) {
  if (!IsValidHandle(hFile)) {
    return STATUS_INVALID_HANDLE;
  }
  // TODO
  return STATUS_NOT_IMPLEMENTED;
}


NTSTATUS FilesystemSourceMountFile::DSetFileSecurity(PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PDOKAN_FILE_INFO DokanFileInfo) {
  if (!IsValidHandle(hFile)) {
    return STATUS_INVALID_HANDLE;
  }
  // TODO
  return STATUS_NOT_IMPLEMENTED;
}
