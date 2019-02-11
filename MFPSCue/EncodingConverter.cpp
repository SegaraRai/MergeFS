#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>

#include <Windows.h>

#include "EncodingConverter.hpp"


// supports UTF-8 with BOM and OEM charset
std::wstring ConvertFileContentToWString(const std::byte* data, std::size_t size) {
  UINT codePage = CP_OEMCP;
  if (size >= 3 && data[0] == std::byte{0xEF} && data[1] == std::byte{0xBB} && data[2] == std::byte{0xBF}) {
    // UTF-8
    data += 3;
    size -= 3;
    codePage = CP_UTF8;
  }
  if (size >= 4 && data[0] == std::byte{0xFF} && data[1] == std::byte{0xFE} && data[2] == std::byte{0x00} && data[3] == std::byte{0x00}) {
    // UTF-32 LE
    throw std::runtime_error(u8"UTF-32 LE not supported");
  }
  if (size >= 4 && data[0] == std::byte{0x00} && data[1] == std::byte{0x00} && data[2] == std::byte{0xFE} && data[3] == std::byte{0xFF}) {
    // UTF-32 BE
    throw std::runtime_error(u8"UTF-32 BE not supported");
  }
  if (size >= 2 && data[0] == std::byte{0xFF} && data[1] == std::byte{0xFE}) {
    // UTF-16 LE
    data += 2;
    size -= 2;
    return std::wstring(reinterpret_cast<const wchar_t*>(data), size >> 1);
  }
  if (size >= 2 && data[0] == std::byte{0xFE} && data[1] == std::byte{0xFF}) {
    // UTF-16 BE
    throw std::runtime_error(u8"UTF-16 BE not supported");
  }
  // not unicode with BOM; assume OEM codepage
  int wideCharLength = MultiByteToWideChar(codePage, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, reinterpret_cast<const char*>(data), static_cast<int>(size), NULL, 0);
  if (!wideCharLength) {
    throw std::runtime_error(u8"MultiByteToWideChar failed");
  }
  auto wideCharBuffer = std::make_unique<wchar_t[]>(wideCharLength);
  int wideCharLength2 = MultiByteToWideChar(codePage, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, reinterpret_cast<const char*>(data), static_cast<int>(size), wideCharBuffer.get(), wideCharLength);
  if (!wideCharLength2) {
    throw std::runtime_error(u8"MultiByteToWideChar failed");
  }
  return std::wstring(wideCharBuffer.get(), wideCharLength2);
}


std::string ConvertWStringToCodePage(std::wstring_view data, UINT codePage) {
  int utf8ByteLength = WideCharToMultiByte(codePage, 0, data.data(), static_cast<int>(data.size()), NULL, 0, NULL, NULL);
  if (!utf8ByteLength) {
    throw std::runtime_error(u8"WideCharToMultiByte failed");
  }
  auto utf8Buffer = std::make_unique<char[]>(utf8ByteLength);
  int utf8ByteLength2 = WideCharToMultiByte(codePage, 0, data.data(), static_cast<int>(data.size()), utf8Buffer.get(), utf8ByteLength, NULL, NULL);
  if (!utf8ByteLength2) {
    throw std::runtime_error(u8"WideCharToMultiByte failed");
  }
  return std::string(utf8Buffer.get(), utf8ByteLength2);
}
