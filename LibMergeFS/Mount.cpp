#define NOMINMAX

#include "Mount.hpp"
#include "NsError.hpp"
#include "Util.hpp"
#include "DokanConfig.hpp"
#include "DokanOperations.hpp"

#include "../Util/VirtualFs.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using namespace std::literals;



std::shared_mutex Mount::gFileContextMapMutex;
std::unordered_map<Mount::FileContext*, std::shared_ptr<Mount::FileContext>> Mount::gFileContextPtrToSharedPtrMap;



namespace {
  template<typename T>
  NTSTATUS WrapException(const T& func) noexcept {
	  static_assert(std::is_same_v<decltype(func()), NTSTATUS>);

    try {
      return func();
    } catch (std::invalid_argument&) {
      return STATUS_INVALID_PARAMETER;
    } catch (std::domain_error&) {
      return STATUS_INVALID_PARAMETER;
    } catch (std::length_error&) {
      return STATUS_INVALID_PARAMETER;
    } catch (std::out_of_range&) {
      return STATUS_ACCESS_VIOLATION;
    } catch (std::bad_optional_access&) {
      return STATUS_ACCESS_VIOLATION;
    } catch (std::range_error&) {
      return STATUS_INVALID_PARAMETER;
    } catch (std::ios_base::failure&) {
      return STATUS_IO_DEVICE_ERROR;
    } catch (std::bad_typeid&) {
      return STATUS_ACCESS_VIOLATION;
    } catch (std::bad_cast&) {
      return STATUS_ACCESS_VIOLATION;
    } catch (std::bad_weak_ptr&) {
      return STATUS_ACCESS_VIOLATION;
    } catch (std::bad_function_call&) {
      return STATUS_ACCESS_VIOLATION;
    } catch (std::bad_alloc&) {
      return STATUS_NO_MEMORY;
    } catch (NsError& nsError) {
      return nsError;
    } catch (...) {
      return STATUS_UNSUCCESSFUL;
    }
  }


  template<typename T>
  NTSTATUS WrapExceptionV(const T& func) noexcept {
	  static_assert(std::is_same_v<decltype(func()), void>);

    return WrapException([&func]() -> NTSTATUS {
      func();
      return STATUS_SUCCESS;
    });
  }


  std::vector<ULONGLONG> CalcFileIndexBases(std::size_t numMounts) {
    const ULONGLONG diff = std::numeric_limits<ULONGLONG>::max() / numMounts;

    std::vector<ULONGLONG> fileIndexBases(numMounts);

    ULONGLONG base = 0;
    for (auto& fileIndexBase : fileIndexBases) {
      fileIndexBase = base;
      base += diff;
    }

    return fileIndexBases;
  }
}



Mount::DokanMainError::DokanMainError(int dokanMainResult) :
  std::runtime_error("DokanMain returned code "s + std::to_string(dokanMainResult)),
  result(dokanMainResult)
{}


int Mount::DokanMainError::GetError() const {
  return result;
}



NTSTATUS Mount::FileContext::UpdateLastAccessTime() {
  if (writable || !autoUpdateLastAccessTime) {
    return STATUS_SUCCESS;
  }

  // edit metadata
  SYSTEMTIME currentSystemtime;
  GetSystemTime(&currentSystemtime);
  FILETIME currentFiletime;
  if (!SystemTimeToFileTime(&currentSystemtime, &currentFiletime)) {
    return __NTSTATUS_FROM_WIN32(GetLastError());
  }

  {
    std::lock_guard lock(mount.m_metadataMutex);
    auto metadata = mount.m_metadataStore.GetMetadata2R(resolvedFilename);
    metadata.lastAccessTime = currentFiletime;
    mount.m_metadataStore.SetMetadataR(resolvedFilename, metadata);
  }

  return STATUS_SUCCESS;
}


NTSTATUS Mount::FileContext::UpdateLastWriteTime() {
  if (writable || !autoUpdateLastWriteTime) {
    return STATUS_SUCCESS;
  }

  // edit metadata
  SYSTEMTIME currentSystemtime;
  GetSystemTime(&currentSystemtime);
  FILETIME currentFiletime;
  if (!SystemTimeToFileTime(&currentSystemtime, &currentFiletime)) {
    return __NTSTATUS_FROM_WIN32(GetLastError());
  }

  {
    std::lock_guard lock(mount.m_metadataMutex);
    auto metadata = mount.m_metadataStore.GetMetadata2R(resolvedFilename);
    metadata.lastWriteTime = currentFiletime;
    mount.m_metadataStore.SetMetadataR(resolvedFilename, metadata);
  }

  return STATUS_SUCCESS;
}



bool Mount::HasFileContext(PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  return DokanFileInfo->Context;
}


Mount::FileContext* Mount::GetFileContextPtr(PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  return reinterpret_cast<FileContext*>(DokanFileInfo->Context);
}


#ifdef USE_SHARED_PTR_FOR_FILE_CONTEXT
std::shared_ptr<Mount::FileContext> Mount::GetFileContextSharedPtr(PDOKAN_FILE_INFO DokanFileInfo) {
  auto ptrFileContext = GetFileContextPtr(DokanFileInfo);
  if (!ptrFileContext) {
    throw NsError(STATUS_INVALID_HANDLE);
  }
  std::shared_lock lock(gFileContextMapMutex);
  if (!gFileContextPtrToSharedPtrMap.count(ptrFileContext)) {
    throw NsError(STATUS_INVALID_HANDLE);
  }
  return gFileContextPtrToSharedPtrMap.at(ptrFileContext);
}
#else
Mount::FileContext* Mount::GetFileContextSharedPtr(PDOKAN_FILE_INFO DokanFileInfo) {
  return GetFileContextPtr(DokanFileInfo);
}
#endif


/*
Mount::FileContext& Mount::GetFileContext(PDOKAN_FILE_INFO DokanFileInfo) {
  auto fileContextPtr = GetFileContextPtr(DokanFileInfo);
  if (!fileContextPtr) {
    throw NsError(STATUS_INVALID_HANDLE);
  }
  return *fileContextPtr;
}
//*/


NTSTATUS Mount::TransportR(std::wstring_view path, bool empty, FILE_CONTEXT_ID fileContextId, MountSource& source, MountSource& destination) {
  const std::wstring sPath(path);
  const auto csPath = sPath.c_str();

  PORTATION_INFO portationInfo{
    csPath,
    fileContextId,
    empty ? TRUE : FALSE,
  };

  if (const auto status = source.ExportStart(&portationInfo); status != STATUS_SUCCESS) {
    return status;
  }

  if (const auto status = destination.ImportStart(&portationInfo); status != STATUS_SUCCESS) {
    source.ExportFinish(&portationInfo, false);
    return status;
  }

  if (!empty && !portationInfo.directory) {
    while (true) {
      const auto statusS = source.ExportData(&portationInfo);
      if (statusS == STATUS_ALREADY_COMPLETE) {
        break;
      }
      if (statusS != STATUS_SUCCESS) {
        source.ExportFinish(&portationInfo, false);
        destination.ImportFinish(&portationInfo, false);
        return statusS;
      }

      if (const auto statusD = destination.ImportData(&portationInfo); statusD != STATUS_SUCCESS) {
        return statusD;
      }
    }
  }

  if (const auto status = destination.ImportFinish(&portationInfo, true); status != STATUS_SUCCESS) {
    source.ExportFinish(&portationInfo, false);
    return status;
  }

  if (const auto status = source.ExportFinish(&portationInfo, true); status != STATUS_SUCCESS) {
    return status;
  }

  return STATUS_SUCCESS;
}


std::wstring Mount::FilenameToKey(std::wstring_view filename) const {
  return ::FilenameToKey(filename, m_caseSensitive);
}


Mount::Mount(std::wstring_view mountPoint, bool writable, std::wstring_view metadataFileName, bool deferCopyEnabled, bool caseSensitive, std::vector<std::unique_ptr<MountSource>>&& sources, std::function<void(int)> callback) :
  m_imdMutex(),
  m_imdCv(),
  m_imdState(ImdState::Pending),
  m_imdResult(DOKAN_SUCCESS),
  m_mutex(),
  m_metadataMutex(),
  m_mountPoint(mountPoint),
  m_mountSources(std::move(sources)),
  m_topSource(*m_mountSources[0].get()),
  m_writable(writable && m_topSource.GetSourceInfo().writable),
  m_metadataFileName(m_writable ? metadataFileName : L""sv),
  m_deferCopyEnabled(deferCopyEnabled),
  m_caseSensitive(caseSensitive),
  m_metadataStore(m_metadataFileName, caseSensitive),
  m_mounted(false),
  m_fileContextMap(),
  m_minimumUnusedFileContextId(FileContextIdStart),
  m_fileIndexBases(CalcFileIndexBases(m_mountSources.size())),
  m_thread([this, callback]() {
    // TODO: make customizable
    ULONG options = DokanConfig::Options;
#ifdef _DEBUG
    options |= DOKAN_OPTION_DEBUG;
#endif
    if (!m_writable) {
      options |= DOKAN_OPTION_WRITE_PROTECT;
    }
    DOKAN_OPTIONS config = {
      DokanConfig::Version,
      DokanConfig::ThreadCount,
      options,
      GetGlobalContextFromMount(this),
      m_mountPoint.c_str(),
      nullptr,
      DokanConfig::Timeout,
      DokanConfig::AllocationUnitSize,
      DokanConfig::SectorSize,
    };
    DOKAN_OPERATIONS operations = gDokanOperations;
    m_mounted = true;
    const auto ret = DokanMain(&config, &operations);
    m_mounted = false;

    {
      std::lock_guard lock(m_imdMutex);
      if (m_imdState == ImdState::Mounting) {
        if (callback) {
          callback(ret);
        }
      }
      m_imdState = ImdState::Finished;
      m_imdResult = ret;
    }
    m_imdCv.notify_one();
  })
{
  std::unique_lock lock(m_imdMutex);
  m_imdCv.wait(lock, [this]() {
    return m_imdState != ImdState::Pending;
  });
  if (m_imdState == ImdState::Finished) {
    assert(m_imdResult != DOKAN_SUCCESS);
    m_thread.join();
    throw DokanMainError(m_imdResult);
  }
}


Mount::~Mount() {
  Unmount();

  std::unique_lock lock(m_imdMutex);
  m_imdCv.wait(lock, [this]() {
    return m_imdState == ImdState::Finished;
  });

  m_thread.join();
}


bool Mount::IsWritable() const {
  return m_writable;
}


bool Mount::SafeUnmount() {
  if (!m_mounted) {
    return true;
  }
  if (!DokanRemoveMountPoint(m_mountPoint.c_str())) {
    return false;
  }
  m_mounted = false;
  return true;
}


bool Mount::Unmount() {
  if (!m_mounted) {
    return true;
  }
#if DOKAN_VERSION >= 122
  if (!DokanRemoveMountPointEx(m_mountPoint.c_str(), FALSE)) {
#else
  if (!DokanRemoveMountPoint(m_mountPoint.c_str())) {
#endif
    return false;
  }
  m_mounted = false;
  return true;
}


std::optional<std::wstring> Mount::ResolveFilepathN(std::wstring_view filename) {
  return m_metadataStore.ResolveFilepath(filename);
}


std::wstring Mount::ResolveFilepath(std::wstring_view filename) {
  const auto resolvedFilenameN = ResolveFilepathN(filename);
  if (!resolvedFilenameN) {
    throw W32Error(ERROR_FILE_NOT_FOUND);
  }
  return resolvedFilenameN.value();
}


std::optional<std::size_t> Mount::GetMountSourceIndexR(std::wstring_view resolvedFilename) {
  if (util::vfs::IsRootDirectory(resolvedFilename)) {
    return TopSourceIndex;
  }

  // 祖先ディレクトリの正当性を確認する
  // 祖先ディレクトリがここより優先度の高いソースにおいてファイルとして存在しているか、
  // メタデータにより削除済みとマークされている場合は対象のオブジェクトは存在しない
  // ここでは直属の親ディレクトリに対してGetMountSourceIndexを呼び出して確認することで再帰的にチェックしている
  const std::wstring parentFilename(util::vfs::GetParentPath(resolvedFilename));
  const auto index = GetMountSourceIndexR(parentFilename);
  if (!index || m_mountSources[index.value()]->GetFileType(parentFilename.c_str()) != FileType::Directory) {
    return std::nullopt;
  }

  // メタデータにより削除済みとマークされている場合は存在しない
  {
    std::shared_lock lock(m_metadataMutex);
    if (!m_metadataStore.ExistsR(resolvedFilename)) {
      return std::nullopt;
    }
  }

  // それ以外の場合、一番上位のソースに存在するものを探す
  const std::wstring sResolvedFilename(resolvedFilename);
  for (std::size_t i = 0; i < m_mountSources.size(); i++) {
    const auto& mountSource = *m_mountSources[i];
    if (mountSource.GetFileType(sResolvedFilename.c_str()) != FileType::Inexistent) {
      return i;
    }
  }

  // 見つからなかった
  return std::nullopt;
}


std::optional<std::size_t> Mount::GetMountSourceIndex(std::wstring_view filename) {
  const auto resolvedFilenameN = ResolveFilepathN(filename);
  if (!resolvedFilenameN) {
    return std::nullopt;
  }
  return GetMountSourceIndexR(resolvedFilenameN.value());
}


bool Mount::FileExists(std::wstring_view filename) {
  return GetMountSourceIndex(filename).has_value();
}


Mount::FileType Mount::GetFileTypeR(std::wstring_view resolvedFilename) {
  if (util::vfs::IsRootDirectory(resolvedFilename)) {
    return FileType::Directory;
  }
  const auto index = GetMountSourceIndexR(resolvedFilename);
  if (!index) {
    return FileType::Inexistent;
  }
  const std::wstring sResolvedFilename(resolvedFilename);
  return m_mountSources[index.value()]->GetFileType(sResolvedFilename.c_str());
}


Mount::FileType Mount::GetFileType(std::wstring_view filename) {
  const auto resolvedFilenameN = ResolveFilepathN(filename);
  if (!resolvedFilenameN) {
    return FileType::Inexistent;
  }
  return GetFileTypeR(resolvedFilenameN.value());
}


void Mount::CopyFileToTopSourceR(std::wstring_view resolvedFilename, bool empty, FILE_CONTEXT_ID fileContextId) {
  if (!m_writable) {
    throw NsError(STATUS_MEDIA_WRITE_PROTECTED);
  }

  /*
  if (fileContextId != FILE_CONTEXT_ID_NULL) {
    const auto sourceIndex = GetMountSourceIndexR(resolvedFilename);
    if (sourceIndex == TopSourceIndex) {
      const auto status = TransportR(resolvedFilename, empty, fileContextId, source, m_topSource);
      if (status != STATUS_SUCCESS) {
        throw NsError(status);
      }
      return;
    }
  }
  //*/
  
  std::size_t offset = 0;
  std::size_t firstCreation = 0;
  try {
    do {
      offset = resolvedFilename.find_first_of(L'\\', offset + 1);

      auto path = resolvedFilename.substr(0, offset);

      const auto sourceIndex = GetMountSourceIndexR(path);
      if (!sourceIndex) {
        throw NsError(STATUS_OBJECT_NAME_NOT_FOUND);
      }
      if (sourceIndex == TopSourceIndex) {
        if (offset == std::wstring_view::npos) {
          offset = 0;
          throw NsError(STATUS_OBJECT_NAME_COLLISION);
        }
        continue;
      }

      if (!firstCreation) {
        firstCreation = offset;
      }

      auto& source = *m_mountSources.at(sourceIndex.value());

      const auto status = TransportR(path, empty, fileContextId, source, m_topSource);
      if (status != STATUS_SUCCESS) {
        throw NsError(status);
      }
    } while (offset != std::wstring_view::npos);
  } catch (...) {
    // TODO: remove created directories
    throw;
  }
}


void Mount::CopyFileToTopSource(std::wstring_view filename, bool empty, FILE_CONTEXT_ID fileContextId) {
  CopyFileToTopSourceR(ResolveFilepath(filename), empty, fileContextId);
}


void Mount::RemoveFile(std::wstring_view filename) {
  const auto resolvedFileName = ResolveFilepath(filename);

  const auto sourceIndex = GetMountSourceIndexR(resolvedFileName);
  if (!sourceIndex) {
    throw NsError(STATUS_OBJECT_NAME_NOT_FOUND);
  }

  bool editMetadata = true;

  if (sourceIndex == TopSourceIndex) {
    if (const auto status = m_mountSources[sourceIndex.value()]->RemoveFile(resolvedFileName.c_str()); status != STATUS_SUCCESS) {
      throw NsError(status);
    }
    editMetadata = FileExists(filename);
  }

  if (editMetadata) {
    std::lock_guard lock(m_metadataMutex);
    m_metadataStore.Delete(filename);
  }
}


Mount::FILE_CONTEXT_ID Mount::AssignFileContextId(std::wstring_view FileName, std::wstring_view ResolvedFileName, PDOKAN_FILE_INFO DokanFileInfo, std::size_t mountSourceIndex, bool isDirectory, bool deferCopy, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions) {
  if (const auto fileContextPtr = GetFileContextPtr(DokanFileInfo)) {
    return fileContextPtr->id;
  }

  std::lock_guard lock(m_mutex);

  const FILE_CONTEXT_ID id = m_minimumUnusedFileContextId;
  do {
    m_minimumUnusedFileContextId++;
  } while (m_fileContextMap.count(m_minimumUnusedFileContextId));

  const bool writable = m_writable && mountSourceIndex == TopSourceIndex;
  auto ptrFileContext = new FileContext{
    std::shared_mutex(),
    id,
    *this,
    *m_mountSources[mountSourceIndex].get(),
    std::wstring(FileName),
    std::wstring(ResolvedFileName),
    isDirectory,
    writable,
    deferCopy,
    true,
    true,
    m_fileIndexBases.at(mountSourceIndex),
    *SecurityContext,
    DesiredAccess,
    FileAttributes,
    ShareAccess,
    CreateDisposition,
    CreateOptions,
  };
#ifdef USE_SHARED_PTR_FOR_FILE_CONTEXT
  std::lock_guard glock(gFileContextMapMutex);
  std::shared_ptr<FileContext> spFileContext(ptrFileContext);
  m_fileContextMap.emplace(id, spFileContext);
  gFileContextPtrToSharedPtrMap.emplace(ptrFileContext, spFileContext);
#else
  m_fileContextMap.emplace(id, std::move(std::unique_ptr<FileContext>(ptrFileContext)));
#endif
  DokanFileInfo->Context = reinterpret_cast<ULONG64>(ptrFileContext);

  return id;
}


bool Mount::ReleaseFileContextId(FILE_CONTEXT_ID FileContextId) noexcept {
  try {
    if (FileContextId < FileContextIdStart) {
      return false;
    }
    std::lock_guard lock(m_mutex);
    if (FileContextId < m_minimumUnusedFileContextId) {
      m_minimumUnusedFileContextId = FileContextId;
    }
#ifdef USE_SHARED_PTR_FOR_FILE_CONTEXT
# ifdef _DEBUG
    assert(m_fileContextMap.count(FileContextId));
    // use_count will be usually 3 (+1 for m_fileContextMap and +1 for gFileContextPtrToSharedPtrMap and +1 for ptrFileContext in DCloseFile)
    const std::size_t useCount = m_fileContextMap.at(FileContextId).use_count();
    //assert(useCount == 3);
    if (useCount != 3) {
      OutputDebugStringW((L"### FileContext "s + std::to_wstring(FileContextId) + L" released with use count "s + std::to_wstring(useCount) + L"\n"s).c_str());
    }
# endif
    std::lock_guard glock(gFileContextMapMutex);
    gFileContextPtrToSharedPtrMap.erase(m_fileContextMap.at(FileContextId).get());
#endif
    return m_fileContextMap.erase(FileContextId);
  } catch (...) {}
  return false;
}


bool Mount::ReleaseFileContextId(PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  try {
    if (!DokanFileInfo->Context) {
      return false;
    }
    const auto id = GetFileContextPtr(DokanFileInfo)->id;
    DokanFileInfo->Context = NULL;
    return ReleaseFileContextId(id);
  } catch (...) {}
  return false;
}


NTSTATUS Mount::TransportIfNeeded(PDOKAN_FILE_INFO DokanFileInfo) {
  auto ptrFileContext = GetFileContextSharedPtr(DokanFileInfo);
  auto& fileContext = *ptrFileContext;
  if (fileContext.copyDeferred) {
    std::lock_guard lock(fileContext.mutex);
    if (fileContext.copyDeferred) {
      auto& oldMountSource = fileContext.mountSource.get();
      // ensure parent directory
      const auto parentPath = util::vfs::GetParentPath(fileContext.resolvedFilename);
      if (GetMountSourceIndexR(parentPath) != TopSourceIndex) {
        CopyFileToTopSourceR(parentPath, true);
      }
      // retrieve original source
      const auto sourceIndex = GetMountSourceIndexR(fileContext.resolvedFilename);
      if (!sourceIndex) {
        return STATUS_OBJECT_NAME_NOT_FOUND;
      }
      if (sourceIndex == TopSourceIndex) {
        return STATUS_OBJECT_NAME_COLLISION;
      }
      auto& source = *m_mountSources.at(sourceIndex.value());
      if (const auto status = m_topSource.SwitchDestinationOpen(fileContext.resolvedFilename.c_str(), &fileContext.SecurityContext, fileContext.DesiredAccess, fileContext.FileAttributes, fileContext.ShareAccess, fileContext.CreateDisposition, fileContext.CreateOptions, DokanFileInfo, fileContext.id); status != STATUS_SUCCESS) {
        return status;
      }
      // transport
      try {
        if (const auto status = TransportR(fileContext.resolvedFilename, false, fileContext.id, source, m_topSource); status != STATUS_SUCCESS) {
          throw NsError(status);
        }
        //CopyFileToTopSourceR(fileContext.resolvedFilename, false, fileContext.id);
      } catch (...) {
        m_topSource.SwitchDestinationClose(fileContext.resolvedFilename.c_str(), DokanFileInfo, fileContext.id);
        throw;
      }
      if (const auto status = oldMountSource.SwitchSourceClose(fileContext.resolvedFilename.c_str(), DokanFileInfo, fileContext.id); status != STATUS_SUCCESS) {
        return status;
      }
      fileContext.mountSource = m_topSource;
      fileContext.copyDeferred = false;
      fileContext.writable = true;
    }
  }
  return STATUS_SUCCESS;
}


/*
CreateFile Dokan API callback.

CreateFile is called each time a request is made on a file system object.

In case OPEN_ALWAYS & CREATE_ALWAYS are successfully opening an existing file, STATUS_OBJECT_NAME_COLLISION should be returned instead of STATUS_SUCCESS . This will inform Dokan that the file has been opened and not created during the request.

If the file is a directory, CreateFile is also called. In this case, CreateFile should return STATUS_SUCCESS when that directory can be opened and DOKAN_FILE_INFO.IsDirectory has to be set to TRUE. On the other hand, if DOKAN_FILE_INFO.IsDirectory is set to TRUE but the path targets a file, STATUS_NOT_A_DIRECTORY must be returned.

DOKAN_FILE_INFO.Context can be used to store Data (like HANDLE) that can be retrieved in all other requests related to the Context. To avoid memory leak, Context needs to be released in DOKAN_OPERATIONS.Cleanup.
*/
NTSTATUS Mount::DZwCreateFile(LPCWSTR FileName, PDOKAN_IO_SECURITY_CONTEXT SecurityContext, ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions, PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  // CreateFileからCloseFileまでを完全に委譲できるのは
  // ・ファイルがmutateeに存在する場合
  // ・読み込み操作のみのとき

  return WrapException([=]() -> NTSTATUS {
    DokanFileInfo->Context = NULL;

    // 親ディレクトリが存在することを確認する
    if (!util::vfs::IsRootDirectory(FileName)) {
      if (GetFileType(util::vfs::GetParentPath(FileName)) != FileType::Directory) {
        return STATUS_OBJECT_PATH_NOT_FOUND;
      }
    }

    bool deferCopy = false;

    const auto resolvedFilenameN = ResolveFilepathN(FileName);

    auto sourceIndex = resolvedFilenameN ? GetMountSourceIndexR(resolvedFilenameN.value()) : std::nullopt;

    DWORD existingFileAttributes = INVALID_FILE_ATTRIBUTES;
    FileType existingFileType = FileType::Inexistent;

    /*
    DWORD creationDisposition;
    DWORD fileAttributesAndFlags;
    ACCESS_MASK genericDesiredAccess;

    DokanMapKernelToUserCreateFileFlags(DesiredAccess, FileAttributes, CreateOptions, CreateDisposition, &genericDesiredAccess, &fileAttributesAndFlags, &creationDisposition);
    //*/

    const bool hasDataReadOperation = DesiredAccess & (FILE_READ_DATA | READ_CONTROL | GENERIC_ALL | GENERIC_EXECUTE | GENERIC_READ);
    const bool hasDataWriteOperation = DesiredAccess & (FILE_WRITE_DATA | FILE_APPEND_DATA | GENERIC_ALL | GENERIC_WRITE);
    const bool hasMetadataWriteOperation = DesiredAccess & (FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA | GENERIC_ALL | GENERIC_WRITE | DELETE | WRITE_DAC | WRITE_OWNER);

    const bool willBeReplaced = CreateDisposition == FILE_SUPERSEDE || CreateDisposition == FILE_OVERWRITE || CreateDisposition == FILE_OVERWRITE_IF;

    if (sourceIndex) {
      const auto status = m_mountSources[sourceIndex.value()]->GetFileInfoAttributes(resolvedFilenameN.value().c_str(), &existingFileAttributes);
      if (status != STATUS_SUCCESS && status != STATUS_OBJECT_NAME_NOT_FOUND && status != STATUS_OBJECT_PATH_NOT_FOUND) {
        return status;
      }
      existingFileType = status == STATUS_OBJECT_NAME_NOT_FOUND || status == STATUS_OBJECT_PATH_NOT_FOUND ? FileType::Inexistent : MountSource::FileAttributesToFileType(existingFileAttributes);
    }

    const bool readonly = (existingFileAttributes != INVALID_FILE_ATTRIBUTES && existingFileAttributes != FILE_ATTRIBUTE_NORMAL && (existingFileAttributes & FILE_ATTRIBUTE_READONLY)) || (FileAttributes & FILE_ATTRIBUTE_READONLY);

    std::size_t targetSourceIndex = sourceIndex.value_or(TopSourceIndex);

    if (existingFileType == FileType::Inexistent) {
      // 存在しないファイルへの要求
      if (CreateDisposition == FILE_OPEN || CreateDisposition == FILE_OVERWRITE) {
        // TODO: STATUS_OBJECT_NAME_NOT_FOUNDとSTATUS_OBJECT_PATH_NOT_FOUNDの使い分け
        return STATUS_OBJECT_NAME_NOT_FOUND;
      }

      if (!m_writable) {
        // readonly
        return STATUS_MEDIA_WRITE_PROTECTED;
      }

      // メタデータを消しておく
      {
        std::lock_guard lock(m_metadataMutex);
        m_metadataStore.RemoveMetadata(FileName);
      }

      // mutateeに対して操作
      // 既にtargetSourceIndex == TopSourceIndex

      // 親ディレクトリをTopSource上に確保しておく
      // 後で
    } else {
      // 存在するファイルへの要求

      const bool directory = existingFileType == FileType::Directory;

      // 種別（ファイルかディレクトリか）チェック
      if (directory) {
        // 既にディレクトリとして存在している
        if (CreateOptions & FILE_NON_DIRECTORY_FILE) {
          // ファイルを開こうとしたが実際はディレクトリ
          return STATUS_FILE_IS_A_DIRECTORY;
        }
        DokanFileInfo->IsDirectory = TRUE;
      } else {
        // 既にファイルとして存在している
        if (CreateOptions & FILE_DIRECTORY_FILE) {
          // ディレクトリを開こうとしたが実際はファイル
          return STATUS_NOT_A_DIRECTORY;
        }
        // その指定がないのにHIDDENまたはSYSTEMなファイルの上書きは不可
        if (willBeReplaced && (((existingFileAttributes & FILE_ATTRIBUTE_HIDDEN) && !(FileAttributes & FILE_ATTRIBUTE_HIDDEN)) || ((existingFileAttributes & FILE_ATTRIBUTE_SYSTEM) && !(FileAttributes & FILE_ATTRIBUTE_SYSTEM)))) {
          return STATUS_ACCESS_DENIED;
        }
        // 読み取り専用ファイルの削除は不可
        if (readonly && (CreateOptions & FILE_DELETE_ON_CLOSE)) {
          return STATUS_CANNOT_DELETE;
        }
      }

      if (CreateDisposition == FILE_CREATE) {
        return STATUS_OBJECT_NAME_COLLISION;
      }

      if (hasDataWriteOperation || willBeReplaced) {
        // 書き込み要求がある場合、またはCreateDispositionの設定により既存のファイルが消去され得る場合
        // mutateeに対して操作

        if (!m_writable) {
          // read only
          return STATUS_MEDIA_WRITE_PROTECTED;
        }

        // 読み取り専用ファイルの書き込みは不可
        if (readonly) {
          return STATUS_ACCESS_DENIED;
        }

        if (sourceIndex != TopSourceIndex) {
          // 事前にmutateeにコピーしておく
          if (m_deferCopyEnabled && !directory && !willBeReplaced && !(CreateOptions & FILE_DELETE_ON_CLOSE)) {
            deferCopy = true;
          }
          
          if (!deferCopy) {
            CopyFileToTopSourceR(resolvedFilenameN.value(), willBeReplaced);
            targetSourceIndex = TopSourceIndex;
          }
        }
      }
    }

    auto& targetSource = *m_mountSources[targetSourceIndex];

    // resolvedFilenameが空の場合、指定された名前は既に存在している（ただし他の名前に変更済みであるため見かけ上存在しないことになっている）ので、
    // この名前でファイルを読み書きするには、まず何か他の名前を与えその移動先をこの名前として作成する必要がある
    // なお、祖先ディレクトリは事前に存在することを確認しているため、祖先ディレクトリが改名されたことによりresolvedFilenameが空になっている訳ではないことが保証される

    std::wstring resolvedFilename;

    if (resolvedFilenameN) {
      resolvedFilename = resolvedFilenameN.value();
    } else {
      const auto resolvedParentFilenameN = ResolveFilepathN(util::vfs::GetParentPath(FileName));
      if (!resolvedParentFilenameN) {
        // 一応
        assert(false);
        return STATUS_OBJECT_PATH_NOT_FOUND;
      }
      const auto baseFilename = (util::vfs::IsRootDirectory(resolvedParentFilenameN.value()) ? L"\\"s : resolvedParentFilenameN.value() + L"\\"s) + std::wstring(GetBaseName(FileName)) + L"."s;
      unsigned long i = 2;
      do {
        resolvedFilename = baseFilename + std::to_wstring(i);
        i++;
      } while (FileExists(resolvedFilename));
    }

    // 親ディレクトリをTopSource上に確保しておく
    if (existingFileType == FileType::Inexistent) {
      auto resolvedParentFilename = util::vfs::GetParentPath(resolvedFilename);
      const auto parentSourceIndex = GetMountSourceIndexR(resolvedParentFilename);
      if (!parentSourceIndex) {
        // 一応
        assert(false);
        return STATUS_OBJECT_PATH_NOT_FOUND;
      }
      if (parentSourceIndex != TopSourceIndex) {
        CopyFileToTopSourceR(resolvedParentFilename, true);
      }
    }

    const auto fileContextId = AssignFileContextId(FileName, resolvedFilename, DokanFileInfo, targetSourceIndex, DokanFileInfo->IsDirectory, deferCopy, SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateOptions, CreateDisposition);

    // CreateDispositionがOPEN_ALWAYSまたはCREATE_ALWAYS相当
    const bool createAlways = CreateDisposition == FILE_OPEN_IF || CreateDisposition == FILE_OVERWRITE_IF || CreateDisposition == FILE_SUPERSEDE;

    if (deferCopy) {
      // TODO: ensure path?
      const auto status = m_topSource.SwitchDestinationPrepare(resolvedFilename.c_str(), SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, fileContextId);
      if (status != STATUS_SUCCESS && (status != STATUS_OBJECT_NAME_COLLISION || !createAlways)) {
        return status;
      }
    }

    const auto status = targetSource.DZwCreateFile(resolvedFilename.c_str(), SecurityContext, DesiredAccess, FileAttributes, ShareAccess, CreateDisposition, CreateOptions, DokanFileInfo, deferCopy, fileContextId);

    if (status != STATUS_SUCCESS && (status != STATUS_OBJECT_NAME_COLLISION || !createAlways)) {
      return status;
    }

    if (!resolvedFilenameN) {
      // TODO: もっと効率良く書く
      std::lock_guard lock(m_metadataMutex);
      m_metadataStore.Rename(std::wstring(util::vfs::GetParentPath(FileName)) + std::wstring(GetBaseName(resolvedFilename)), FileName);
      //m_metadataStore.Rename(resolvedFilename, FileName);
    }

    if (existingFileType != FileType::Inexistent && createAlways) {
      // OPEN_ALWAYSまたはCREATE_ALWAYSが指定されている場合、既存のファイルを開いた（新たにファイルを作成しなかった）ときは
      // STATUS_SUCCESSではなくSTATUS_OBJECT_NAME_COLLISIONを返すことになっている（どのみち成功していることに変わりはない）
      return STATUS_OBJECT_NAME_COLLISION;
    }

    return STATUS_SUCCESS;
  });
}


/*
Cleanup Dokan API callback.

Cleanup request before CloseFile is called.

When DOKAN_FILE_INFO.DeleteOnClose is TRUE, the file in Cleanup must be deleted. See DeleteFile documentation for explanation.
*/
void Mount::DCleanup(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  try {
    if (!HasFileContext(DokanFileInfo)) {
      return;
    }
    auto ptrFileContext = GetFileContextSharedPtr(DokanFileInfo);
    auto& fileContext = *ptrFileContext;
    fileContext.mountSource.get().DCleanup(fileContext.resolvedFilename.c_str(), DokanFileInfo, fileContext.id);
    if (fileContext.copyDeferred) {
      m_topSource.SwitchDestinationCleanup(fileContext.resolvedFilename.c_str(), DokanFileInfo, fileContext.id);
    }
    if (DokanFileInfo->DeleteOnClose) {
      // Cleanup後CloseFile前にもファイルハンドルを要求されることがあるため、ここで削除するのが正しいかは分からない
      // が、ドキュメントによれば削除しろとのこと
      RemoveFile(FileName);
    }
    // メモリリークを避けるためにCleanupでContextを解放しろと書かれているが、この後CloseFileでも使うためここでは解放しない
    // メモリリークするかも
  } catch (...) {}
}


/*
CloseFile Dokan API callback.

Clean remaining Context

CloseFile is called at the end of the life of the context. Anything remaining in DOKAN_FILE_INFO::Context must be cleared before returning.
*/
void Mount::DCloseFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  try {
    if (!HasFileContext(DokanFileInfo)) {
      return;
    }
    auto ptrFileContext = GetFileContextSharedPtr(DokanFileInfo);
    auto& fileContext = *ptrFileContext;
    fileContext.mountSource.get().DCloseFile(fileContext.resolvedFilename.c_str(), DokanFileInfo, fileContext.id);
    if (fileContext.copyDeferred) {
      m_topSource.SwitchDestinationClose(fileContext.resolvedFilename.c_str(), DokanFileInfo, fileContext.id);
    }
    ReleaseFileContextId(DokanFileInfo);
  } catch (...) {}
}


/*
ReadFile Dokan API callback.

ReadFile callback on the file previously opened in DOKAN_OPERATIONS.ZwCreateFile. It can be called by different threads at the same time, so the read/context has to be thread safe.
*/
NTSTATUS Mount::DReadFile(LPCWSTR FileName, LPVOID Buffer, DWORD BufferLength, LPDWORD ReadLength, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  // MUST BE THEAD SAFE
  return WrapException([=]() -> NTSTATUS {
    if (!HasFileContext(DokanFileInfo)) {
      return STATUS_INVALID_HANDLE;
    }
    auto ptrFileContext = GetFileContextSharedPtr(DokanFileInfo);
    auto& fileContext = *ptrFileContext;
    std::shared_lock lock(fileContext.mutex);
    if (const auto status = fileContext.mountSource.get().DReadFile(fileContext.resolvedFilename.c_str(), Buffer, BufferLength, ReadLength, Offset, DokanFileInfo, fileContext.id); status != STATUS_SUCCESS) {
      return status;
    }
    /*
    if (const auto status = fileContext.UpdateLastAccessTime(); status != STATUS_SUCCESS) {
      return status;
    }
    //*/
    return STATUS_SUCCESS;
  });
}


/*
WriteFile Dokan API callback.

WriteFile callback on the file previously opened in DOKAN_OPERATIONS.ZwCreateFile It can be called by different threads at the same time, sp the write/context has to be thread safe.
*/
NTSTATUS Mount::DWriteFile(LPCWSTR FileName, LPCVOID Buffer, DWORD NumberOfBytesToWrite, LPDWORD NumberOfBytesWritten, LONGLONG Offset, PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  // MUST BE THEAD SAFE
  return WrapException([=]() -> NTSTATUS {
    if (!HasFileContext(DokanFileInfo)) {
      return STATUS_INVALID_HANDLE;
    }
    if (!m_writable) {
      return STATUS_MEDIA_WRITE_PROTECTED;
    }
    if (const auto status = TransportIfNeeded(DokanFileInfo); status != STATUS_SUCCESS) {
      return status;
    }
    auto ptrFileContext = GetFileContextSharedPtr(DokanFileInfo);
    auto& fileContext = *ptrFileContext;
    if (!fileContext.writable) {
      return STATUS_ACCESS_DENIED;
    }
    return fileContext.mountSource.get().DWriteFile(fileContext.resolvedFilename.c_str(), Buffer, NumberOfBytesToWrite, NumberOfBytesWritten, Offset, DokanFileInfo, fileContext.id);
  });
}


/*
FlushFileBuffers Dokan API callback.

Clears buffers for this context and causes any buffered data to be written to the file.
*/
NTSTATUS Mount::DFlushFileBuffers(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  return WrapException([=]() -> NTSTATUS {
    if (!HasFileContext(DokanFileInfo)) {
      return STATUS_INVALID_HANDLE;
    }
    if (!m_writable) {
      return STATUS_MEDIA_WRITE_PROTECTED;
    }
    auto ptrFileContext = GetFileContextSharedPtr(DokanFileInfo);
    auto& fileContext = *ptrFileContext;
    if (fileContext.copyDeferred) {
      return STATUS_SUCCESS;
    }
    if (!fileContext.writable) {
      return STATUS_ACCESS_DENIED;
    }
    return fileContext.mountSource.get().DFlushFileBuffers(fileContext.resolvedFilename.c_str(), DokanFileInfo, fileContext.id);
  });
}


/*
GetFileInformation Dokan API callback.

Get specific information on a file.
*/
NTSTATUS Mount::DGetFileInformation(LPCWSTR FileName, LPBY_HANDLE_FILE_INFORMATION Buffer, PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  return WrapException([=]() -> NTSTATUS {
    if (!HasFileContext(DokanFileInfo)) {
      return STATUS_INVALID_HANDLE;
    }
    auto ptrFileContext = GetFileContextSharedPtr(DokanFileInfo);
    auto& fileContext = *ptrFileContext;
    const auto& resolvedFilename = fileContext.resolvedFilename;
    const auto status = fileContext.mountSource.get().DGetFileInformation(resolvedFilename.c_str(), Buffer, DokanFileInfo, fileContext.id);
    if (status != STATUS_SUCCESS || fileContext.writable) {
      return status;
    }
  
    // modify fileIndex to avoid collision
    if (Buffer) {
      auto fileIndex = (static_cast<ULONGLONG>(Buffer->nFileIndexHigh) << 32) | Buffer->nFileIndexLow;
      fileIndex += fileContext.fileIndexBase;
      Buffer->nFileIndexHigh = (fileIndex >> 32) & 0xFFFFFFFF;
      Buffer->nFileIndexLow = fileIndex & 0xFFFFFFFF;
    }

    // read metadata if available
    if (Buffer) {
      std::shared_lock lock(m_metadataMutex);
      if (m_metadataStore.HasMetadataR(resolvedFilename)) {
        const auto& metadata = m_metadataStore.GetMetadataR(resolvedFilename);
        if (metadata.fileAttributes) {
          Buffer->dwFileAttributes = metadata.fileAttributes.value();
        }
        if (metadata.creationTime) {
          Buffer->ftCreationTime = metadata.creationTime.value();
        }
        if (metadata.lastAccessTime) {
          Buffer->ftLastAccessTime = metadata.lastAccessTime.value();
        }
        if (metadata.lastWriteTime) {
          Buffer->ftLastWriteTime = metadata.lastWriteTime.value();
        }
      }
    }

    return STATUS_SUCCESS;
  });
}


/*
FindFiles Dokan API callback.

List all files in the requested path DOKAN_OPERATIONS::FindFilesWithPattern is checked first. If it is not implemented or returns STATUS_NOT_IMPLEMENTED, then FindFiles is called, if implemented.
*/
NTSTATUS Mount::DFindFiles(LPCWSTR FileName, PFillFindData FillFindData, PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  return WrapException([=]() -> NTSTATUS {
    const bool isRootDirectory = util::vfs::IsRootDirectory(FileName);
    std::map<std::wstring, WIN32_FIND_DATAW> findDataMap;

    //const auto resolvedFilename = ResolveFilepath(FileName);
    auto ptrFileContext = GetFileContextSharedPtr(DokanFileInfo);
    auto& fileContext = *ptrFileContext;
    const auto& resolvedFilename = fileContext.resolvedFilename;

    //
    std::vector<std::pair<std::wstring, std::wstring>> excludeList;
    std::vector<std::pair<std::wstring, std::wstring>> includeList;
    {
      std::shared_lock lock(m_metadataMutex);
      excludeList = m_metadataStore.ListChildrenInReverseLookupTree(fileContext.filename);
      includeList = m_metadataStore.ListChildrenInForwardLookupTree(fileContext.filename);
    }

    std::unordered_set<std::wstring> excludeSet;
    for (const auto& [key, value] : excludeList) {
      excludeSet.emplace(FilenameToKey(key));
    }

    const auto resolvedDirectoryPrefix = resolvedFilename + L"\\";

    // list files
    bool isFirst = true;
    for (std::size_t i = 0; i < m_mountSources.size(); i++) {
      const bool canAddCurrentAndParentDirectory = !isRootDirectory && isFirst;

      auto& mountSource = *m_mountSources[i];
      NTSTATUS statusFromCallback = STATUS_SUCCESS;
      const auto status = mountSource.ListFiles(resolvedFilename.c_str(), [this, sourceIndex = i, &resolvedDirectoryPrefix, &excludeSet, &findDataMap, canAddCurrentAndParentDirectory, &statusFromCallback](PWIN32_FIND_DATAW ptrFindData) noexcept {
        if (statusFromCallback != STATUS_SUCCESS) {
          return;
        }
        if (!ptrFindData) {
          statusFromCallback = STATUS_ACCESS_VIOLATION;
          return;
        }
        statusFromCallback = WrapExceptionV([&]() {
          WIN32_FIND_DATAW findData(*ptrFindData);

          const std::wstring wsFileName(findData.cFileName);
          const std::wstring wsKey(FilenameToKey(wsFileName));
          if (!canAddCurrentAndParentDirectory && (wsFileName == L"."sv || wsFileName == L".."sv)) {
            return;
          }
          if (excludeSet.count(wsKey)) {
            return;
          }
          if (findDataMap.count(wsKey)) {
            return;
          }
          // refer metadata if available
          // excludeSetに登録されていないということは、このファイルはリネームされていない
          if (sourceIndex != TopSourceIndex) {
            std::shared_lock lock(m_metadataMutex);
            const auto resolvedFilepath = resolvedDirectoryPrefix + wsFileName;
            if (m_metadataStore.HasMetadataR(resolvedFilepath)) {
              const auto& metadata = m_metadataStore.GetMetadataR(resolvedFilepath);
              if (metadata.fileAttributes) {
                findData.dwFileAttributes = metadata.fileAttributes.value();
              }
              if (metadata.creationTime) {
                findData.ftCreationTime = metadata.creationTime.value();
              }
              if (metadata.lastAccessTime) {
                findData.ftLastAccessTime = metadata.lastAccessTime.value();
              }
              if (metadata.lastWriteTime) {
                findData.ftLastWriteTime = metadata.lastWriteTime.value();
              }
            }
          }
          findDataMap.emplace(wsKey, findData);
        });
      });
      if (status == STATUS_OBJECT_NAME_NOT_FOUND || status == STATUS_OBJECT_PATH_NOT_FOUND) {
        // ディレクトリの存在しないソースは無視
        continue;
      }
      if (!isFirst && status == STATUS_NOT_A_DIRECTORY) {
        // 2番目以降のソースでディレクトリ以外として存在している場合は無視
        continue;
      }
      if (status != STATUS_SUCCESS) {
        return status;
      }
      if (statusFromCallback != STATUS_SUCCESS) {
        return statusFromCallback;
      }
      isFirst = false;
    }

    if (isFirst) {
      // TODO: STATUS_OBJECT_NAME_NOT_FOUNDとSTATUS_OBJECT_PATH_NOT_FOUNDの使い分け
      return STATUS_OBJECT_PATH_NOT_FOUND;
    }


    //
    auto addObject = [this, &findDataMap](std::wstring_view filename, std::wstring_view resolvedFullPath, bool forceAddAsDirectory) -> NTSTATUS {
      const std::wstring wsKey = FilenameToKey(filename);
      if (findDataMap.count(wsKey)) {
        return STATUS_OBJECT_NAME_COLLISION;
      }

      WIN32_FILE_ATTRIBUTE_DATA Win32FileAttributeData{
        FILE_ATTRIBUTE_DIRECTORY,
        {0, 0},
        {0, 0},
        {0, 0},
        0,
        0,
      };

      const auto sourceIndex = GetMountSourceIndexR(resolvedFullPath);
      if (!forceAddAsDirectory && !sourceIndex) {
        // TODO: STATUS_OBJECT_NAME_NOT_FOUNDとSTATUS_OBJECT_PATH_NOT_FOUNDの使い分け
        return STATUS_OBJECT_NAME_NOT_FOUND;
      }

      const std::wstring sResolvedFullPath(resolvedFullPath);
      if (const auto status = m_mountSources[sourceIndex.value()]->GetFileInfo(sResolvedFullPath.c_str(), &Win32FileAttributeData); status != STATUS_SUCCESS) {
        return status;
      }

      if (sourceIndex != TopSourceIndex) {
        std::shared_lock lock(m_metadataMutex);
        if (m_metadataStore.HasMetadataR(resolvedFullPath)) {
          const auto& metadata = m_metadataStore.GetMetadataR(resolvedFullPath);
          if (metadata.fileAttributes) {
            Win32FileAttributeData.dwFileAttributes = metadata.fileAttributes.value();
          }
          if (metadata.creationTime) {
            Win32FileAttributeData.ftCreationTime = metadata.creationTime.value();
          }
          if (metadata.lastAccessTime) {
            Win32FileAttributeData.ftLastAccessTime = metadata.lastAccessTime.value();
          }
          if (metadata.lastWriteTime) {
            Win32FileAttributeData.ftLastWriteTime = metadata.lastWriteTime.value();
          }
        }
      }

      Win32FileAttributeData.dwFileAttributes &= ~static_cast<DWORD>(FILE_ATTRIBUTE_REPARSE_POINT);

      WIN32_FIND_DATAW findData = {
        Win32FileAttributeData.dwFileAttributes,
        Win32FileAttributeData.ftCreationTime,
        Win32FileAttributeData.ftLastAccessTime,
        Win32FileAttributeData.ftLastWriteTime,
        Win32FileAttributeData.nFileSizeHigh,
        Win32FileAttributeData.nFileSizeLow,
        0,    // ?
        0,    // ?
      };
      const std::size_t size = std::min(sizeof(findData.cFileName) / sizeof(wchar_t) - 1, filename.size());
      memcpy(findData.cFileName, filename.data(), size * sizeof(wchar_t));
      findData.cFileName[size] = L'\0';
      findData.cAlternateFileName[0] = L'\0';

      findDataMap.emplace(wsKey, findData);

      return STATUS_SUCCESS;
    };

    // add files in includeList
    for (const auto& [key, value] : includeList) {
      if (const auto status = addObject(key, value, false); status != STATUS_SUCCESS) {
        return status;
      }
    }

    // add . and .. for non-root directory
    if (!isRootDirectory) {
      if (const auto status = addObject(L"."sv, resolvedFilename, true); status != STATUS_SUCCESS && status != STATUS_OBJECT_NAME_COLLISION) {
        return status;
      }
      if (const auto status = addObject(L".."sv, util::vfs::GetParentPath(resolvedFilename), true); status != STATUS_SUCCESS && status != STATUS_OBJECT_NAME_COLLISION) {
        return status;
      }
    }

    // call callback
    for (auto& [key, findData] : findDataMap) {
      FillFindData(&findData, DokanFileInfo);
    }

    return STATUS_SUCCESS;
  });
}


/*
SetFileAttributes Dokan API callback.

Set file attributes on a specific file
*/
NTSTATUS Mount::DSetFileAttributes(LPCWSTR FileName, DWORD FileAttributes, PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  return WrapException([=]() -> NTSTATUS {
    if (!HasFileContext(DokanFileInfo)) {
      return STATUS_INVALID_HANDLE;
    }
    if (!m_writable) {
      return STATUS_MEDIA_WRITE_PROTECTED;
    }
    auto ptrFileContext = GetFileContextSharedPtr(DokanFileInfo);
    auto& fileContext = *ptrFileContext;
    const auto& resolvedFilename = fileContext.resolvedFilename;
    if (fileContext.writable) {
      return fileContext.mountSource.get().DSetFileAttributes(resolvedFilename.c_str(), FileAttributes, DokanFileInfo, fileContext.id);
    }
  
    // edit metadata
    {
      const auto filteredFileAttributes = (FileAttributes & FILE_ATTRIBUTE_NORMAL) && (FileAttributes != FILE_ATTRIBUTE_NORMAL)
        ? FileAttributes & ~static_cast<DWORD>(FILE_ATTRIBUTE_NORMAL)
        : FileAttributes;
      std::lock_guard lock(m_metadataMutex);
      auto metadata = m_metadataStore.GetMetadata2R(resolvedFilename);
      metadata.fileAttributes = filteredFileAttributes;
      m_metadataStore.SetMetadataR(resolvedFilename, metadata);
    }

    /*
    if (const auto status = fileContext.UpdateLastWriteTime(); status != STATUS_SUCCESS) {
      return status;
    }
    //*/

    return STATUS_SUCCESS;
  });
}


/*
SetFileTime Dokan API callback.

Set file attributes on a specific file
*/
NTSTATUS Mount::DSetFileTime(LPCWSTR FileName, const FILETIME *CreationTime, const FILETIME *LastAccessTime, const FILETIME *LastWriteTime, PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  return WrapException([=]() -> NTSTATUS {
    if (!HasFileContext(DokanFileInfo)) {
      return STATUS_INVALID_HANDLE;
    }
    if (!m_writable) {
      return STATUS_MEDIA_WRITE_PROTECTED;
    }
    auto ptrFileContext = GetFileContextSharedPtr(DokanFileInfo);
    auto& fileContext = *ptrFileContext;
    const auto& resolvedFilename = fileContext.resolvedFilename;
    if (fileContext.writable) {
      return fileContext.mountSource.get().DSetFileTime(resolvedFilename.c_str(), CreationTime, LastAccessTime, LastWriteTime, DokanFileInfo, fileContext.id);
    }

    constexpr auto IsZeroFiletime = [](const FILETIME& filetime) -> bool {
      return filetime.dwLowDateTime == 0 && filetime.dwHighDateTime == 0;
    };

    constexpr auto IsMinusOneFiletime = [](const FILETIME& filetime) -> bool {
      return filetime.dwLowDateTime == 0xFFFFFFFF && filetime.dwHighDateTime == 0xFFFFFFFF;
    };

    // edit metadata
    {
      std::lock_guard lock(m_metadataMutex);
      auto metadata = m_metadataStore.GetMetadata2R(resolvedFilename);
      if (CreationTime && !IsZeroFiletime(*CreationTime)) {
        metadata.creationTime = *CreationTime;
      }
      if (LastAccessTime && !IsZeroFiletime(*LastAccessTime)) {
        if (!IsMinusOneFiletime(*LastAccessTime)) {
          metadata.lastAccessTime = *LastAccessTime;
        } else {
          fileContext.autoUpdateLastAccessTime = false;
        }
      }
      if (LastWriteTime && !IsZeroFiletime(*LastWriteTime)) {
        if (!IsMinusOneFiletime(*LastAccessTime)) {
          metadata.lastWriteTime = *LastWriteTime;
        } else {
          fileContext.autoUpdateLastWriteTime = false;
        }
      }
      m_metadataStore.SetMetadataR(resolvedFilename, metadata);
    }

    return STATUS_SUCCESS;
  });
}


/*
DeleteFile Dokan API callback.

Check if it is possible to delete a file.

DeleteFile will also be called with DOKAN_FILE_INFO.DeleteOnClose set to FALSE to notify the driver when the file is no longer requested to be deleted.

The file in DeleteFile should not be deleted, but instead the file must be checked as to whether or not it can be deleted, and STATUS_SUCCESS should be returned (when it can be deleted) or appropriate error codes, such as STATUS_ACCESS_DENIED or STATUS_OBJECT_NAME_NOT_FOUND, should be returned.

When STATUS_SUCCESS is returned, a Cleanup call is received afterwards with DOKAN_FILE_INFO.DeleteOnClose set to TRUE. Only then must the closing file be deleted.
*/
NTSTATUS Mount::DDeleteFile(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  // DeleteFile、DeleteDirectoryコールバックは、主にファイル削除時に呼ばれますが、その名前に反して対象が削除可能かどうかの確認のみを行います（削除処理は行いません）
  // この関数がSTATUS_SUCCESSを返すと、DOKAN_FILE_INFO.DeleteOnCloseがTRUEの状態でCleanupコールバックが呼ばれるので、そのCleanup関数内で削除処理を行います
  // また、この関数はファイル削除時以外に、ファイル削除が取り消されたときにも呼ばれます。そのとき、DokanFileInfo->DeleteOnCloseがFALSEとなっています
  // https://dokan-dev.github.io/dokany-doc/html/struct_d_o_k_a_n___o_p_e_r_a_t_i_o_n_s.html

  return WrapException([=]() -> NTSTATUS {
    if (!HasFileContext(DokanFileInfo)) {
      return STATUS_INVALID_HANDLE;
    }
    if (!m_writable) {
      return STATUS_MEDIA_WRITE_PROTECTED;
    }
    auto ptrFileContext = GetFileContextSharedPtr(DokanFileInfo);
    auto& fileContext = *ptrFileContext;
    if (fileContext.directory) {
      return STATUS_ACCESS_DENIED;
    }
    if (fileContext.writable) {
      return fileContext.mountSource.get().DDeleteFile(fileContext.resolvedFilename.c_str(), DokanFileInfo, fileContext.id);
    }
    // メタデータの編集だけで済むので、常に成功する
    return STATUS_SUCCESS;
  });
}


/*
DeleteDirectory Dokan API callback.

Check if it is possible to delete a directory.

DeleteDirectory will also be called with DOKAN_FILE_INFO.DeleteOnClose set to FALSE to notify the driver when the file is no longer requested to be deleted.

The Directory in DeleteDirectory should not be deleted, but instead must be checked as to whether or not it can be deleted, and STATUS_SUCCESS should be returned (when it can be deleted) or appropriate error codes, such as STATUS_ACCESS_DENIED, STATUS_OBJECT_PATH_NOT_FOUND, or STATUS_DIRECTORY_NOT_EMPTY, should be returned.

When STATUS_SUCCESS is returned, a Cleanup call is received afterwards with DOKAN_FILE_INFO.DeleteOnClose set to TRUE. Only then must the closing file be deleted.
*/
NTSTATUS Mount::DDeleteDirectory(LPCWSTR FileName, PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  // DeleteDirectory、DeleteFileコールバックは、主にファイル削除時に呼ばれますが、その名前に反して対象が削除可能かどうかの確認のみを行います（削除処理は行いません）
  // この関数がSTATUS_SUCCESSを返すと、DOKAN_FILE_INFO->DeleteOnCloseがTRUEの状態でCleanupコールバックが呼ばれるので、そのCleanup関数内で削除処理を行います
  // また、この関数はファイル削除時以外に、ファイル削除が取り消されたときにも呼ばれます。そのとき、DokanFileInfo->DeleteOnCloseがFALSEとなっています
  // https://dokan-dev.github.io/dokany-doc/html/struct_d_o_k_a_n___o_p_e_r_a_t_i_o_n_s.html

  return WrapException([=]() -> NTSTATUS {
    if (!HasFileContext(DokanFileInfo)) {
      return STATUS_INVALID_HANDLE;
    }
    if (!m_writable) {
      return STATUS_MEDIA_WRITE_PROTECTED;
    }
    auto ptrFileContext = GetFileContextSharedPtr(DokanFileInfo);
    auto& fileContext = *ptrFileContext;
    if (!fileContext.directory) {
      return STATUS_ACCESS_DENIED;
    }
    if (fileContext.writable) {
      return fileContext.mountSource.get().DDeleteDirectory(fileContext.resolvedFilename.c_str(), DokanFileInfo, fileContext.id);
    }
    // メタデータの編集だけで済むので、常に成功する
    return STATUS_SUCCESS;
  });
}


/*
MoveFile Dokan API callback.

Move a file or directory to a new destination
*/
NTSTATUS Mount::DMoveFile(LPCWSTR FileName, LPCWSTR NewFileName, BOOL ReplaceIfExisting, PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  return WrapException([=]() -> NTSTATUS {
    if (!HasFileContext(DokanFileInfo)) {
      return STATUS_INVALID_HANDLE;
    }
    if (!m_writable) {
      return STATUS_MEDIA_WRITE_PROTECTED;
    }
    // ルートディレクトリに対しての操作は無効
    if (util::vfs::IsRootDirectory(FileName)) {
      return STATUS_ACCESS_DENIED;
    }
    if (util::vfs::IsRootDirectory(NewFileName)) {
      return STATUS_ACCESS_DENIED;
    }
    auto ptrFileContext = GetFileContextSharedPtr(DokanFileInfo);
    auto& fileContext = *ptrFileContext;
    // 既に存在するか確認
    if (!ReplaceIfExisting && FileExists(NewFileName)) {
      return STATUS_OBJECT_NAME_COLLISION;
    }
    // リネーム先の親ディレクトリの実パスを確認
    // ついでに親ディレクトリが存在していることも確認
    const auto resolvedParentNewFilenameN = ResolveFilepathN(util::vfs::GetParentPath(NewFileName));
    if (!resolvedParentNewFilenameN) {
      return STATUS_OBJECT_PATH_NOT_FOUND;
    }
    //
    const auto& resolvedFilename = fileContext.resolvedFilename;
    // 元ファイルがTopSourceにあり、かつ下位ソースにも存在するディレクトリでなければ直接MoveFileを行う
    // fileContext.writableはファイルがTopSourceにあることも保証する（TopSource以外書き込まれないので）
    if (fileContext.writable) {
      bool directlyRenamable = true;
      if (fileContext.directory) {
        for (std::size_t i = TopSourceIndex + 1; i < m_mountSources.size(); i++) {
          auto& mountSource = *m_mountSources[i];
          if (mountSource.GetFileType(resolvedFilename.c_str()) == FileType::Directory) {
            directlyRenamable = false;
            break;
          }
        }
      }
      if (directlyRenamable) {
        const bool isOriginalFilenameRenamed = m_metadataStore.ExistsO(fileContext.filename) == true;
        if (isOriginalFilenameRenamed) {
          // リネーム元ファイル名が既にリネーム済みである場合、そのエントリを削除する必要がある
          // TODO: エラーチェック
          std::lock_guard lock(m_metadataMutex);
          m_metadataStore.RemoveRenameEntry(fileContext.filename);
        }
        const auto resolvedNewFileNameN = ResolveFilepathN(NewFileName);
        std::wstring resolvedNewFileName;
        if (resolvedNewFileNameN) {
          resolvedNewFileName = resolvedNewFileNameN.value();
        } else {
          // ソース上に既にリネームされて存在している
          // せめて近い名前で存在させてあげる
          // TODO: この処理はDZwCreateFileでも記述しているので適当に共通化したい
          const auto baseFilename = (util::vfs::IsRootDirectory(resolvedParentNewFilenameN.value()) ? L"\\"s : resolvedParentNewFilenameN.value() + L"\\"s) + std::wstring(GetBaseName(NewFileName)) + L"."s;
          unsigned long i = 2;
          do {
            resolvedNewFileName = baseFilename + std::to_wstring(i);
            i++;
          } while (FileExists(resolvedNewFileName));
        }
        if (const auto status = fileContext.mountSource.get().DMoveFile(resolvedFilename.c_str(), resolvedNewFileName.c_str(), ReplaceIfExisting, DokanFileInfo, fileContext.id); status != STATUS_SUCCESS) {
          return status;
        }
        // リネームにより下位層に隠れていたファイルが出現してしまうことがあるので、その対策
        // TopSourceと比較しているのは、同名の大文字小文字違いにリネームされたときに削除しないようにするため
        // TODO: 現状DMoveFileのIOに依存しているので、DMoveFileの前に下位層に存在しているか確認して処理した方が良いかも知れない
        const auto newIndex = GetMountSourceIndexR(resolvedFilename);
        if (newIndex && newIndex != TopSourceIndex) {
          std::lock_guard lock(m_metadataMutex);
          m_metadataStore.Delete(fileContext.filename);
        }
        //
        if (!resolvedNewFileNameN) {
          // TODO: もっと効率良く書く
          std::lock_guard lock(m_metadataMutex);
          m_metadataStore.Rename(std::wstring(util::vfs::GetParentPath(NewFileName)) + std::wstring(GetBaseName(resolvedNewFileName)), NewFileName);
          //m_metadataStore.Rename(resolvedNewFileName, NewFileName);
        }
        return STATUS_SUCCESS;
      }
    }
    // リネーム
    {
      std::lock_guard lock(m_metadataMutex);
      m_metadataStore.Rename(FileName, NewFileName);
    }
    return STATUS_SUCCESS;
  });
}


/*
SetEndOfFile Dokan API callback.

SetEndOfFile is used to truncate or extend a file (physical file size).
*/
NTSTATUS Mount::DSetEndOfFile(LPCWSTR FileName, LONGLONG ByteOffset, PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  return WrapException([=]() -> NTSTATUS {
    if (!HasFileContext(DokanFileInfo)) {
      return STATUS_INVALID_HANDLE;
    }
    if (!m_writable) {
      return STATUS_MEDIA_WRITE_PROTECTED;
    }
    if (const auto status = TransportIfNeeded(DokanFileInfo); status != STATUS_SUCCESS) {
      return status;
    }
    auto ptrFileContext = GetFileContextSharedPtr(DokanFileInfo);
    auto& fileContext = *ptrFileContext;
    if (!fileContext.writable) {
      return STATUS_ACCESS_DENIED;
    }
    return fileContext.mountSource.get().DSetEndOfFile(fileContext.resolvedFilename.c_str(), ByteOffset, DokanFileInfo, fileContext.id);
  });
}


/*
SetAllocationSize Dokan API callback.

SetAllocationSize is used to truncate or extend a file.
*/
NTSTATUS Mount::DSetAllocationSize(LPCWSTR FileName, LONGLONG AllocSize, PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  return WrapException([=]() -> NTSTATUS {
    if (!HasFileContext(DokanFileInfo)) {
      return STATUS_INVALID_HANDLE;
    }
    if (!m_writable) {
      return STATUS_MEDIA_WRITE_PROTECTED;
    }
    if (const auto status = TransportIfNeeded(DokanFileInfo); status != STATUS_SUCCESS) {
      return status;
    }
    auto ptrFileContext = GetFileContextSharedPtr(DokanFileInfo);
    auto& fileContext = *ptrFileContext;
    if (!fileContext.writable) {
      return STATUS_ACCESS_DENIED;
    }
    return fileContext.mountSource.get().DSetAllocationSize(fileContext.resolvedFilename.c_str(), AllocSize, DokanFileInfo, fileContext.id);
  });
}


/*
GetDiskFreeSpace Dokan API callback.

Retrieves information about the amount of space that is available on a disk volume. It consits of the total amount of space, the total amount of free space, and the total amount of free space available to the user that is associated with the calling thread.

Neither GetDiskFreeSpace nor GetVolumeInformation save the DOKAN_FILE_INFO.Context. Before these methods are called, ZwCreateFile may not be called. (ditto CloseFile and Cleanup)
*/
NTSTATUS Mount::DGetDiskFreeSpace(PULONGLONG FreeBytesAvailable, PULONGLONG TotalNumberOfBytes, PULONGLONG TotalNumberOfFreeBytes, PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  // 空き容量に関してはTopSourceIndexのものを用いる
  // 全体的な容量は各ソースを合計する

  return WrapException([=]() -> NTSTATUS {
    if (!TotalNumberOfBytes) {
      // 全体の容量を計算する必要がないならTopSourceIndexだけ確認すれば良い
      return m_topSource.DGetDiskFreeSpace(FreeBytesAvailable, TotalNumberOfBytes, TotalNumberOfFreeBytes, DokanFileInfo);
    }

    ULONGLONG total = 0;
    for (std::size_t i = 0; i < m_mountSources.size(); i++) {
      ULONGLONG tempTotal;
      const auto status = m_mountSources[i]->DGetDiskFreeSpace(i == TopSourceIndex ? FreeBytesAvailable : nullptr, &tempTotal, i == TopSourceIndex ? TotalNumberOfFreeBytes : nullptr, DokanFileInfo);
      if (status != STATUS_SUCCESS) {
        return status;
      }
      total += tempTotal;
    }
    *TotalNumberOfBytes = total;

    return STATUS_SUCCESS;
  });
}


/*
GetVolumeInformation Dokan API callback.

Retrieves information about the file system and volume associated with the specified root directory.

Neither GetVolumeInformation nor GetDiskFreeSpace save the DOKAN_FILE_INFO::Context. Before these methods are called, ZwCreateFile may not be called. (ditto CloseFile and Cleanup)

FileSystemName could be anything up to 10 characters. But Windows check few feature availability based on file system name. For this, it is recommended to set NTFS or FAT here.

FILE_READ_ONLY_VOLUME is automatically added to the FileSystemFlags if DOKAN_OPTION_WRITE_PROTECT was specified in DOKAN_OPTIONS when the volume was mounted.
*/
NTSTATUS Mount::DGetVolumeInformation(LPWSTR VolumeNameBuffer, DWORD VolumeNameSize, LPDWORD VolumeSerialNumber, LPDWORD MaximumComponentLength, LPDWORD FileSystemFlags, LPWSTR FileSystemNameBuffer, DWORD FileSystemNameSize, PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  return WrapException([=]() -> NTSTATUS {
    return m_topSource.DGetVolumeInformation(VolumeNameBuffer, VolumeNameSize, VolumeSerialNumber, MaximumComponentLength, FileSystemFlags, FileSystemNameBuffer, FileSystemNameSize, DokanFileInfo);
  });
}


/*
Mounted Dokan API callback.

Called when Dokan successfully mounts the volume.
*/
NTSTATUS Mount::DMounted(PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  return WrapException([=]() -> NTSTATUS {
    // notify mounted
    {
      std::lock_guard lock(m_imdMutex);
      m_imdState = ImdState::Mounting;
    }
    m_imdCv.notify_one();
    
    return STATUS_SUCCESS;
  });
}


/*
Unmounted Dokan API callback.

Called when Dokan is unmounting the volume.
*/
NTSTATUS Mount::DUnmounted(PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  return WrapException([=]() -> NTSTATUS {
    return STATUS_SUCCESS;
  });
}


/*
GetFileSecurity Dokan API callback.

Get specified information about the security of a file or directory.

Return STATUS_NOT_IMPLEMENTED to let dokan library build a sddl of the current process user with authenticate user rights for context menu. Return STATUS_BUFFER_OVERFLOW if buffer size is too small.

Since
Supported since version 0.6.0. The version must be specified in DOKAN_OPTIONS::Version.
*/
NTSTATUS Mount::DGetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PULONG LengthNeeded, PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  return WrapException([=]() -> NTSTATUS {
    if (!HasFileContext(DokanFileInfo)) {
      return STATUS_INVALID_HANDLE;
    }
    auto ptrFileContext = GetFileContextSharedPtr(DokanFileInfo);
    auto& fileContext = *ptrFileContext;
    const auto status = fileContext.mountSource.get().DGetFileSecurity(fileContext.resolvedFilename.c_str(), SecurityInformation, SecurityDescriptor, BufferLength, LengthNeeded, DokanFileInfo, fileContext.id);
    if (fileContext.writable || status != STATUS_SUCCESS) {
      return status;
    }
    // TODO: read metadata if available
    return STATUS_SUCCESS;
  });
}


/*
SetFileSecurity Dokan API callback.

Sets the security of a file or directory object.

Since
Supported since version 0.6.0. The version must be specified in DOKAN_OPTIONS::Version.
*/
NTSTATUS Mount::DSetFileSecurity(LPCWSTR FileName, PSECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG BufferLength, PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  return WrapException([=]() -> NTSTATUS {
    if (!HasFileContext(DokanFileInfo)) {
      return STATUS_INVALID_HANDLE;
    }
    if (!m_writable) {
      return STATUS_MEDIA_WRITE_PROTECTED;
    }
    auto ptrFileContext = GetFileContextSharedPtr(DokanFileInfo);
    auto& fileContext = *ptrFileContext;
    if (fileContext.writable) {
      return fileContext.mountSource.get().DSetFileSecurity(fileContext.resolvedFilename.c_str(), SecurityInformation, SecurityDescriptor, BufferLength, DokanFileInfo, fileContext.id);
    }
    // TODO: edit metadata
    /*
    if (const auto status = fileContext.UpdateLastWriteTime(); status != STATUS_SUCCESS) {
      return status;
    }
    //*/
    return STATUS_SUCCESS;
  });
}


/*
FindStreams Dokan API callback.

Retrieve all NTFS Streams informations on the file. This is only called if DOKAN_OPTION_ALT_STREAM is enabled.

Since
Supported since version 0.8.0. The version must be specified in DOKAN_OPTIONS::Version.
*/
NTSTATUS Mount::DFindStreams(LPCWSTR FileName, PFillFindStreamData FillFindStreamData, PDOKAN_FILE_INFO DokanFileInfo) noexcept {
  return WrapException([=]() -> NTSTATUS {
    if (!HasFileContext(DokanFileInfo)) {
      return STATUS_INVALID_HANDLE;
    }
    auto ptrFileContext = GetFileContextSharedPtr(DokanFileInfo);
    auto& fileContext = *ptrFileContext;
    // とりあえずファイルが属するソースにおける代替ストリームのみを列挙する
    NTSTATUS statusFromCallback = STATUS_SUCCESS;
    const auto status = fileContext.mountSource.get().ListStreams(fileContext.resolvedFilename.c_str(), [FillFindStreamData, DokanFileInfo, &statusFromCallback](WIN32_FIND_STREAM_DATA* findStreamData) noexcept {
      if (statusFromCallback != STATUS_SUCCESS) {
        return;
      }
      if (!findStreamData) {
        statusFromCallback = STATUS_ACCESS_VIOLATION;
        return;
      }
      FillFindStreamData(findStreamData, DokanFileInfo);
    });
    if (status != STATUS_SUCCESS) {
      return status;
    }
    if (statusFromCallback != STATUS_SUCCESS) {
      return statusFromCallback;
    }
    return STATUS_SUCCESS;
  });
}
