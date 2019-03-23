#pragma once

#include <string>
#include <string_view>


namespace util::ifs {
  // functions for internal filesystem where paths are represented like "abc\\def.txt"
  // note that paths of internal filesystem do not have leading backslash

  std::wstring_view GetParentPath(std::wstring_view filepath);
  std::wstring_view GetParentPathTs(std::wstring_view filepath);    // GetParentPath with trailing backslash
  std::wstring_view GetBaseName(std::wstring_view filepath);
}
