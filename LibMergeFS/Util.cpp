#include "Util.hpp"
#include "NsError.hpp"

#include <cassert>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>

#include <winnls.h>

using namespace std::literals;


/*
パスの規則
- ルートは L"\\"
- 他は L"\\a\\\b\\c" のようにする
- ディレクトリ区切り記号は常に L"\\" （バックスラッシュ）
- 末尾に L"\\" （バックスラッシュ）をつけない
*/


bool IsValidHandle(HANDLE handle) noexcept {
  return handle != NULL && handle != INVALID_HANDLE_VALUE;
}


std::wstring FilenameToKey(std::wstring_view filename, bool caseSensitive) {
#if 0
  int bufferSize;
  std::unique_ptr<wchar_t[]> buffer;

  // normalize unicode
  bufferSize = NormalizeString(NormalizationC, filename.data(), filename.size(), nullptr, 0);
  if (bufferSize > 0) {
    buffer = std::make_unique<wchar_t[]>(bufferSize);
    const auto ret = NormalizeString(NormalizationC, filename.data(), filename.size(), buffer.get(), bufferSize);
    if (ret <= 0) {
      // normalization failed
      bufferSize = ret;
    }
  }

  // process without normalization when normalization failed
  if (bufferSize <= 0) {
    throw W32Error();
    /*
    bufferSize = filename.size();
    buffer = std::make_unique<wchar_t[]>(bufferSize);
    std::memcpy(buffer.get(), filename.data(), bufferSize);
    */
  }

  // to lowercase
  if (!caseSensitive) {
    for (std::size_t i = 0; i < bufferSize; i++) {
      if (L'A' <= buffer[i] && buffer[i] <= L'Z') {
        buffer[i] = buffer[i] - L'A' + L'a';
      }
    }
  }

  return std::wstring(buffer.get(), bufferSize);
#else
  std::wstring str(filename);

  if (!caseSensitive) {
    for (auto& c : str) {
      if (L'A' <= c && c <= L'Z') {
        c = c - L'A' + L'a';
      }
    }
  }

  return str;
#endif
}


std::wstring_view GetParentPath(std::wstring_view filename) {
  // ルートディレクトリの親ディレクトリを取得しようとするべきではない
  assert(filename.size() > 1);
  const std::size_t lastSlashPos = filename.find_last_of(L'\\');
  if (lastSlashPos == 0) {
    // root直下
    return L"\\"sv;
  }
  return filename.substr(0, lastSlashPos);
  //return std::wstring{filename.cbegin(), filename.cbegin() + lastSlashPos};
}


std::wstring_view GetBaseName(std::wstring_view filename) {
  // ルートディレクトリのベース名を取得しようとするべきではない
  assert(filename.size() > 1);
  const std::size_t lastSlashPos = filename.find_last_of(L'\\');
  assert(lastSlashPos != std::wstring_view::npos);
  if (lastSlashPos == std::wstring_view::npos) {
    // 不正な形式
    return filename;
  }
  return filename.substr(lastSlashPos + 1);
}


bool IsRootDirectory(LPCWSTR filename) noexcept {
  return filename[0] == L'\0' || filename[1] == L'\0';
}


bool IsRootDirectory(std::wstring_view filename) noexcept {
  return filename.size() <= 1;
  //return filename == L""sv || filename == L"\\"sv;
}
