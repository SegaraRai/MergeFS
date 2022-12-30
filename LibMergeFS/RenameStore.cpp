#include "RenameStore.hpp"
#include "Util.hpp"

#include "../SDK/CaseSensitivity.hpp"

#include "../Util/VirtualFs.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

using namespace std::literals;



namespace {
  static const std::wstring StrDelimiter = L"\\"s;
}



RenameStore::PathTrieTree::PathTrieTree(bool caseSensitive) :
  mCaseSensitive(caseSensitive),
  mChildren(0, CaseSensitivity::CiHash(caseSensitive), CaseSensitivity::CiEqualTo(caseSensitive)),
  mValid(false),
  mFilepath()
{}


RenameStore::PathTrieTree::PathTrieTree(bool caseSensitive, std::wstring_view filepath) :
  mCaseSensitive(caseSensitive),
  mChildren(0, CaseSensitivity::CiHash(caseSensitive), CaseSensitivity::CiEqualTo(caseSensitive)),
  mValid(true),
  mFilepath(filepath)
{}


void RenameStore::PathTrieTree::Traverse(const std::wstring& prefix, std::function<void(const std::wstring&, const std::wstring&, bool)>& callback) const {
  for (const auto& [key, value] : mChildren) {
    const std::wstring filepath = prefix + key;
    callback(filepath, value.mFilepath, value.mValid);
    value.Traverse(filepath + StrDelimiter, callback);
  }
}


bool RenameStore::PathTrieTree::IsCaseSensitive() const {
  return mCaseSensitive;
}


bool RenameStore::PathTrieTree::IsValid() const {
  return mValid;
}


const std::wstring& RenameStore::PathTrieTree::GetData() const {
  return mFilepath;
}


void RenameStore::PathTrieTree::SetData(std::wstring_view filepath) {
  mFilepath = filepath;
  mValid = true;
}


void RenameStore::PathTrieTree::Invalidate() {
  mFilepath.clear();
  mValid = false;
}


bool RenameStore::PathTrieTree::Has(std::wstring_view filepath, bool validOnly) const {
  const auto itr = mChildren.find(std::wstring(filepath));
  if (itr == mChildren.cend()) {
    return false;
  }
  return !validOnly || itr->second.mValid;
}


RenameStore::PathTrieTree* RenameStore::PathTrieTree::Get(std::wstring_view filepath, bool validOnly) {
  auto itr = mChildren.find(std::wstring(filepath));
  if (itr == mChildren.end()) {
    return nullptr;
  }
  return !validOnly || itr->second.mValid ? &itr->second : nullptr;
}


const RenameStore::PathTrieTree* RenameStore::PathTrieTree::Get(std::wstring_view filepath, bool validOnly) const {
  const auto itr = mChildren.find(std::wstring(filepath));
  if (itr == mChildren.cend()) {
    return nullptr;
  }
  return !validOnly || itr->second.mValid ? &itr->second : nullptr;
}


bool RenameStore::PathTrieTree::Remove(std::wstring_view filepath) {
  return mChildren.erase(std::wstring(filepath));
}


std::vector<std::pair<std::wstring, std::wstring>> RenameStore::PathTrieTree::ListChildren(bool validOnly) const {
  std::vector<std::pair<std::wstring, std::wstring>> children;
  children.reserve(mChildren.size());
  for (const auto& [key, value] : mChildren) {
    if (validOnly && !value.mValid) {
      continue;
    }
    children.emplace_back(key, value.mFilepath);
  }
  return children;
}


RenameStore::PathTrieTree* RenameStore::PathTrieTree::RetrieveRecursive(std::wstring_view key) {
  const auto firstDelimiterPos = key.find_first_of(Delimiter);
  auto itrChild = mChildren.find(std::wstring(key.substr(0, firstDelimiterPos)));
  if (itrChild == mChildren.end()) {
    // child not found
    return nullptr;
  }
  if (firstDelimiterPos == std::wstring_view::npos) {
    return &itrChild->second;
  }
  return itrChild->second.RetrieveRecursive(key.substr(firstDelimiterPos + 1));
}


const RenameStore::PathTrieTree* RenameStore::PathTrieTree::RetrieveRecursive(std::wstring_view key) const {
  const auto firstDelimiterPos = key.find_first_of(Delimiter);
  const auto itrChild = mChildren.find(std::wstring(key.substr(0, firstDelimiterPos)));
  if (itrChild == mChildren.cend()) {
    // child not found
    return nullptr;
  }
  if (firstDelimiterPos == std::wstring_view::npos) {
    return &itrChild->second;
  }
  return itrChild->second.RetrieveRecursive(key.substr(firstDelimiterPos + 1));
}


bool RenameStore::PathTrieTree::Match(std::wstring_view key) const {
  // returns !!FindLongestMatch(key);
  if (mValid) {
    return true;
  }
  const auto firstDelimiterPos = key.find_first_of(Delimiter);
  const auto itrChild = mChildren.find(std::wstring(key.substr(0, firstDelimiterPos)));
  if (itrChild == mChildren.end()) {
    // child not found
    return false;
  }
  return itrChild->second.Match(key.substr(firstDelimiterPos + 1));
}


std::optional<std::pair<std::wstring, std::wstring>> RenameStore::PathTrieTree::FindLongestMatch(std::wstring_view key) const {
  const auto firstDelimiterPos = key.find_first_of(Delimiter);
  const std::wstring childKey(key.substr(0, firstDelimiterPos));
  const auto itrChild = mChildren.find(childKey);
  if (itrChild != mChildren.end()) {
    auto result = itrChild->second.FindLongestMatch(key.substr(firstDelimiterPos + 1));
    if (result) {
      result.value().first = result.value().first.empty() ? childKey : childKey + StrDelimiter + result.value().first;
      return result;
    }
  }
  if (mValid) {
    return std::make_pair(L""s, mFilepath);
  }
  return std::nullopt;
}


void RenameStore::PathTrieTree::Traverse(std::function<void(const std::wstring&, const std::wstring&, bool)> callback, bool self) const {
  if (self) {
    callback(L""s, mFilepath, mValid);
  }
  Traverse(L""s, callback);
}


std::size_t RenameStore::PathTrieTree::CleanupUnusedNodes(std::wstring_view key) {
  // remove nodes
  std::deque<std::tuple<PathTrieTree*, std::wstring_view, PathTrieTree*>> nodes;
  PathTrieTree* ptrCurrentNode = this;
  std::wstring_view leftKey = key;
  while (true) {
    const auto firstDelimiterPos = leftKey.find_first_of(Delimiter);
    if (firstDelimiterPos == std::wstring_view::npos) {
      break;
    }
    const std::wstring_view currentKey = leftKey.substr(0, firstDelimiterPos);
    leftKey.remove_prefix(firstDelimiterPos + 1);
    auto ptrChildNode = ptrCurrentNode->Get(currentKey, false);
    if (!ptrChildNode) {
      break;
    }
    nodes.emplace_front(ptrCurrentNode, currentKey, ptrChildNode);
    ptrCurrentNode = ptrChildNode;
  }

  std::size_t removedCount = 0;
  for (const auto&[ptrParentNode, key, ptrChildNode] : nodes) {
    if (ptrChildNode->mValid || !ptrChildNode->mChildren.empty()) {
      break;
    }
    ptrParentNode->Remove(key);
    removedCount++;
  }

  return removedCount;
}


std::pair<RenameStore::PathTrieTree&, bool> RenameStore::PathTrieTree::InsertRecursive(std::wstring_view key) {
  const auto firstDelimiterPos = key.find_first_of(Delimiter);
  const std::wstring childKey(key.substr(0, firstDelimiterPos));
  bool emplaced = false;
  auto itrChild = mChildren.find(childKey);
  if (itrChild == mChildren.end()) {
    itrChild = mChildren.emplace(childKey, PathTrieTree(this)).first;
    emplaced = true;
  }
  if (firstDelimiterPos == std::wstring_view::npos) {
    return {itrChild->second, emplaced};
  }
  return itrChild->second.InsertRecursive(key.substr(firstDelimiterPos + 1));
}


std::pair<RenameStore::PathTrieTree&, bool> RenameStore::PathTrieTree::InsertRecursive(std::wstring_view key, std::wstring_view filepath) {
  const auto firstDelimiterPos = key.find_first_of(Delimiter);
  const std::wstring childKey(key.substr(0, firstDelimiterPos));
  auto itrChild = mChildren.find(childKey);
  if (itrChild == mChildren.end()) {
    itrChild = mChildren.emplace(childKey, PathTrieTree(this)).first;
  }
  if (firstDelimiterPos == std::wstring_view::npos) {
    if (itrChild->second.mValid) {
      return {itrChild->second, false};
    }
    itrChild->second.SetData(filepath);
    return {itrChild->second, true};
  }
  return itrChild->second.InsertRecursive(key.substr(firstDelimiterPos + 1), filepath);
}


std::optional<std::pair<std::wstring, bool>> RenameStore::PathTrieTree::ResetEntry(std::wstring_view key) {
  auto ptrNode = RetrieveRecursive(key);
  if (!ptrNode) {
    return std::nullopt;
  }
  auto ret = std::make_pair(ptrNode->mFilepath, ptrNode->mValid);
  ptrNode->Invalidate();
  CleanupUnusedNodes(key);
  return ret;
}


RenameStore::PathTrieTree* RenameStore::PathTrieTree::MoveNode(std::wstring_view source, std::wstring_view destination) {
  // get parent node of source
  std::wstring_view sourceBaseKey;
  RenameStore::PathTrieTree* ptrSourceParentTree = nullptr;
  if (auto lastSourceDelimiterPos = source.find_last_of(Delimiter); lastSourceDelimiterPos != std::wstring_view::npos) {
    sourceBaseKey = source.substr(lastSourceDelimiterPos + 1);
    auto sourceParentKey = source.substr(0, lastSourceDelimiterPos);
    ptrSourceParentTree = RetrieveRecursive(sourceParentKey);
    if (!ptrSourceParentTree) {
      // not exists
      return nullptr;
    }
  } else {
    sourceBaseKey = source;
    ptrSourceParentTree = this;
  }

  // get target node
  auto ptrSourceTargetNode = ptrSourceParentTree->Get(sourceBaseKey, false);
  if (!ptrSourceTargetNode) {
    // not exists
    return nullptr;
  }

  // get parent node of destination
  std::wstring_view destinationBaseKey;
  RenameStore::PathTrieTree* ptrDestinationParentTree = nullptr;
  if (auto lastDestinationDelimiterPos = destination.find_last_of(Delimiter); lastDestinationDelimiterPos != std::wstring_view::npos) {
    destinationBaseKey = destination.substr(lastDestinationDelimiterPos + 1);
    auto destinationParentKey = destination.substr(0, lastDestinationDelimiterPos);
    ptrDestinationParentTree = &InsertRecursive(destinationParentKey).first;
  } else {
    destinationBaseKey = destination;
    ptrDestinationParentTree = this;
  }

  auto ptrDestinationTargetNode = &ptrDestinationParentTree->InsertRecursive(destinationBaseKey).first;
  if (ptrDestinationTargetNode == ptrSourceTargetNode) {
    // same name but different letter case
    PathTrieTree tempTree(std::move(*ptrSourceTargetNode));
    ptrSourceParentTree->Remove(sourceBaseKey);
    ptrSourceParentTree->InsertRecursive(destinationBaseKey).first = std::move(tempTree);
  } else {
    if (ptrDestinationTargetNode->mValid) {
      // already exists
      return nullptr;
    }

    *ptrDestinationTargetNode = std::move(*ptrSourceTargetNode);
    ptrSourceParentTree->Remove(sourceBaseKey);
  }

  // remove unnecessary nodes
  CleanupUnusedNodes(source);

  return ptrDestinationTargetNode;
}



RenameStore::RenameStore(bool caseSensitive) :
  mCaseSensitive(caseSensitive),
  mForwardLookupTree(caseSensitive),
  mReverseLookupTree(caseSensitive)
{}


void RenameStore::AddEntry(std::wstring_view originalFilepath, std::wstring_view renamedFilepath) {
  mForwardLookupTree.InsertRecursive(renamedFilepath.substr(1), originalFilepath);
  mReverseLookupTree.InsertRecursive(originalFilepath.substr(1), renamedFilepath);
}


std::vector<std::pair<std::wstring, std::wstring>> RenameStore::GetEntries() const {
  std::vector<std::pair<std::wstring, std::wstring>> result;
  mForwardLookupTree.Traverse([&result](const std::wstring& fullKey, const std::wstring& filepath, bool isValid) -> void {
    if (!isValid) {
      return;
    }
    result.emplace_back(L"\\"s + fullKey, filepath);
  }, false);
  return result;
}


std::vector<std::pair<std::wstring, std::wstring>> RenameStore::ListChildrenInForwardLookupTree(std::wstring_view filepath) const {
  // trim leading backslash
  const auto trimedFilepath = filepath.substr(1);
  const auto ptrTree = trimedFilepath.empty() ? &mForwardLookupTree : mForwardLookupTree.RetrieveRecursive(trimedFilepath);
  if (!ptrTree) {
    return {};
  }
  return ptrTree->ListChildren(true);
}


std::vector<std::pair<std::wstring, std::wstring>> RenameStore::ListChildrenInReverseLookupTree(std::wstring_view filepath) const {
  // trim leading backslash
  const auto trimedFilepath = filepath.substr(1);
  const auto ptrTree = trimedFilepath.empty() ? &mReverseLookupTree : mReverseLookupTree.RetrieveRecursive(trimedFilepath);
  if (!ptrTree) {
    return {};
  }
  return ptrTree->ListChildren(true);
}


std::optional<bool> RenameStore::Exists(std::wstring_view filepath) const {
  if (util::vfs::IsRootDirectory(filepath)) {
    return std::nullopt;
  }
  // trim leading backslash
  const auto trimedFilepath = filepath.substr(1);
  if (mForwardLookupTree.Match(trimedFilepath)) {
    return true;
  }
  if (mReverseLookupTree.Match(trimedFilepath)) {
    return false;
  }
  return std::nullopt;
}


std::optional<std::wstring> RenameStore::Resolve(std::wstring_view filepath) const {
  if (util::vfs::IsRootDirectory(filepath)) {
    return std::wstring(filepath);
  }
  // trim leading backslash
  const auto trimedFilepath = filepath.substr(1);
  const auto forwardLongestMatch = mForwardLookupTree.FindLongestMatch(trimedFilepath);
  if (forwardLongestMatch) {
    return forwardLongestMatch.value().second + std::wstring(trimedFilepath.substr(forwardLongestMatch.value().first.size()));
  }
  const auto reverseMatch = mReverseLookupTree.Match(trimedFilepath);
  if (reverseMatch) {
    return std::nullopt;
  }
  return std::wstring(filepath);
}


RenameStore::Result RenameStore::Rename(std::wstring_view srcFilepath, std::wstring_view destFilepath) {
  if (util::vfs::IsRootDirectory(srcFilepath) || util::vfs::IsRootDirectory(destFilepath)) {
    return Result::Invalid;
  }

  if (srcFilepath == destFilepath) {
    return Result::Success;
  }

  const auto trimedSrcFilepath = srcFilepath.substr(1);
  const auto trimedDestFilepath = destFilepath.substr(1);

  const auto resolvedSrcFilepath = Resolve(srcFilepath);
  if (!resolvedSrcFilepath) {
    return Result::NotExists;
  }

  // modify forawrd lookup tree
  PathTrieTree* ptrForwardDestinationNode = nullptr;
  if (auto ptrForwardSourceNode = mForwardLookupTree.RetrieveRecursive(trimedSrcFilepath); ptrForwardSourceNode) {
    // node already exists; move children
    ptrForwardDestinationNode = mForwardLookupTree.MoveNode(trimedSrcFilepath, trimedDestFilepath);
    if (!ptrForwardDestinationNode) {
      return Result::AlreadyExists;
    }
  } else {
    // create node
    ptrForwardDestinationNode = &mForwardLookupTree.InsertRecursive(trimedDestFilepath).first;
  }
  ptrForwardDestinationNode->SetData(resolvedSrcFilepath.value());    // use resolved one

  // modify reverse lookup tree
  mReverseLookupTree.InsertRecursive(trimedSrcFilepath, destFilepath);
  const std::size_t oldPrefixLength = srcFilepath.size();   // not resolved one
  const std::wstring newPrefix(destFilepath);
  ptrForwardDestinationNode->Traverse([this, oldPrefixLength, &newPrefix](const std::wstring& key, const std::wstring& value, bool valid) {
    if (!valid) {
      return;
    }
    auto ptrNode = mReverseLookupTree.RetrieveRecursive(std::wstring_view(value).substr(1));
    assert(ptrNode && ptrNode->IsValid());
    if (!ptrNode) {
      // TODO: error handling
      return;
    }
    if (!ptrNode->IsValid()) {
      // TODO: error handling
      return;
    }
    ptrNode->SetData(newPrefix + ptrNode->GetData().substr(oldPrefixLength));
  }, false);

  return Result::Success;
}


bool RenameStore::RemoveEntry(std::wstring_view filepath) {
  if (util::vfs::IsRootDirectory(filepath)) {
    return false;
  }
  const auto trimedFilepath = filepath.substr(1);
  auto forwardResult = mForwardLookupTree.ResetEntry(trimedFilepath);
  if (!forwardResult) {
    return false;
  }
  auto& [resolved, forwardValid] = forwardResult.value();
  if (!forwardValid) {
    return false;
  }
  const auto trimedResolved = std::wstring_view(resolved).substr(1);
  auto reverseResult = mReverseLookupTree.ResetEntry(trimedResolved);
  if (!reverseResult || !reverseResult.value().second) {
    // error
    return false;
  }
  return true;
}
