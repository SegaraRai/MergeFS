// for source plugin developers:
// - create SouceCppReadonly.cpp
//   - in SourceCppReadonly.cpp, include dokan.h, then include this file
// - or set include path

#pragma once


#ifndef DOKAN_VERSION
# if __has_include(<dokan/dokan.h>)
#  include <dokan/dokan.h>
# else
#  error include dokan.h
# endif
#endif

#include "SourceCppReadonly.hpp"

#include <memory>

#include <Windows.h>



namespace {
  constexpr NTSTATUS ReadonlyErrorStatus = STATUS_MEDIA_WRITE_PROTECTED;
}



// ReadonlySourceMountFileBase
////////////////////////////////////////////////////////////////////////////////////////////////////

ReadonlySourceMountFileBase::ReadonlySourceMountFileBase(SourceMountBase& sourceMountBase, LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, BOOL MaybeSwitched, FILE_CONTEXT_ID FileContextId) :
  SourceMountFileBase(sourceMountBase, FileName, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, MaybeSwitched, FileContextId)
{}


NTSTATUS ReadonlySourceMountFileBase::ImportStart(PORTATION_INFO* PortationInfo) {
  return ReadonlyErrorStatus;
}


NTSTATUS ReadonlySourceMountFileBase::ImportData(PORTATION_INFO* PortationInfo) {
  return ReadonlyErrorStatus;
}


NTSTATUS ReadonlySourceMountFileBase::ImportFinish(PORTATION_INFO* PortationInfo, BOOL Success) {
  return ReadonlyErrorStatus;
}


NTSTATUS ReadonlySourceMountFileBase::SwitchDestinationCleanup(PDOKAN_FILE_INFO DokanFileInfo) {
  return STATUS_SUCCESS;
}


NTSTATUS ReadonlySourceMountFileBase::SwitchDestinationClose(PDOKAN_FILE_INFO DokanFileInfo) {
  return STATUS_SUCCESS;
}


NTSTATUS ReadonlySourceMountFileBase::DWriteFile(LPCVOID Buffer, DWORD NumberOfBytesToWrite, LPDWORD NumberOfBytesWritten, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo) {
  return ReadonlyErrorStatus;
}


NTSTATUS ReadonlySourceMountFileBase::DFlushFileBuffers(PDOKAN_FILE_INFO DokanFileInfo) {
  return STATUS_SUCCESS;
}


NTSTATUS ReadonlySourceMountFileBase::DSetFileAttributes(DWORD FileAttributes, PDOKAN_FILE_INFO DokanFileInfo) {
  return ReadonlyErrorStatus;
}


NTSTATUS ReadonlySourceMountFileBase::DSetFileTime(const FILETIME* CreationTime, const FILETIME* LastAccessTime, const FILETIME* LastWriteTime, PDOKAN_FILE_INFO DokanFileInfo) {
  return ReadonlyErrorStatus;
}


NTSTATUS ReadonlySourceMountFileBase::DDeleteFile(PDOKAN_FILE_INFO DokanFileInfo) {
  return ReadonlyErrorStatus;
}


NTSTATUS ReadonlySourceMountFileBase::DDeleteDirectory(PDOKAN_FILE_INFO DokanFileInfo) {
  return ReadonlyErrorStatus;
}


NTSTATUS ReadonlySourceMountFileBase::DMoveFile(LPCWSTR NewFileName, BOOL ReplaceIfExisting, PDOKAN_FILE_INFO DokanFileInfo) {
  return ReadonlyErrorStatus;
}


NTSTATUS ReadonlySourceMountFileBase::DSetEndOfFile(LONGLONG ByteOffset, PDOKAN_FILE_INFO DokanFileInfo) {
  return ReadonlyErrorStatus;
}


NTSTATUS ReadonlySourceMountFileBase::DSetAllocationSize(LONGLONG AllocSize, PDOKAN_FILE_INFO DokanFileInfo) {
  return ReadonlyErrorStatus;
}


NTSTATUS ReadonlySourceMountFileBase::DSetFileSecurity(PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PDOKAN_FILE_INFO DokanFileInfo) {
  return ReadonlyErrorStatus;
}



// ReadonlySourceMountBase
////////////////////////////////////////////////////////////////////////////////////////////////////

ReadonlySourceMountBase::ReadonlySourceMountBase(const PLUGIN_INITIALIZE_MOUNT_INFO* InitializeMountInfo, SOURCE_CONTEXT_ID sourceContextId) :
  SourceMountBase(InitializeMountInfo, sourceContextId)
{}


NTSTATUS ReadonlySourceMountBase::RemoveFile(LPCWSTR FileName) {
  return ReadonlyErrorStatus;
}


NTSTATUS ReadonlySourceMountBase::SwitchDestinationPrepareImpl(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  return ReadonlyErrorStatus;
}


NTSTATUS ReadonlySourceMountBase::SwitchDestinationCleanupImpl(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  return STATUS_SUCCESS;
}


NTSTATUS ReadonlySourceMountBase::SwitchDestinationCloseImpl(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  return STATUS_SUCCESS;
}


NTSTATUS ReadonlySourceMountBase::ImportStartImpl(PORTATION_INFO* PortationInfo) {
  return ReadonlyErrorStatus;
}


NTSTATUS ReadonlySourceMountBase::ImportDataImpl(PORTATION_INFO* PortationInfo) {
  return ReadonlyErrorStatus;
}


NTSTATUS ReadonlySourceMountBase::ImportFinishImpl(PORTATION_INFO* PortationInfo, BOOL Success) {
  return ReadonlyErrorStatus;
}


std::unique_ptr<SourceMountFileBase> ReadonlySourceMountBase::SwitchDestinationOpenImpl(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo, FILE_CONTEXT_ID FileContextId) {
  throw NtstatusError(ReadonlyErrorStatus);
}
