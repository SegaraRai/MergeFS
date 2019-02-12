#pragma once

#include <cstddef>
#include <string>


namespace CaseSensitivity {
  class CiEqualTo {
    bool mCaseSensitive;
    int(*mCompareFunc)(const wchar_t*, const wchar_t*);   // wcscmp or wcsicmp

  public:
    static std::size_t CaseSensitiveEqualTo(const std::wstring& a, const std::wstring& b);
    static std::size_t CaseInsensitiveEqualTo(const std::wstring& a, const std::wstring& b);
    static std::size_t EqualTo(const std::wstring& a, const std::wstring& b, bool caseSensitive);

    CiEqualTo(bool caseSensitive);
    bool operator() (const std::wstring& a, const std::wstring& b) const;
  };


  class CiHash {
    bool mCaseSensitive;
    std::size_t(*mHashFunc)(const std::wstring&);   // CaseSensitiveHash or CaseInsensitiveHash

  public:
    static std::size_t CaseSensitiveHash(const std::wstring& x);
    static std::size_t CaseInsensitiveHash(const std::wstring& x);
    static std::size_t Hash(const std::wstring& x, bool caseSensitive);

    CiHash(bool caseSensitive);
    std::size_t operator() (const std::wstring& x) const;
  };
}
