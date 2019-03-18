#include <cassert>
#include <string>
#include <string_view>

#include "InternalFs.hpp"

using namespace std::literals;


namespace util::ifs {
  std::wstring_view GetParentPath(std::wstring_view filepath) {
    assert(!filepath.empty());
    const auto lastBackslashPos = filepath.find_last_of(L'\\');
    if (lastBackslashPos == std::wstring_view::npos) {
      return L""sv;
    }
    return filepath.substr(0, lastBackslashPos);
  }


  std::wstring_view GetParentPathTs(std::wstring_view filepath) {
    assert(!filepath.empty());
    const auto lastBackslashPos = filepath.find_last_of(L'\\');
    if (lastBackslashPos == std::wstring_view::npos) {
      return L""sv;
    }
    return filepath.substr(0, lastBackslashPos + 1);
  }
}
