#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "../SDK/LibMergeFS.h"


class MountManager {
public:
  struct MountData {
    std::wstring configFilepath;
    std::function<void(MOUNT_ID, int, MountData&, const MOUNT_INFO*)> callback;
  };

  struct MergeFSError : MERGEFS_ERROR_INFO {
  private:
    static MERGEFS_ERROR_INFO GetLastErrorInfo();

  public:
    std::wstring errorMessage;

    MergeFSError(const MERGEFS_ERROR_INFO& errorInfo);
    MergeFSError();
  };

  struct MountError : MergeFSError {
    std::wstring configFilepath;
    std::wstring mountPoint;
  };

  struct Win32ApiError : MergeFSError {
    Win32ApiError(DWORD error = GetLastError());
  };

private:
  std::unordered_map<MOUNT_ID, MountData> mMountDataMap;

  static void CheckLibMergeFSResult(BOOL ret);
  static std::wstring ResolveMountPoint(const std::wstring& mountPoint);

  static void WINAPI MountCallback(MOUNT_ID mountId, const MOUNT_INFO* ptrMountInfo, int dokanMainResult) noexcept;

  MountManager();

public:
  MountManager(const MountManager&) = delete;
  MountManager(MountManager&&) = delete;

  MountManager& operator=(const MountManager&) = delete;
  MountManager& operator=(MountManager&&) = delete;

  static MountManager& GetInstance();

  void AddPlugin(const std::wstring& filepath);
  void AddPluginsByDirectory(const std::wstring& directory);
  void GetPluginInfo(PLUGIN_ID pluginId, PLUGIN_INFO_EX& pluginInfoEx) const;
  PLUGIN_INFO_EX GetPluginInfo(PLUGIN_ID pluginId) const;
  std::size_t CountPlugins() const;
  std::vector<PLUGIN_ID> ListPluginIds() const;
  std::vector<std::pair<PLUGIN_ID, PLUGIN_INFO_EX>> ListPlugins() const;

  MOUNT_ID AddMount(const std::wstring& configFilepath, std::function<void(MOUNT_ID, int, MountData&, const MOUNT_INFO*)> callback);
  void RemoveMount(MOUNT_ID mountId, bool safe);
  void GetMountInfo(MOUNT_ID mountId, MOUNT_INFO& mountInfo) const;
  MOUNT_INFO GetMountInfo(MOUNT_ID mountId) const;
  const MountData& GetMountData(MOUNT_ID mountId) const;
  std::size_t CountMounts() const;
  std::vector<MOUNT_ID> ListMountIds() const;
  std::vector<std::pair<MOUNT_ID, MOUNT_INFO>> ListMounts() const;
  void UnmountAll(bool safe);

  void Uninit(bool safe);
};
