#pragma once

#define FROMLIBMERGEFS

#include "../SDK/LibMergeFS.h"

#include "Mount.hpp"
#include "SourcePluginStore.hpp"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>


class MountStore : public SourcePluginStore {
public:
  class MountStoreError : public std::exception {};
  class NoSourceError : public MountStoreError {};
  class NoSourcePluginError : public MountStoreError {};

  using MOUNT_ID = ::MOUNT_ID;

  static constexpr MOUNT_ID MOUNT_ID_NULL = ::MOUNT_ID_NULL;
  static constexpr MOUNT_ID MountIdStart = MOUNT_ID_NULL + 1;

private:
  struct MountData {
    class MountInfoWrapper {
      class MountSourceInfoWrapper {
        std::wstring mountSource;
        PLUGIN_ID sourcePluginId;
        std::string sourcePluginOptionsJSON;
        MOUNT_SOURCE_INFO mountSourceInfo;

        void Update();

      public:
        MountSourceInfoWrapper(const MountSourceInfoWrapper& other);
        MountSourceInfoWrapper(MountSourceInfoWrapper&& other);
        MountSourceInfoWrapper& operator=(const MountSourceInfoWrapper& other);
        MountSourceInfoWrapper& operator=(MountSourceInfoWrapper&& other);

        MountSourceInfoWrapper() = default;
        MountSourceInfoWrapper(std::wstring_view mountSource, PLUGIN_ID sourcePluginId, std::string_view sourcePluginOptionsJSON);

        const MOUNT_SOURCE_INFO& Get() const;
      };

      std::wstring mountPoint;
      bool writable;
      std::wstring metadataFileName;
      bool deferCopyEnabled;
      bool caseSensitive;
      std::vector<MountSourceInfoWrapper> wrappedSources;
      std::vector<MOUNT_SOURCE_INFO> sources;
      MOUNT_INFO mountInfo;

      void Update();

    public:
      MountInfoWrapper(const MountInfoWrapper& other);
      MountInfoWrapper(MountInfoWrapper&& other);
      MountInfoWrapper& operator=(const MountInfoWrapper& other);
      MountInfoWrapper& operator=(MountInfoWrapper&& other);

      MountInfoWrapper(std::wstring_view mountPoint, bool writable, std::wstring_view metadataFileName, bool deferCopyEnabled, bool caseSensitive, const std::vector<std::pair<PLUGIN_ID, PLUGIN_INITIALIZE_MOUNT_INFO>>& sources);

      void SetWritable(bool writable);

      const MOUNT_INFO& Get() const;
    };

    std::unique_ptr<Mount> mount;
    MountInfoWrapper wrappedMountInfo;
  };

  mutable std::shared_mutex m_generalMutex;
  std::unordered_map<MOUNT_ID, MountData> m_mountMap;
  MOUNT_ID m_minimumUnusedMountId;
  std::mutex m_itMutex;
  std::condition_variable m_itCv;
  bool m_itFinish;
  std::deque<MOUNT_ID> m_itUnregisterIds;
  std::thread m_unregisterThread;

public:
  MountStore();
  ~MountStore();

  MOUNT_ID Mount(std::wstring_view mountPoint, bool writable, std::wstring_view metadataFileName, bool deferCopyEnabled, bool caseSensitive, const std::vector<std::pair<PLUGIN_ID, PLUGIN_INITIALIZE_MOUNT_INFO>>& sources, std::function<void(MOUNT_ID, const MOUNT_INFO*, int)> callback);
  bool HasMount(MOUNT_ID mountId) const;
  std::size_t CountMounts() const;
  std::vector<MOUNT_ID> ListMounts() const;
  const MOUNT_INFO& GetMountInfo(MOUNT_ID mountId) const;
  bool Unmount(MOUNT_ID mountId);
  void UnmountAll();
  bool SafeUnmount(MOUNT_ID mountId);
  void SafeUnmountAll();
};
