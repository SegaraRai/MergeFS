#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include <Windows.h>

#include "EncodingConverter.hpp"



namespace {
  template<UINT CodePage>
  std::wstring MBStringtoWString(std::string_view mbString) {
    if (mbString.empty()) {
      return {};
    }

    int wideCharLength = MultiByteToWideChar(CodePage, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, mbString.data(), static_cast<int>(mbString.size()), NULL, 0);
    if (!wideCharLength) {
      throw std::runtime_error("MultiByteToWideChar failed");
    }
    auto wideCharBuffer = std::make_unique<wchar_t[]>(wideCharLength);
    int wideCharLength2 = MultiByteToWideChar(CodePage, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, mbString.data(), static_cast<int>(mbString.size()), wideCharBuffer.get(), wideCharLength);
    if (!wideCharLength2) {
      throw std::runtime_error("MultiByteToWideChar failed");
    }
    return std::wstring(wideCharBuffer.get(), wideCharLength2);
  }
}



std::wstring LocaleStringtoWString(std::string_view localeString) {
  return MBStringtoWString<CP_OEMCP>(localeString);
}


std::wstring UTF8toWString(std::string_view utf8String) {
  return MBStringtoWString<CP_UTF8>(utf8String);
}


std::string WStringtoUTF8(std::wstring_view wString) {
  if (wString.empty()) {
    return {};
  }

  int utf8ByteLength = WideCharToMultiByte(CP_UTF8, 0, wString.data(), static_cast<int>(wString.size()), NULL, 0, NULL, NULL);
  if (!utf8ByteLength) {
    throw std::runtime_error("WideCharToMultiByte failed");
  }
  auto utf8Buffer = std::make_unique<char[]>(utf8ByteLength);
  int utf8ByteLength2 = WideCharToMultiByte(CP_UTF8, 0, wString.data(), static_cast<int>(wString.size()), utf8Buffer.get(), utf8ByteLength, NULL, NULL);
  if (!utf8ByteLength2) {
    throw std::runtime_error("WideCharToMultiByte failed");
  }
  return std::string(utf8Buffer.get(), utf8ByteLength2);
}
