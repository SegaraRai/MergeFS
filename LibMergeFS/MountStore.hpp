#pragma once

#define FROMLIBMERGEFS

#include "../SDK/LibMergeFS.h"

#include "Mount.hpp"
#include "SourcePluginStore.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
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
  std::unordered_map<MOUNT_ID, std::unique_ptr<Mount>> m_mountMap;
  MOUNT_ID m_minimumUnusedMountId = MountIdStart;

public:
  MOUNT_ID Mount(std::wstring_view mountPoint, bool writable, std::wstring_view metadataFileName, bool deferCopyEnabled, const std::vector<std::pair<PLUGIN_ID, std::wstring>>& sources, std::function<void(int)> callback);
  bool HasMount(MOUNT_ID mountId) const;
  std::size_t CountMounts() const;
  std::vector<MOUNT_ID> ListMounts() const;
  bool SafeUnmount(MOUNT_ID mountId);
  bool Unmount(MOUNT_ID mountId);
  void SafeUnmountAll();
  void UnmountAll();
};
