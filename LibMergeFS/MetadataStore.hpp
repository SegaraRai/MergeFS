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
  static constexpr auto RemovedPrefix = L"$MergeFSSystemData\\Removed";

private:
  std::shared_mutex mMutex;
  const bool mCaseSensitive;
  HANDLE mHFile = NULL;
  RenameStore mRenameStore;
  std::unordered_map<std::wstring, Metadata> mMetadataMap;

  std::wstring FilenameToKeyL(std::wstring_view filename) const;
  void LoadFromFileL();
  void SaveToFileL();

  std::optional<std::wstring> ResolveFilepathL(std::wstring_view filename) const;
  bool HasMetadataRL(std::wstring_view resolvedFilename) const;
  const Metadata& GetMetadataRL(std::wstring_view resolvedFilename) const;
  Metadata GetMetadata2RL(std::wstring_view resolvedFilename) const;
  void SetMetadataRL(std::wstring_view resolvedFilename, const Metadata& metadata);
  bool RemoveMetadataRL(std::wstring_view resolvedFilename);
  bool ExistsRL(std::wstring_view resolvedFilename) const;
  void RenameL(std::wstring_view srcFilename, std::wstring_view destFilename);

public:
  MetadataStore(const MetadataStore&) = delete;

  MetadataStore() = default;
  MetadataStore(std::wstring_view storeFileName, bool caseSensitive);
  ~MetadataStore();

  void SetFilePath(std::wstring_view storeFilename);
  std::optional<std::wstring> ResolveFilepath(std::wstring_view filename);
  bool HasMetadataR(std::wstring_view resolvedFilename);
  bool HasMetadata(std::wstring_view filename);
  const Metadata& GetMetadataR(std::wstring_view resolvedFilename);
  const Metadata& GetMetadata(std::wstring_view filename);
  Metadata GetMetadata2R(std::wstring_view resolvedFilename);
  Metadata GetMetadata2(std::wstring_view filename);
  void SetMetadataR(std::wstring_view resolvedFilename, const Metadata& metadata);
  void SetMetadata(std::wstring_view filename, const Metadata& metadata);
  bool RemoveMetadataR(std::wstring_view resolvedFilename);
  bool RemoveMetadata(std::wstring_view filename);
  bool ExistsR(std::wstring_view resolvedFilename);
  bool Exists(std::wstring_view filename);
  std::optional<bool> ExistsO(std::wstring_view filename);
  std::vector<std::pair<std::wstring, std::wstring>> ListChildrenInForwardLookupTree(std::wstring_view filename);
  std::vector<std::pair<std::wstring, std::wstring>> ListChildrenInReverseLookupTree(std::wstring_view filename);
  void Rename(std::wstring_view srcFilename, std::wstring_view destFilename);
  void Delete(std::wstring_view filename);
  bool RemoveRenameEntry(std::wstring_view filename);
};
