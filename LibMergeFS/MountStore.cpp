#include "MountStore.hpp"
#include "Mount.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Windows.h>

using namespace std::literals;



MountStore::MOUNT_ID MountStore::Mount(std::wstring_view mountPoint, bool writable, std::wstring_view metadataFileName, bool deferCopyEnabled, const std::vector<std::pair<PLUGIN_ID, PLUGIN_INITIALIZE_MOUNT_INFO>>& sources, std::function<void(int)> callback) {
  if (sources.empty()) {
    throw NoSourceError();
  }
  const std::size_t sourceCount = sources.size();
  std::vector<std::unique_ptr<MountSource>> mountSources(sourceCount);
  for (std::size_t i = 0; i < sourceCount; i++) {
    PLUGIN_ID sourcePluginId = sources[i].first;
    const auto& initializeMountInfo = sources[i].second;
    if (sourcePluginId == PLUGIN_ID_NULL) {
      sourcePluginId = SearchSourcePluginForSource(initializeMountInfo).value_or(PLUGIN_ID_NULL);
    }
    if (sourcePluginId == PLUGIN_ID_NULL || !HasSourcePlugin(sourcePluginId)) {
      throw NoSourcePluginError();
    }
    auto& sourcePlugin = GetSourcePlugin(sourcePluginId);
    mountSources[i] = std::make_unique<MountSource>(initializeMountInfo, sourcePlugin);
  }
  const MOUNT_ID mountId = m_minimumUnusedMountId;
  do {
    m_minimumUnusedMountId++;
  } while (m_mountMap.count(m_minimumUnusedMountId));
  auto mount = std::make_unique<::Mount>(mountPoint, writable, metadataFileName, deferCopyEnabled, false, std::move(mountSources), callback);
  m_mountMap.emplace(mountId, std::move(mount));
  return mountId;
}


bool MountStore::HasMount(MOUNT_ID mountId) const {
  return m_mountMap.count(mountId);
}


std::size_t MountStore::CountMounts() const {
  return m_mountMap.size();
}


std::vector<MountStore::MOUNT_ID> MountStore::ListMounts() const {
  std::vector<MOUNT_ID> mountIds(m_mountMap.size());
  std::size_t i = 0;
  for (const auto& [mountId, _] : m_mountMap) {
    mountIds[i++] = mountId;
  }
  sort(mountIds.begin(), mountIds.end());
  return mountIds;
}


bool MountStore::SafeUnmount(MOUNT_ID mountId) {
  if (!m_mountMap.count(mountId)) {
    return false;
  }
  return m_mountMap.at(mountId)->SafeUnmount();
}


void MountStore::SafeUnmountAll() {
  for (auto& [mountId, upMount] : m_mountMap) {
    upMount->SafeUnmount();
  }
}


bool MountStore::Unmount(MOUNT_ID mountId) {
  if (!m_mountMap.count(mountId)) {
    return false;
  }
  m_mountMap.erase(mountId);
  if (mountId < m_minimumUnusedMountId) {
    m_minimumUnusedMountId = mountId;
  }
  return true;
}


void MountStore::UnmountAll() {
  m_mountMap.clear();
  m_minimumUnusedMountId = MountIdStart;
}
