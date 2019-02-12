#pragma once

#define FROMLIBMERGEFS

#include "../dokan/dokan/dokan.h"

#include "PluginBase.hpp"

#include "../SDK/Plugin/Source.h"

#include <functional>
#include <string>
#include <string_view>
#include <unordered_set>


class SourcePlugin : public PluginBase {
public:
  using SOURCE_CONTEXT_ID = ::SOURCE_CONTEXT_ID;
  using FILE_CONTEXT_ID = ::FILE_CONTEXT_ID;
  using SOURCE_INFO = ::SOURCE_INFO;
  using PORTATION_INFO = ::PORTATION_INFO;

  using ListFilesUserCallback = std::function<void(PWIN32_FIND_DATAW FindDataW)>;
  using ListStreamsUserCallback = std::function<void(PWIN32_FIND_STREAM_DATA FindStreamData)>;

  static constexpr SOURCE_CONTEXT_ID SOURCE_CONTEXT_ID_NULL = ::SOURCE_CONTEXT_ID_NULL;
  static constexpr FILE_CONTEXT_ID FILE_CONTEXT_ID_NULL = ::FILE_CONTEXT_ID_NULL;

  /*
  class ExportedFileRAII {
    SourcePlugin& sourcePlugin;
    const SOURCE_CONTEXT_ID sourceContextId;
    EXPORTED_FILE* ptrExportedFile;

  public:
    ExportedFileRAII(ExportedFileRAII&&) = delete;
    ExportedFileRAII(const ExportedFileRAII&) = delete;

    ExportedFileRAII(SourcePlugin& sourcePlugin, SOURCE_CONTEXT_ID sourceContextId, LPCWSTR FileName, BOOL Empty);
    virtual ~ExportedFileRAII();

    const EXPORTED_FILE* GetExportedFile() const noexcept;
    operator const EXPORTED_FILE*() const noexcept;
  };
  //*/

private:
  static constexpr SOURCE_CONTEXT_ID SourceContextIdStart = SOURCE_CONTEXT_ID_NULL + 1;

  using CALLBACK_CONTEXT = ::CALLBACK_CONTEXT;
  using PListFilesCallback = ::PListFilesCallback;
  using PListStreamsCallback = ::PListStreamsCallback;

  using PListFiles = decltype(&External::Plugin::Source::ListFiles);
  using PListStreams = decltype(&External::Plugin::Source::ListStreams);

public:
  using PSIsSupported = decltype(&External::Plugin::Source::SIsSupported);

  using PMount = decltype(&External::Plugin::Source::Mount);
  using PUnmount = decltype(&External::Plugin::Source::Unmount);
  using PGetSourceInfo = decltype(&External::Plugin::Source::GetSourceInfo);
  using PGetFileInfo = decltype(&External::Plugin::Source::GetFileInfo);
  using PGetDirectoryInfo = decltype(&External::Plugin::Source::GetDirectoryInfo);
  using PRemoveFile = decltype(&External::Plugin::Source::RemoveFile);
  using PExportStart = decltype(&External::Plugin::Source::ExportStart);
  using PExportData = decltype(&External::Plugin::Source::ExportData);
  using PExportFinish = decltype(&External::Plugin::Source::ExportFinish);
  using PImportStart = decltype(&External::Plugin::Source::ImportStart);
  using PImportData = decltype(&External::Plugin::Source::ImportData);
  using PImportFinish = decltype(&External::Plugin::Source::ImportFinish);
  using PSwitchSourceClose = decltype(&External::Plugin::Source::SwitchSourceClose);
  using PSwitchDestinationPrepare = decltype(&External::Plugin::Source::SwitchDestinationPrepare);
  using PSwitchDestinationOpen = decltype(&External::Plugin::Source::SwitchDestinationOpen);
  using PSwitchDestinationCleanup = decltype(&External::Plugin::Source::SwitchDestinationCleanup);
  using PSwitchDestinationClose = decltype(&External::Plugin::Source::SwitchDestinationClose);

  using PDZwCreateFile = decltype(&External::Plugin::Source::DZwCreateFile);
  using PDCleanup = decltype(&External::Plugin::Source::DCleanup);
  using PDCloseFile = decltype(&External::Plugin::Source::DCloseFile);
  using PDReadFile = decltype(&External::Plugin::Source::DReadFile);
  using PDWriteFile = decltype(&External::Plugin::Source::DWriteFile);
  using PDFlushFileBuffers = decltype(&External::Plugin::Source::DFlushFileBuffers);
  using PDGetFileInformation = decltype(&External::Plugin::Source::DGetFileInformation);
  using PDSetFileAttributes = decltype(&External::Plugin::Source::DSetFileAttributes);
  using PDSetFileTime = decltype(&External::Plugin::Source::DSetFileTime);
  using PDDeleteFile = decltype(&External::Plugin::Source::DDeleteFile);
  using PDDeleteDirectory = decltype(&External::Plugin::Source::DDeleteDirectory);
  using PDMoveFile = decltype(&External::Plugin::Source::DMoveFile);
  using PDSetEndOfFile = decltype(&External::Plugin::Source::DSetEndOfFile);
  using PDSetAllocationSize = decltype(&External::Plugin::Source::DSetAllocationSize);
  using PDGetDiskFreeSpace = decltype(&External::Plugin::Source::DGetDiskFreeSpace);
  using PDGetVolumeInformation = decltype(&External::Plugin::Source::DGetVolumeInformation);
  using PDGetFileSecurity = decltype(&External::Plugin::Source::DGetFileSecurity);
  using PDSetFileSecurity = decltype(&External::Plugin::Source::DSetFileSecurity);

private:
  static void WINAPI ListFilesCallback(PWIN32_FIND_DATAW FindDataW, void* CallbackContext) noexcept;
  static void WINAPI ListStreamsCallback(PWIN32_FIND_STREAM_DATA FindStreamData, void* CallbackContext) noexcept;

  std::unordered_set<SOURCE_CONTEXT_ID> m_usedSourceContextIdSet;
  SOURCE_CONTEXT_ID m_nextSourceContextId = SourceContextIdStart;

  const PMount _Mount;
  const PUnmount _Unmount;
  const PListFiles _ListFiles;
  const PListStreams _ListStreams;

  SOURCE_CONTEXT_ID AllocateSourceContextId();
  bool ReleaseSourceContextId(SOURCE_CONTEXT_ID sourceContextId);

public:
  const PSIsSupported SIsSupported;

  const PGetSourceInfo GetSourceInfo;
  const PGetFileInfo GetFileInfo;
  const PGetDirectoryInfo GetDirectoryInfo;
  const PRemoveFile RemoveFile;
  const PExportStart ExportStart;
  const PExportData ExportData;
  const PExportFinish ExportFinish;
  const PImportStart ImportStart;
  const PImportData ImportData;
  const PImportFinish ImportFinish;
  const PSwitchSourceClose SwitchSourceClose;
  const PSwitchDestinationPrepare SwitchDestinationPrepare;
  const PSwitchDestinationOpen SwitchDestinationOpen;
  const PSwitchDestinationCleanup SwitchDestinationCleanup;
  const PSwitchDestinationClose SwitchDestinationClose;

  const PDZwCreateFile DZwCreateFile;
  const PDCleanup DCleanup;
  const PDCloseFile DCloseFile;
  const PDReadFile DReadFile;
  const PDWriteFile DWriteFile;
  const PDFlushFileBuffers DFlushFileBuffers;
  const PDGetFileInformation DGetFileInformation;
  const PDSetFileAttributes DSetFileAttributes;
  const PDSetFileTime DSetFileTime;
  const PDDeleteFile DDeleteFile;
  const PDDeleteDirectory DDeleteDirectory;
  const PDMoveFile DMoveFile;
  const PDSetEndOfFile DSetEndOfFile;
  const PDSetAllocationSize DSetAllocationSize;
  const PDGetDiskFreeSpace DGetDiskFreeSpace;
  const PDGetVolumeInformation DGetVolumeInformation;
  const PDGetFileSecurity DGetFileSecurity;
  const PDSetFileSecurity DSetFileSecurity;

  SourcePlugin(std::wstring_view pluginFilePath);

  NTSTATUS Mount(LPCWSTR FileName, SOURCE_CONTEXT_ID& sourceContextId) noexcept;
  BOOL Unmount(SOURCE_CONTEXT_ID sourceContextId) noexcept;
  NTSTATUS ListFiles(LPCWSTR FileName, ListFilesUserCallback Callback, SOURCE_CONTEXT_ID SourceContextId) noexcept;
  NTSTATUS ListStreams(LPCWSTR FileName, ListStreamsUserCallback Callback, SOURCE_CONTEXT_ID SourceContextId) noexcept;
};
