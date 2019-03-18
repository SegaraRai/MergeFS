#include <cassert>
#include <string>
#include <string_view>

#include <Windows.h>

using namespace std::literals;



namespace util::vfs {
  bool IsRootDirectory(LPCWSTR filepath) noexcept {
    assert(filepath[0] == L'\\');
    return filepath[0] == L'\0' || filepath[1] == L'\0';
  }


  bool IsRootDirectory(std::wstring_view filepath) noexcept {
    assert(filepath.size() >= 1);
    if (!filepath.empty()) {
      assert(filepath[0] == L'\\');
    }
    return filepath.size() < 2;
  }


  std::wstring_view GetParentPath(std::wstring_view filepath) {
    assert(!IsRootDirectory(filepath));
    const auto lastBackslashPos = filepath.find_last_of(L'\\');
    if (lastBackslashPos == std::wstring_view::npos || lastBackslashPos == 0) {
      return L"\\"sv;
    }
    return filepath.substr(0, lastBackslashPos);
  }


  std::wstring_view GetParentPathTs(std::wstring_view filepath) {
    assert(!IsRootDirectory(filepath));
    const auto lastBackslashPos = filepath.find_last_of(L'\\');
    if (lastBackslashPos == std::wstring_view::npos || lastBackslashPos == 0) {
      return L"\\"sv;
    }
    return filepath.substr(0, lastBackslashPos + 1);
  }
}
