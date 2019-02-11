#include <string>
#include <string_view>

#include "DirectoryTree.hpp"



const DirectoryTree* DirectoryTree::Get(std::wstring_view filepath) const {
  if (filepath.empty()) {
    return this;
  }
  if (!directory) {
    return nullptr;
  }
  const auto firstDelimiterPos = filepath.find_first_of(DirectorySeparator);
  auto itrChild = children.find(std::wstring(filepath.substr(0, firstDelimiterPos)));
  if (itrChild == children.end()) {
    return nullptr;
  }
  if (firstDelimiterPos == std::wstring_view::npos) {
    return &itrChild->second;
  }
  return itrChild->second.Get(filepath.substr(firstDelimiterPos + 1));
}


bool DirectoryTree::Exists(std::wstring_view filepath) const {
  return Get(filepath) != nullptr;
}
