#include <fstream>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

#include "MountManager.hpp"
#include "JsonYamlInterop.hpp"
#include "YamlWstring.hpp"

using namespace std::literals;
using json = nlohmann::json;



namespace {
#if defined(_M_X64)
  const auto gFilterSuffix = L"\\MFP*_x64.dll"s;
#elif defined(_M_IX86)
  const auto gFilterSuffix = L"\\MFP*_x86.dll"s;
#else
# error unsupported architecture
#endif
}



MountManager::MergeFSError::MergeFSError(DWORD mergefsErrorCode, DWORD win32ErrorCode) :
  mergefsErrorCode(mergefsErrorCode),
  win32ErrorCode(win32ErrorCode)
{
  if (mergefsErrorCode == MERGEFS_ERROR_WINDOWS_ERROR) {
    errorMessage = L"MergeFS Error "s + std::to_wstring(mergefsErrorCode);
  } else {
    errorMessage = L"Win32 Error "s + std::to_wstring(win32ErrorCode);
  }
}


MountManager::MergeFSError::MergeFSError() :
  MergeFSError(GetError(NULL), GetLastError())
{}



MountManager::Win32ApiError::Win32ApiError(DWORD error) :
  MergeFSError(MERGEFS_ERROR_WINDOWS_ERROR, error)
{}



void MountManager::CheckLibMergeFSResult(BOOL ret) {
  if (!ret) {
    throw MergeFSError();
  }
}


void WINAPI MountManager::MountCallback(MOUNT_ID mountId, const MOUNT_INFO* ptrMountInfo, int dokanMainResult) noexcept {
  try {
    auto& mountManager = GetInstance();
    MountData& mountData = mountManager.mMountDataMap.at(mountId);
    mountData.callback(mountId, dokanMainResult, mountData, *ptrMountInfo);
    mountManager.mMountDataMap.erase(mountId);
  } catch (...) {}
}


MountManager& MountManager::GetInstance() {
  static MountManager sMountManager;
  return sMountManager;
}


void MountManager::AddPlugin(const std::wstring& filepath) {
  CheckLibMergeFSResult(::AddSourcePlugin(filepath.c_str(), FALSE, nullptr));
}


void MountManager::AddPluginsByDirectory(const std::wstring& directory) {
  WIN32_FIND_DATAW win32FindDataW;
  const auto filter = directory.c_str() + gFilterSuffix;
  const auto hFind = FindFirstFileW(filter.c_str(), &win32FindDataW);
  if (hFind == INVALID_HANDLE_VALUE) {
    if (GetLastError() == ERROR_FILE_NOT_FOUND) {
      return;
    }
    throw Win32ApiError();
  }
  do {
    try {
      AddPlugin(directory.c_str() + L"\\"s + win32FindDataW.cFileName);
    } catch (...) {
      FindClose(hFind);
      throw;
    }
  } while (FindNextFileW(hFind, &win32FindDataW));
  const auto error = GetLastError();
  FindClose(hFind);
  if (error != ERROR_SUCCESS && error != ERROR_NO_MORE_FILES) {
    throw Win32ApiError();
  }
}


void MountManager::GetPluginInfo(PLUGIN_ID pluginId, PLUGIN_INFO_EX& pluginInfoEx) const {
  CheckLibMergeFSResult(::GetSourcePluginInfo(pluginId, &pluginInfoEx));
}


PLUGIN_INFO_EX MountManager::GetPluginInfo(PLUGIN_ID pluginId) const {
  PLUGIN_INFO_EX pluginInfoEx;
  GetPluginInfo(pluginId, pluginInfoEx);
  return pluginInfoEx;
}


std::size_t MountManager::CountPlugins() const {
  DWORD numPluginIds = 0;
  CheckLibMergeFSResult(::GetSourcePlugins(&numPluginIds, nullptr, 0));
  return numPluginIds;
}


std::vector<PLUGIN_ID> MountManager::ListPluginIds() const {
  const auto numPluginIds = CountPlugins();
  std::vector<PLUGIN_ID> pluginIds(numPluginIds);
  CheckLibMergeFSResult(::GetSourcePlugins(nullptr, pluginIds.data(), static_cast<DWORD>(numPluginIds)));
  return pluginIds;
}


std::vector<std::pair<PLUGIN_ID, PLUGIN_INFO_EX>> MountManager::ListPlugins() const {
  const auto pluginIds = ListPluginIds();
  std::vector<std::pair<PLUGIN_ID, PLUGIN_INFO_EX>> pluginInfos(pluginIds.size());
  for (DWORD i = 0; i < pluginIds.size(); i++) {
    const auto pluginId = pluginIds[i];
    pluginInfos[i].first = pluginId;
    GetPluginInfo(pluginId, pluginInfos[i].second);
  }
  return pluginInfos;
}


MOUNT_ID MountManager::AddMount(const std::wstring& configFilepath, std::function<void(MOUNT_ID, int, MountData&, const MOUNT_INFO&)> callback) {
  std::ifstream ifs(configFilepath);
  if (!ifs) {
    throw std::ifstream::failure("failed to load configuration file");
  }
  const auto yaml = YAML::Load(ifs);
  ifs.close();

  const auto mountPoint = yaml["mountPoint"].as<std::wstring>();

  // TODO: convert to absolute path with configuration file's directory as base directory
  const auto metadataFileName = yaml["metadata"].as<std::wstring>();

  bool writable = true;
  try {
    const auto& yamlWritable = yaml["writable"].as<bool>();
  } catch (YAML::BadConversion&) {
  } catch (YAML::InvalidNode&) {}

  bool deferCopyEnabled = true;
  try {
    deferCopyEnabled = yaml["deferCopyEnabled"].as<bool>();
  } catch (YAML::BadConversion&) {
  } catch (YAML::InvalidNode&) {}

  bool caseSensitive = false;
  try {
    caseSensitive = yaml["caseSensitive"].as<bool>();
  } catch (YAML::BadConversion&) {
  } catch (YAML::InvalidNode&) {}

  struct SourceInitializeInfoStorage {
    std::wstring mountSource;
    std::wstring sourcePluginFilename;
    std::string sourcePluginOptionsJSON;
  };
  const auto& yamlSources = yaml["sources"];
  std::vector<SourceInitializeInfoStorage> sourceInitializeInfoStorages(yamlSources.size());
  std::vector<MOUNT_SOURCE_INITIALIZE_INFO> sourceInitializeInfos(yamlSources.size());
  for (std::size_t i = 0; i < yamlSources.size(); i++) {
    const auto& yamlSource = yamlSources[i];
    const auto& yamlPlugin = yamlSource["plugin"];

    const std::wstring mountSource = yamlSource["source"].as<std::wstring>();

    std::wstring sourcePluginFilename;
    try {
      sourcePluginFilename = yamlPlugin["filename"].as<std::wstring>();
    } catch (YAML::BadConversion&) {
    } catch (YAML::InvalidNode&) {}

    std::string sourcePluginOptionsJSON;
    try {
      const auto& yamlOptions = yamlPlugin["options"];
      try {
        sourcePluginOptionsJSON = yamlOptions.as<std::string>();
      } catch (YAML::BadConversion&) {
        const json optionsJson = yamlOptions;
        sourcePluginOptionsJSON = optionsJson.dump();
      }
    } catch (YAML::BadConversion&) {
    } catch (YAML::InvalidNode&) {}

    auto& storage = sourceInitializeInfoStorages[i];
    storage = SourceInitializeInfoStorage{
      mountSource,
      sourcePluginFilename,
      sourcePluginOptionsJSON,
    };
    sourceInitializeInfos[i] = MOUNT_SOURCE_INITIALIZE_INFO{
      storage.mountSource.c_str(),
      {},   // TODO: parse GUID
      storage.sourcePluginFilename.c_str(),
      storage.sourcePluginOptionsJSON.c_str(),
    };
  }

  MOUNT_INITIALIZE_INFO mountInitializeInfo{
    mountPoint.c_str(),
    writable,
    metadataFileName.c_str(),
    deferCopyEnabled,
    caseSensitive,
    static_cast<DWORD>(sourceInitializeInfos.size()),
    sourceInitializeInfos.data(),
  };

  // TODO: error handling, restore current directory
  const std::wstring configFileDirectory = configFilepath + L"\\.."s;
  SetCurrentDirectoryW(configFileDirectory.c_str());

  MOUNT_ID mountId = MOUNT_ID_NULL;
  if (!::Mount(&mountInitializeInfo, MountCallback, &mountId)) {
    throw MountError{
      MergeFSError(),
      mountPoint,
      configFilepath,
    };
  }

  mMountDataMap.emplace(mountId, MountData{
    configFilepath,
    callback,
  });

  return mountId;
}


void MountManager::RemoveMount(MOUNT_ID mountId, bool safe) {
  if (safe) {
    CheckLibMergeFSResult(::SafeUnmount(mountId));
  } else {
    CheckLibMergeFSResult(::Unmount(mountId));
  }
}


void MountManager::GetMountInfo(MOUNT_ID mountId, MOUNT_INFO& mountInfo) const {
  CheckLibMergeFSResult(::GetMountInfo(mountId, &mountInfo));
}


MOUNT_INFO MountManager::GetMountInfo(MOUNT_ID mountId) const {
  MOUNT_INFO mountInfo;
  GetMountInfo(mountId, mountInfo);
  return mountInfo;
}


const MountManager::MountData& MountManager::GetMountData(MOUNT_ID mountId) const {
  return mMountDataMap.at(mountId);
}


std::size_t MountManager::CountMounts() const {
  DWORD numMountIds = 0;
  CheckLibMergeFSResult(GetMounts(&numMountIds, nullptr, 0));
  return numMountIds;
}


std::vector<MOUNT_ID> MountManager::ListMountIds() const {
  const auto numMountIds = CountMounts();
  std::vector<MOUNT_ID> mountIds(numMountIds);
  CheckLibMergeFSResult(::GetMounts(nullptr, mountIds.data(), static_cast<DWORD>(numMountIds)));
  return mountIds;
}


std::vector<std::pair<MOUNT_ID, MOUNT_INFO>> MountManager::ListMounts() const {
  const auto mountIds = ListMountIds();
  std::vector<std::pair<MOUNT_ID, MOUNT_INFO>> mountInfos(mountIds.size());
  for (DWORD i = 0; i < mountIds.size(); i++) {
    const auto mountId = mountIds[i];
    mountInfos[i].first = mountId;
    GetMountInfo(mountId, mountInfos[i].second);
  }
  return mountInfos;
}


void MountManager::UnmountAll(bool safe) {
  if (safe) {
    CheckLibMergeFSResult(::SafeUnmountAll());
  } else {
    CheckLibMergeFSResult(::UnmountAll());
  }
}
