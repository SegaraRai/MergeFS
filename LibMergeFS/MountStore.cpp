#include "MountStore.hpp"
#include "Mount.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Windows.h>

using namespace std::literals;



void MountStore::MountData::MountInfoWrapper::MountSourceInfoWrapper::Update() {
  mountSourceInfo = MOUNT_SOURCE_INFO{
    mountSource.c_str(),
    sourcePluginId,
    sourcePluginOptionsJSON.c_str(),
  };
}


MountStore::MountData::MountInfoWrapper::MountSourceInfoWrapper::MountSourceInfoWrapper(const MountSourceInfoWrapper& other) :
  mountSource(other.mountSource),
  sourcePluginId(other.sourcePluginId),
  sourcePluginOptionsJSON(other.sourcePluginOptionsJSON)
{
  Update();
}


MountStore::MountData::MountInfoWrapper::MountSourceInfoWrapper::MountSourceInfoWrapper(MountSourceInfoWrapper&& other) :
  mountSource(std::move(other.mountSource)),
  sourcePluginId(std::move(other.sourcePluginId)),
  sourcePluginOptionsJSON(std::move(other.sourcePluginOptionsJSON))
{
  Update();
}


MountStore::MountData::MountInfoWrapper::MountSourceInfoWrapper& MountStore::MountData::MountInfoWrapper::MountSourceInfoWrapper::operator=(const MountSourceInfoWrapper& other) {
  mountSource = other.mountSource;
  sourcePluginId = other.sourcePluginId;
  sourcePluginOptionsJSON = other.sourcePluginOptionsJSON;

  Update();

  return *this;
}


MountStore::MountData::MountInfoWrapper::MountSourceInfoWrapper& MountStore::MountData::MountInfoWrapper::MountSourceInfoWrapper::operator=(MountSourceInfoWrapper&& other) {
  mountSource = std::move(other.mountSource);
  sourcePluginId = std::move(other.sourcePluginId);
  sourcePluginOptionsJSON = std::move(other.sourcePluginOptionsJSON);

  Update();

  return *this;
}


MountStore::MountData::MountInfoWrapper::MountSourceInfoWrapper::MountSourceInfoWrapper(std::wstring_view mountSource, PLUGIN_ID sourcePluginId, std::string_view sourcePluginOptionsJSON) :
  mountSource(mountSource),
  sourcePluginId(sourcePluginId),
  sourcePluginOptionsJSON(sourcePluginOptionsJSON)
{
  Update();
}


const MOUNT_SOURCE_INFO& MountStore::MountData::MountInfoWrapper::MountSourceInfoWrapper::Get() const {
  return mountSourceInfo;
}



void MountStore::MountData::MountInfoWrapper::Update() {
  sources.resize(wrappedSources.size());
  for (std::size_t i = 0; i < wrappedSources.size(); i++) {
    sources[i] = wrappedSources[i].Get();
  }
  mountInfo = MOUNT_INFO{
    mountPoint.c_str(),
    writable ? TRUE : FALSE,
    metadataFileName.c_str(),
    deferCopyEnabled ? TRUE : FALSE,
    caseSensitive ? TRUE : FALSE,
    static_cast<DWORD>(sources.size()),
    sources.data(),
  };
}


MountStore::MountData::MountInfoWrapper::MountInfoWrapper(const MountInfoWrapper& other) :
  mountPoint(other.mountPoint),
  writable(other.writable),
  metadataFileName(other.metadataFileName),
  deferCopyEnabled(other.deferCopyEnabled),
  caseSensitive(other.caseSensitive),
  wrappedSources(other.wrappedSources)
{
  Update();
}


MountStore::MountData::MountInfoWrapper::MountInfoWrapper(MountInfoWrapper&& other) :
  mountPoint(std::move(other.mountPoint)),
  writable(std::move(other.writable)),
  metadataFileName(std::move(other.metadataFileName)),
  deferCopyEnabled(std::move(other.deferCopyEnabled)),
  caseSensitive(std::move(other.caseSensitive)),
  wrappedSources(std::move(other.wrappedSources))
{
  Update();
}


MountStore::MountData::MountInfoWrapper& MountStore::MountData::MountInfoWrapper::operator=(const MountInfoWrapper& other) {
  mountPoint = other.mountPoint;
  writable = other.writable;
  metadataFileName = other.metadataFileName;
  deferCopyEnabled = other.deferCopyEnabled;
  caseSensitive = other.caseSensitive;
  wrappedSources = other.wrappedSources;

  Update();

  return *this;
}


MountStore::MountData::MountInfoWrapper& MountStore::MountData::MountInfoWrapper::operator=(MountInfoWrapper&& other) {
  mountPoint = std::move(other.mountPoint);
  writable = std::move(other.writable);
  metadataFileName = std::move(other.metadataFileName);
  deferCopyEnabled = std::move(other.deferCopyEnabled);
  caseSensitive = std::move(other.caseSensitive);
  wrappedSources = std::move(other.wrappedSources);

  Update();

  return *this;
}


MountStore::MountData::MountInfoWrapper::MountInfoWrapper(std::wstring_view mountPoint, bool writable, std::wstring_view metadataFileName, bool deferCopyEnabled, bool caseSensitive, const std::vector<std::pair<PLUGIN_ID, PLUGIN_INITIALIZE_MOUNT_INFO>>& sources) :
  mountPoint(mountPoint),
  writable(writable),
  metadataFileName(metadataFileName),
  deferCopyEnabled(deferCopyEnabled),
  caseSensitive(caseSensitive),
  wrappedSources(sources.size())
{
  for (std::size_t i = 0; i < sources.size(); i++) {
    const auto& [pluginId, pluginInitializeInfo] = sources[i];
    wrappedSources[i] = MountSourceInfoWrapper(pluginInitializeInfo.FileName, pluginId, pluginInitializeInfo.OptionsJSON);
  }
  Update();
}


const MOUNT_INFO& MountStore::MountData::MountInfoWrapper::Get() const {
  return mountInfo;
}



MountStore::MountStore() :
  m_generalMutex(),
  m_mountMap(),
  m_minimumUnusedMountId(MountIdStart),
  m_itMutex(),
  m_itCv(),
  m_itFinish(false),
  m_itUnregisterIds(),
  m_unregisterThread([this]() {
    std::unique_lock lock(m_itMutex);

    do {
      m_itCv.wait(lock, [this]() {
        return m_itFinish || !m_itUnregisterIds.empty();
      });

      if (!m_itUnregisterIds.empty()) {
        {
          std::lock_guard lock(m_generalMutex);
          for (const auto& mountId : m_itUnregisterIds) {
            m_mountMap.erase(mountId);
            if (mountId < m_minimumUnusedMountId) {
              m_minimumUnusedMountId = mountId;
            }
          }
        }
        m_itUnregisterIds.clear();
      }
    } while (!m_itFinish);
  })
{}


MountStore::~MountStore() {
  {
    std::lock_guard lock(m_itMutex);
    m_itFinish = true;
  }
  m_itCv.notify_one();
  m_unregisterThread.join();
}


MountStore::MOUNT_ID MountStore::Mount(std::wstring_view mountPoint, bool writable, std::wstring_view metadataFileName, bool deferCopyEnabled, bool caseSensitive, const std::vector<std::pair<PLUGIN_ID, PLUGIN_INITIALIZE_MOUNT_INFO>>& sources, std::function<void(MOUNT_ID, const MOUNT_INFO&, int)> callback) {
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

  std::lock_guard lock(m_generalMutex);

  const MOUNT_ID mountId = m_minimumUnusedMountId;
  do {
    m_minimumUnusedMountId++;
  } while (m_mountMap.count(m_minimumUnusedMountId));

  auto mount = std::make_unique<::Mount>(mountPoint, writable, metadataFileName, deferCopyEnabled, caseSensitive, std::move(mountSources), [this, callback, mountId](int dokanMainResult) {
    callback(mountId, m_mountMap.at(mountId).wrappedMountInfo.Get(), dokanMainResult);

    {
      std::lock_guard lock(m_itMutex);
      m_itUnregisterIds.emplace_back(mountId);
    }
    m_itCv.notify_one();
  });

  const auto sourceWritable = mount->IsWritable();
  m_mountMap.emplace(mountId, MountData{
    std::move(mount),
    MountData::MountInfoWrapper(mountPoint, sourceWritable, metadataFileName, deferCopyEnabled, caseSensitive, sources),
  });

  return mountId;
}


bool MountStore::HasMount(MOUNT_ID mountId) const {
  std::shared_lock lock(m_generalMutex);
  return m_mountMap.count(mountId);
}


std::size_t MountStore::CountMounts() const {
  std::shared_lock lock(m_generalMutex);
  return m_mountMap.size();
}


std::vector<MountStore::MOUNT_ID> MountStore::ListMounts() const {
  std::shared_lock lock(m_generalMutex);
  std::vector<MOUNT_ID> mountIds(m_mountMap.size());
  std::size_t i = 0;
  for (const auto& [mountId, _] : m_mountMap) {
    mountIds[i++] = mountId;
  }
  sort(mountIds.begin(), mountIds.end());
  return mountIds;
}


const MOUNT_INFO& MountStore::GetMountInfo(MOUNT_ID mountId) const {
  std::shared_lock lock(m_generalMutex);
  if (!m_mountMap.count(mountId)) {
    throw std::out_of_range(u8"no such mountId");
  }
  return m_mountMap.at(mountId).wrappedMountInfo.Get();
}


bool MountStore::SafeUnmount(MOUNT_ID mountId) {
  std::lock_guard lock(m_generalMutex);
  if (!m_mountMap.count(mountId)) {
    return false;
  }
  if (!m_mountMap.at(mountId).mount->SafeUnmount()) {
    return false;
  }
  return true;
}


void MountStore::SafeUnmountAll() {
  std::lock_guard lock(m_generalMutex);
  for (auto& [mountId, mountData] : m_mountMap) {
    mountData.mount->SafeUnmount();
  }
}


bool MountStore::Unmount(MOUNT_ID mountId) {
  std::lock_guard lock(m_generalMutex);
  if (!m_mountMap.count(mountId)) {
    return false;
  }
  m_mountMap.at(mountId).mount->Unmount();
  return true;
}


void MountStore::UnmountAll() {
  std::lock_guard lock(m_generalMutex);
  for (auto&[mountId, mountData] : m_mountMap) {
    mountData.mount->Unmount();
  }
}
