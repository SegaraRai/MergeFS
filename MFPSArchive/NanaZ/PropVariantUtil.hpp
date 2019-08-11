#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <Windows.h>
#include <Propidl.h>


template<typename T>
inline std::optional<T> FromPropVariantN(const PROPVARIANT& propVariant) {
  static_assert(false, "no suitable specialized template function");
}


template<>
inline std::optional<bool> FromPropVariantN(const PROPVARIANT& propVariant) {
  if (propVariant.vt == VT_EMPTY) {
    return std::nullopt;
  }
  if (propVariant.vt == VT_BOOL) {
    return propVariant.boolVal;
  }
  throw std::invalid_argument("property does not have a bool value");
}


template<>
inline std::optional<std::uint32_t> FromPropVariantN(const PROPVARIANT& propVariant) {
  if (propVariant.vt == VT_EMPTY) {
    return std::nullopt;
  }
  if (propVariant.vt == VT_UI1 || propVariant.vt == VT_UI2 || propVariant.vt == VT_UI4) {
    return std::make_optional(propVariant.ulVal);
  }
  throw std::invalid_argument("property does not have a uint32_t value");
}


template<>
inline std::optional<std::uint64_t> FromPropVariantN(const PROPVARIANT& propVariant) {
  if (propVariant.vt == VT_EMPTY) {
    return std::nullopt;
  }
  if (propVariant.vt == VT_UI1 || propVariant.vt == VT_UI2 || propVariant.vt == VT_UI4 || propVariant.vt == VT_UI8) {
    return std::make_optional(propVariant.uhVal.QuadPart);
  }
  throw std::invalid_argument("property does not have a uint64_t value");
}


template<>
inline std::optional<FILETIME> FromPropVariantN(const PROPVARIANT& propVariant) {
  if (propVariant.vt == VT_EMPTY) {
    return std::nullopt;
  }
  if (propVariant.vt == VT_FILETIME) {
    return std::make_optional<FILETIME>(propVariant.filetime);
  }
  throw std::invalid_argument("property does not have a string value");
}


template<>
inline std::optional<std::wstring> FromPropVariantN(const PROPVARIANT& propVariant) {
  if (propVariant.vt == VT_EMPTY) {
    return std::nullopt;
  }
  if (propVariant.vt == VT_BSTR) {
    // BSTR == WCHAR* == wchar_t*
    return std::make_optional<std::wstring>(propVariant.bstrVal);
  }
  throw std::invalid_argument("property does not have a string value");
}


// raw data (from BSTR)
template<>
inline std::optional<std::vector<std::byte>> FromPropVariantN(const PROPVARIANT& propVariant) {
  if (propVariant.vt == VT_EMPTY) {
    return std::nullopt;
  }
  if (propVariant.vt == VT_BSTR) {
    // BSTR == WCHAR* == wchar_t*
    std::vector<std::byte> ret(SysStringByteLen(propVariant.bstrVal));
    if (!ret.empty()) {
      std::memcpy(ret.data(), propVariant.bstrVal, ret.size());
    }
    return std::make_optional(ret);   // RVO
  }
  throw std::invalid_argument("property does not have a string value");
}


template<typename T>
inline T FromPropVariant(const PROPVARIANT& propVariant) {
  return FromPropVariantN<T>(propVariant).value();
}
