#include "SourcePluginStore.hpp"
#include "Util.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

using namespace std::literals;


std::wstring SourcePluginStore::FilenameToKey(std::wstring_view filename) {
  std::wstring normalizedFilename(filename);
  // convert to lowercase, slash to backslash
  for (auto& c : normalizedFilename) {
    if (c >= L'A' && c <= L'Z') {
      c = c - L'A' + L'a';
    } else if (c == L'/') {
      c = L'\\';
    }
  }
  // trim directory
  std::wstring_view key = GetBaseName(normalizedFilename);
  // trim extension
  key = key.substr(0, key.find_last_of(L'.'));
  // trim _x86 or _x64 suffix
  if (key.size() > 4 && key[key.size() - 4] == L'_' && key[key.size() - 3] == L'x') {
    key.remove_suffix(4);
  }
  return std::wstring(key);
}


SourcePluginStore::PLUGIN_ID SourcePluginStore::AddSourcePlugin(std::wstring_view filename, bool front) {
  const PLUGIN_ID pluginId = m_minimumUnusedPluginId;
  const auto filenameKey = FilenameToKey(filename);
  auto ptrSourcePlugin = std::make_unique<SourcePlugin>(filename);
  const auto& guid = ptrSourcePlugin->pluginInfo.guid;
  m_sourcePluginMap.emplace(pluginId, std::move(ptrSourcePlugin));
  m_filenameIdMap.emplace(filenameKey, pluginId);
  m_guidIdMap.emplace(guid, pluginId);
  if (front) {
    m_sourcePluginOrder.emplace_front(pluginId);
  } else {
    m_sourcePluginOrder.emplace_back(pluginId);
  }
  do {
    m_minimumUnusedPluginId++;
  } while (m_sourcePluginMap.count(m_minimumUnusedPluginId));
  return pluginId;
}


bool SourcePluginStore::HasSourcePlugin(PLUGIN_ID pluginId) const {
  return m_sourcePluginMap.count(pluginId);
}


std::size_t SourcePluginStore::CountSourcePlugins() const {
  return m_sourcePluginMap.size();
}


std::vector<SourcePluginStore::PLUGIN_ID> SourcePluginStore::ListSourcePlugins() const {
  return std::vector(m_sourcePluginOrder.cbegin(), m_sourcePluginOrder.cend());
}


SourcePlugin& SourcePluginStore::GetSourcePlugin(PLUGIN_ID pluginId) {
  return *m_sourcePluginMap.at(pluginId);
}


const SourcePlugin& SourcePluginStore::GetSourcePlugin(PLUGIN_ID pluginId) const {
  return *m_sourcePluginMap.at(pluginId);
}


std::optional<SourcePluginStore::PLUGIN_ID> SourcePluginStore::GetSourcePluginIdFromGUID(const GUID& guid) const {
  if (!m_guidIdMap.count(guid)) {
    return std::nullopt;
  }
  return m_guidIdMap.at(guid);
}


std::optional<SourcePluginStore::PLUGIN_ID> SourcePluginStore::GetSourcePluginIdFromFilename(std::wstring_view filename) const {
  const auto filenameKey = FilenameToKey(filename);
  if (!m_filenameIdMap.count(filenameKey)) {
    return std::nullopt;
  }
  return m_filenameIdMap.at(filenameKey);
}


std::optional<SourcePluginStore::PLUGIN_ID> SourcePluginStore::SearchSourcePluginForSource(std::wstring_view sourceFilename) const {
  const std::wstring sSourceFilename(sourceFilename);
  const auto csSourceFilename = sSourceFilename.c_str();
  for (const auto& id : m_sourcePluginOrder) {
    auto& sourcePlugin = *m_sourcePluginMap.at(id);
    if (sourcePlugin.SIsSupportedFile(csSourceFilename)) {
      return id;
    }
  }
  return std::nullopt;
}


bool SourcePluginStore::RemoveSourcePlugin(PLUGIN_ID pluginId) {
  if (!m_sourcePluginMap.count(pluginId)) {
    return false;
  }
  const auto& guid = m_sourcePluginMap.at(pluginId)->pluginInfo.guid;
  const auto filenameKey = FilenameToKey(m_sourcePluginMap.at(pluginId)->pluginFilePath);
  m_sourcePluginMap.erase(pluginId);
  m_filenameIdMap.erase(filenameKey);
  m_guidIdMap.erase(guid);
  m_sourcePluginOrder.erase(find(m_sourcePluginOrder.begin(), m_sourcePluginOrder.end(), pluginId));
  if (pluginId < m_minimumUnusedPluginId) {
    m_minimumUnusedPluginId = pluginId;
  }
  return true;
}


bool SourcePluginStore::SetSourcePluginOrder(const PLUGIN_ID* pluginIds, std::size_t numPluginIds) {
  const std::size_t count = m_sourcePluginOrder.size();

  if (numPluginIds != count) {
    return false;
  }

  for (std::size_t i = 0; i < count; i++) {
    if (!m_sourcePluginMap.count(pluginIds[i])) {
      return false;
    }
  }

  // random access of deque: O(1)
  for (std::size_t i = 0; i < count; i++) {
    m_sourcePluginOrder[i] = pluginIds[i];
  }

  return true;
}
