#pragma once

#define FROMLIBMERGEFS

#include "../SDK/LibMergeFS.h"

#include "SourcePlugin.hpp"
#include "GUIDUtil.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>


class SourcePluginStore {
public:
  using PLUGIN_ID = ::PLUGIN_ID;

  static constexpr PLUGIN_ID PLUGIN_ID_NULL = ::PLUGIN_ID_NULL;
  static constexpr PLUGIN_ID PluginIdStart = PLUGIN_ID_NULL + 1;

private:
  PLUGIN_ID m_minimumUnusedPluginId = PluginIdStart;
  std::unordered_map<PLUGIN_ID, std::unique_ptr<SourcePlugin>> m_sourcePluginMap;
  std::unordered_map<GUID, PLUGIN_ID, GUIDHash> m_guidIdMap;
  std::unordered_map<std::wstring, PLUGIN_ID> m_filenameIdMap;
  std::deque<PLUGIN_ID> m_sourcePluginOrder;

  static std::wstring FilenameToKey(std::wstring_view filename);

public:
  PLUGIN_ID AddSourcePlugin(std::wstring_view filename, bool front = false);
  bool HasSourcePlugin(PLUGIN_ID pluginId) const;
  std::size_t CountSourcePlugins() const;
  std::vector<SourcePluginStore::PLUGIN_ID> ListSourcePlugins() const;
  SourcePlugin& GetSourcePlugin(PLUGIN_ID pluginId);
  const SourcePlugin& GetSourcePlugin(PLUGIN_ID pluginId) const;
  std::optional<PLUGIN_ID> GetSourcePluginIdFromGUID(const GUID& guid) const;
  std::optional<PLUGIN_ID> GetSourcePluginIdFromFilename(std::wstring_view name) const;
  std::optional<PLUGIN_ID> SearchSourcePluginForSource(std::wstring_view sourceFilename) const;
  bool RemoveSourcePlugin(PLUGIN_ID pluginId);
  bool SetSourcePluginOrder(const PLUGIN_ID* pluginIds, std::size_t numPluginIds);
};
