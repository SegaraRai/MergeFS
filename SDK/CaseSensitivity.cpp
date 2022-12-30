#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "CaseSensitivity.hpp"



namespace CaseSensitivity {
  std::size_t CiEqualTo::CaseSensitiveEqualTo(const std::wstring& a, const std::wstring& b) {
    return a == b;
  }


  std::size_t CiEqualTo::CaseInsensitiveEqualTo(const std::wstring& a, const std::wstring& b) {
    return a.size() == b.size() && _wcsicmp(a.c_str(), b.c_str()) == 0;
  }


  std::size_t CiEqualTo::EqualTo(const std::wstring& a, const std::wstring& b, bool caseSensitive) {
    return caseSensitive
      ? CaseSensitiveEqualTo(a, b)
      : CaseInsensitiveEqualTo(a, b);
  }


  CiEqualTo::CiEqualTo(bool caseSensitive) :
    mCaseSensitive(caseSensitive),
    mCompareFunc(caseSensitive ? std::wcscmp : _wcsicmp)
  {}


  bool CiEqualTo::operator() (const std::wstring& a, const std::wstring& b) const {
    return a.size() == b.size() && mCompareFunc(a.c_str(), b.c_str()) == 0;
  }



  std::size_t CiHash::CaseSensitiveHash(const std::wstring& x) {
    static std::hash<std::wstring> hashFunctor;
    return hashFunctor(x);
  }


  std::size_t CiHash::CaseInsensitiveHash(const std::wstring& x) {
    // FNV-1a
    static_assert(sizeof(wchar_t) == 2);
    static_assert(sizeof(std::size_t) == 4 || sizeof(std::size_t) == 8);
    constexpr std::size_t Prime = sizeof(std::size_t) == 4 ? 16777619 : 1099511628211;
    constexpr std::size_t Basis = sizeof(std::size_t) == 4 ? 2166136261 : 14695981039346656037;
    std::size_t hash = Basis;
    for (const auto& c : x) {
      const auto uc = static_cast<std::uint_fast16_t>(c);
      const auto uc1b = static_cast<std::uint_fast8_t>(uc & 0xFF) | 0x20;   // convert to lowercase
      const auto uc2b = static_cast<std::uint_fast8_t>(uc >> 8);
      hash ^= uc1b;
      hash *= Prime;
      hash ^= uc2b;
      hash *= Prime;
    }
    return hash;
  }


  std::size_t CiHash::Hash(const std::wstring& x, bool caseSensitive) {
    return caseSensitive
      ? CaseSensitiveHash(x)
      : CaseInsensitiveHash(x);
  }


  CiHash::CiHash(bool caseSensitive) :
    mCaseSensitive(caseSensitive),
    mHashFunc(caseSensitive ? CaseSensitiveHash : CaseInsensitiveHash)
  {}


  std::size_t CiHash::operator() (const std::wstring& x) const {
    return mHashFunc(x);
  }
}
