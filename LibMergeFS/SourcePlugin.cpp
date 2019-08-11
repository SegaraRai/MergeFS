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
  _Mount(dll.GetProc<PMount>("Mount")),
  _Unmount(dll.GetProc<PUnmount>("Unmount")),
  _ListFiles(dll.GetProc<PListFiles>("ListFiles")),
  _ListStreams(dll.GetProc<PListStreams>("ListStreams")),
  SIsSupported(dll.GetProc<PSIsSupported>("SIsSupported")),
  GetSourceInfo(dll.GetProc<PGetSourceInfo>("GetSourceInfo")),
  GetFileInfo(dll.GetProc<PGetFileInfo>("GetFileInfo")),
  GetDirectoryInfo(dll.GetProc<PGetDirectoryInfo>("GetDirectoryInfo")),
  RemoveFile(dll.GetProc<PRemoveFile>("RemoveFile")),
  ExportStart(dll.GetProc<PExportStart>("ExportStart")),
  ImportStart(dll.GetProc<PImportStart>("ImportStart")),
  ExportData(dll.GetProc<PExportData>("ExportData")),
  ImportData(dll.GetProc<PImportData>("ImportData")),
  ExportFinish(dll.GetProc<PExportFinish>("ExportFinish")),
  ImportFinish(dll.GetProc<PImportFinish>("ImportFinish")),
  SwitchSourceClose(dll.GetProc<PSwitchSourceClose>("SwitchSourceClose")),
  SwitchDestinationPrepare(dll.GetProc<PSwitchDestinationPrepare>("SwitchDestinationPrepare")),
  SwitchDestinationOpen(dll.GetProc<PSwitchDestinationOpen>("SwitchDestinationOpen")),
  SwitchDestinationCleanup(dll.GetProc<PSwitchDestinationCleanup>("SwitchDestinationCleanup")),
  SwitchDestinationClose(dll.GetProc<PSwitchDestinationClose>("SwitchDestinationClose")),
  DZwCreateFile(dll.GetProc<PDZwCreateFile>("DZwCreateFile")),
  DCleanup(dll.GetProc<PDCleanup>("DCleanup")),
  DCloseFile(dll.GetProc<PDCloseFile>("DCloseFile")),
  DReadFile(dll.GetProc<PDReadFile>("DReadFile")),
  DWriteFile(dll.GetProc<PDWriteFile>("DWriteFile")),
  DFlushFileBuffers(dll.GetProc<PDFlushFileBuffers>("DFlushFileBuffers")),
  DGetFileInformation(dll.GetProc<PDGetFileInformation>("DGetFileInformation")),
  DSetFileAttributes(dll.GetProc<PDSetFileAttributes>("DSetFileAttributes")),
  DSetFileTime(dll.GetProc<PDSetFileTime>("DSetFileTime")),
  DDeleteFile(dll.GetProc<PDDeleteFile>("DDeleteFile")),
  DDeleteDirectory(dll.GetProc<PDDeleteDirectory>("DDeleteDirectory")),
  DMoveFile(dll.GetProc<PDMoveFile>("DMoveFile")),
  DSetEndOfFile(dll.GetProc<PDSetEndOfFile>("DSetEndOfFile")),
  DSetAllocationSize(dll.GetProc<PDSetAllocationSize>("DSetAllocationSize")),
  DGetDiskFreeSpace(dll.GetProc<PDGetDiskFreeSpace>("DGetDiskFreeSpace")),
  DGetVolumeInformation(dll.GetProc<PDGetVolumeInformation>("DGetVolumeInformation")),
  DGetFileSecurity(dll.GetProc<PDGetFileSecurity>("DGetFileSecurity")),
  DSetFileSecurity(dll.GetProc<PDSetFileSecurity>("DSetFileSecurity"))
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


NTSTATUS SourcePlugin::Mount(const PLUGIN_INITIALIZE_MOUNT_INFO* InitializeMountInfo, SOURCE_CONTEXT_ID& sourceContextId) noexcept {
  try {
    const auto allocatedSourceContextId = AllocateSourceContextId();
    const auto status = _Mount(InitializeMountInfo, allocatedSourceContextId);
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
