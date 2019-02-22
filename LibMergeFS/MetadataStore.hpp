#pragma once

#include "Metadata.hpp"
#include "RenameStore.hpp"

#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <Windows.h>


class MetadataStore {
public:
  static constexpr auto RemovedPrefix = L"\\$MergeFSSystemData\\Removed";

private:
  const bool mCaseSensitive;
  HANDLE mHFile = NULL;
  RenameStore mRenameStore;
  std::unordered_map<std::wstring, Metadata> mMetadataMap;

  std::wstring FilenameToKey(std::wstring_view filename) const;
  void LoadFromFile();
  void SaveToFile();


public:
  MetadataStore(const MetadataStore&) = delete;

  MetadataStore() = default;
  MetadataStore(std::wstring_view storeFileName, bool caseSensitive);
  ~MetadataStore();

  void SetFilePath(std::wstring_view storeFilename);
  std::optional<std::wstring> ResolveFilepath(std::wstring_view filename) const;
  bool HasMetadataR(std::wstring_view resolvedFilename) const;
  bool HasMetadata(std::wstring_view filename) const;
  const Metadata& GetMetadataR(std::wstring_view resolvedFilename) const;
  const Metadata& GetMetadata(std::wstring_view filename) const;
  Metadata GetMetadata2R(std::wstring_view resolvedFilename) const;
  Metadata GetMetadata2(std::wstring_view filename) const;
  void SetMetadataR(std::wstring_view resolvedFilename, const Metadata& metadata);
  void SetMetadata(std::wstring_view filename, const Metadata& metadata);
  bool RemoveMetadataR(std::wstring_view resolvedFilename);
  bool RemoveMetadata(std::wstring_view filename);
  bool ExistsR(std::wstring_view resolvedFilename) const;
  bool Exists(std::wstring_view filename) const;
  std::optional<bool> ExistsO(std::wstring_view filename) const;
  std::vector<std::pair<std::wstring, std::wstring>> ListChildrenInForwardLookupTree(std::wstring_view filename) const;
  std::vector<std::pair<std::wstring, std::wstring>> ListChildrenInReverseLookupTree(std::wstring_view filename) const;
  void Rename(std::wstring_view srcFilename, std::wstring_view destFilename);
  void Delete(std::wstring_view filename);
  bool RemoveRenameEntry(std::wstring_view filename);
};
