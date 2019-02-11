#include "SourcePlugin.hpp"
#include "NsError.hpp"

#include <stdexcept>

using namespace std::literals;



void WINAPI SourcePlugin::ListFilesCallback(PWIN32_FIND_DATAW FindDataW, void* CallbackContext) noexcept {
  try {
    (*static_cast<ListFilesUserCallback*>(CallbackContext))(FindDataW);
  } catch (...) {}
}


void WINAPI SourcePlugin::ListStreamsCallback(PWIN32_FIND_STREAM_DATA FindStreamData, void* CallbackContext) noexcept {
  try {
    (*static_cast<ListStreamsUserCallback*>(CallbackContext))(FindStreamData);
  } catch (...) {}
}


SourcePlugin::SourcePlugin(std::wstring_view pluginFilePath) :
  PluginBase(pluginFilePath, PluginBase::PLUGIN_TYPE::Source),
  _Mount(dll.GetProc<PMount>(u8"Mount")),
  _Unmount(dll.GetProc<PUnmount>(u8"Unmount")),
  _ListFiles(dll.GetProc<PListFiles>(u8"ListFiles")),
  _ListStreams(dll.GetProc<PListStreams>(u8"ListStreams")),
  SIsSupportedFile(dll.GetProc<PSIsSupportedFile>(u8"SIsSupportedFile")),
  GetSourceInfo(dll.GetProc<PGetSourceInfo>(u8"GetSourceInfo")),
  GetFileInfo(dll.GetProc<PGetFileInfo>(u8"GetFileInfo")),
  GetDirectoryInfo(dll.GetProc<PGetDirectoryInfo>(u8"GetDirectoryInfo")),
  RemoveFile(dll.GetProc<PRemoveFile>(u8"RemoveFile")),
  ExportStart(dll.GetProc<PExportStart>(u8"ExportStart")),
  ImportStart(dll.GetProc<PImportStart>(u8"ImportStart")),
  ExportData(dll.GetProc<PExportData>(u8"ExportData")),
  ImportData(dll.GetProc<PImportData>(u8"ImportData")),
  ExportFinish(dll.GetProc<PExportFinish>(u8"ExportFinish")),
  ImportFinish(dll.GetProc<PImportFinish>(u8"ImportFinish")),
  SwitchSourceClose(dll.GetProc<PSwitchSourceClose>(u8"SwitchSourceClose")),
  SwitchDestinationPrepare(dll.GetProc<PSwitchDestinationPrepare>(u8"SwitchDestinationPrepare")),
  SwitchDestinationOpen(dll.GetProc<PSwitchDestinationOpen>(u8"SwitchDestinationOpen")),
  SwitchDestinationCleanup(dll.GetProc<PSwitchDestinationCleanup>(u8"SwitchDestinationCleanup")),
  SwitchDestinationClose(dll.GetProc<PSwitchDestinationClose>(u8"SwitchDestinationClose")),
  DZwCreateFile(dll.GetProc<PDZwCreateFile>(u8"DZwCreateFile")),
  DCleanup(dll.GetProc<PDCleanup>(u8"DCleanup")),
  DCloseFile(dll.GetProc<PDCloseFile>(u8"DCloseFile")),
  DReadFile(dll.GetProc<PDReadFile>(u8"DReadFile")),
  DWriteFile(dll.GetProc<PDWriteFile>(u8"DWriteFile")),
  DFlushFileBuffers(dll.GetProc<PDFlushFileBuffers>(u8"DFlushFileBuffers")),
  DGetFileInformation(dll.GetProc<PDGetFileInformation>(u8"DGetFileInformation")),
  DSetFileAttributes(dll.GetProc<PDSetFileAttributes>(u8"DSetFileAttributes")),
  DSetFileTime(dll.GetProc<PDSetFileTime>(u8"DSetFileTime")),
  DDeleteFile(dll.GetProc<PDDeleteFile>(u8"DDeleteFile")),
  DDeleteDirectory(dll.GetProc<PDDeleteDirectory>(u8"DDeleteDirectory")),
  DMoveFile(dll.GetProc<PDMoveFile>(u8"DMoveFile")),
  DSetEndOfFile(dll.GetProc<PDSetEndOfFile>(u8"DSetEndOfFile")),
  DSetAllocationSize(dll.GetProc<PDSetAllocationSize>(u8"DSetAllocationSize")),
  DGetDiskFreeSpace(dll.GetProc<PDGetDiskFreeSpace>(u8"DGetDiskFreeSpace")),
  DGetVolumeInformation(dll.GetProc<PDGetVolumeInformation>(u8"DGetVolumeInformation")),
  DGetFileSecurity(dll.GetProc<PDGetFileSecurity>(u8"DGetFileSecurity")),
  DSetFileSecurity(dll.GetProc<PDSetFileSecurity>(u8"DSetFileSecurity"))
{}


SourcePlugin::SOURCE_CONTEXT_ID SourcePlugin::AllocateSourceContextId() {
  const auto sourceContextId = m_nextSourceContextId;
  m_usedSourceContextIdSet.emplace(sourceContextId);
  do {
    m_nextSourceContextId++;
  } while (m_usedSourceContextIdSet.count(m_nextSourceContextId));
  return sourceContextId;
}


bool SourcePlugin::ReleaseSourceContextId(SOURCE_CONTEXT_ID sourceContextId) {
  if (sourceContextId < SourceContextIdStart) {
    return false;
  }
  if (sourceContextId < m_nextSourceContextId) {
    m_nextSourceContextId = sourceContextId;
  }
  return m_usedSourceContextIdSet.erase(sourceContextId);
}


NTSTATUS SourcePlugin::Mount(LPCWSTR FileName, SOURCE_CONTEXT_ID& sourceContextId) noexcept {
  try {
    const auto allocatedSourceContextId = AllocateSourceContextId();
    const auto status = _Mount(FileName, allocatedSourceContextId);
    if (status == STATUS_SUCCESS) {
      sourceContextId = allocatedSourceContextId;
    } else {
      ReleaseSourceContextId(allocatedSourceContextId);
    }
    return status;
  } catch (std::bad_alloc&) {
    return STATUS_NO_MEMORY;
  } catch (...) {
    return STATUS_UNSUCCESSFUL;
  }
}


BOOL SourcePlugin::Unmount(SOURCE_CONTEXT_ID sourceContextId) noexcept {
  try {
    ReleaseSourceContextId(sourceContextId);
    return _Unmount(sourceContextId);
  } catch (...) {
    return FALSE;
  }
}


NTSTATUS SourcePlugin::ListFiles(LPCWSTR FileName, ListFilesUserCallback Callback, SOURCE_CONTEXT_ID SourceContextId) noexcept {
  try {
    return _ListFiles(FileName, ListFilesCallback, &Callback, SourceContextId);
  } catch (std::bad_alloc&) {
    return STATUS_NO_MEMORY;
  } catch (...) {
    return STATUS_UNSUCCESSFUL;
  }
}


NTSTATUS SourcePlugin::ListStreams(LPCWSTR FileName, ListStreamsUserCallback Callback, SOURCE_CONTEXT_ID SourceContextId) noexcept {
  try {
    return _ListStreams(FileName, ListStreamsCallback, &Callback, SourceContextId);
  } catch (std::bad_alloc&) {
    return STATUS_NO_MEMORY;
  } catch (...) {
    return STATUS_UNSUCCESSFUL;
  }
}
