#include <dokan/dokan.h>

#include <7z/CPP/Common/Common.h>

#include "../SDK/Plugin/SourceCpp.hpp"

#include "../../Util/Common.hpp"

#include <cassert>
#include <mutex>
#include <stdexcept>

#include <Windows.h>

#include "FileStream.hpp"



void InFileStream::Init() {
  if (!GetFileInformationByHandle(fileHandle, &byHandleFileInformation)) {
    throw Win32Error();
  }
  if (byHandleFileInformation.dwFileAttributes != FILE_ATTRIBUTE_NORMAL && (byHandleFileInformation.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
    // cannot use directory for InFileStream
    throw Win32Error(ERROR_DIRECTORY_NOT_SUPPORTED);
  }
  fileSize = (static_cast<UInt64>(byHandleFileInformation.nFileSizeHigh) << 32) | byHandleFileInformation.nFileSizeLow;
  if (!SetFilePointerEx(fileHandle, util::CreateLargeInteger(0), nullptr, SEEK_SET)) {
    throw Win32Error();
  }
}


InFileStream::InFileStream(LPCWSTR filepath) :
  needClose(true),
  fileHandle(NULL),
  byHandleFileInformation{},
#ifdef FILESTREAM_MANAGE_SEEK
  seekOffset(0),
#endif
  fileSize(0)
{
  fileHandle = CreateFileW(filepath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (!util::IsValidHandle(fileHandle)) {
    throw Win32Error();
  }
  try {
    Init();
  } catch (...) {
    CloseHandle(fileHandle);
    fileHandle = NULL;
    throw;
  }
}


InFileStream::InFileStream(HANDLE hFile) :
  needClose(false),
  fileHandle(hFile),
  byHandleFileInformation{},
#ifdef FILESTREAM_MANAGE_SEEK
  seekOffset(0),
#endif
  fileSize(0)
{
  if (!util::IsValidHandle(fileHandle)) {
    throw Win32Error(ERROR_INVALID_HANDLE);
  }
  Init();
}


InFileStream::~InFileStream() {
  if (needClose && util::IsValidHandle(fileHandle)) {
    CloseHandle(fileHandle);
    fileHandle = NULL;
  }
}


InFileStream& InFileStream::operator=(InFileStream&& other) noexcept {
  needClose = other.needClose;
  fileHandle = other.fileHandle;
  byHandleFileInformation = other.byHandleFileInformation;
#ifdef FILESTREAM_MANAGE_SEEK
  seekOffset = other.seekOffset;
#endif
  fileSize = other.fileSize;
  other.fileHandle = NULL;
}


InFileStream::InFileStream(InFileStream&& other) noexcept {
  *this = std::move(other);
}


STDMETHODIMP InFileStream::Read(void* data, UInt32 size, UInt32* processedSize) {
  if (!util::IsValidHandle(fileHandle)) {
    return __HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE);
  }
  if (size == 0) {
    if (processedSize) {
      *processedSize = 0;
    }
    return S_OK;
  }
  static_assert(sizeof(DWORD) == sizeof(UInt32));
#ifdef FILESTREAM_MANAGE_SEEK
  static_assert(sizeof(LARGE_INTEGER) == sizeof(LARGE_INTEGER::QuadPart));
  static_assert(sizeof(LARGE_INTEGER) == sizeof(Int64));
  static_assert(sizeof(LARGE_INTEGER) == sizeof(UInt64));
  std::lock_guard lock(mutex);
  if (!SetFilePointerEx(fileHandle, util::CreateLargeInteger(seekOffset), NULL, SEEK_SET)) {
    return HRESULT_FROM_WIN32(GetLastError());
  }
#endif
  DWORD numberOfBytesRead = 0;
  if (!ReadFile(fileHandle, data, size, &numberOfBytesRead, NULL)) {
    return HRESULT_FROM_WIN32(GetLastError());
  }
  if (processedSize) {
    *processedSize = numberOfBytesRead;
  }
#ifdef FILESTREAM_MANAGE_SEEK
  /*
  using namespace std::literals;
  std::wstring debugStr = L"@READ  by: "s + std::to_wstring(GetCurrentThreadId()) + L", seekOffset: "s + std::to_wstring(seekOffset) + L", newOffset: "s + std::to_wstring(seekOffset + numberOfBytesRead) + L", size: "s + std::to_wstring(numberOfBytesRead) + L" / "s + std::to_wstring(size) + L"\n"s;
  OutputDebugStringW(debugStr.c_str());
  */
  seekOffset += numberOfBytesRead;
#endif

  return S_OK;
}


STDMETHODIMP InFileStream::Seek(Int64 offset, UInt32 seekOrigin, UInt64* newPosition) {
  if (!util::IsValidHandle(fileHandle)) {
    return __HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE);
  }
#ifdef FILESTREAM_MANAGE_SEEK
  std::lock_guard lock(mutex);
  Int64 newOffset;
  switch (seekOrigin) {
    case STREAM_SEEK_SET:
      newOffset = offset;
      break;

    case STREAM_SEEK_CUR:
      newOffset = seekOffset + offset;
      break;

    case STREAM_SEEK_END:
      newOffset = fileSize + offset;
      break;

    default:
      return STG_E_INVALIDFUNCTION;
  }
  if (newOffset < 0) {
    return __HRESULT_FROM_WIN32(ERROR_NEGATIVE_SEEK);
  } else if (static_cast<UInt64>(newOffset) > fileSize) {
    // over seeking seems to be allowed (cf. LimitedStreams.cpp)
    //newOffset = fileSize;
  }
  if (newPosition) {
    *newPosition = newOffset;
  }
  /*
  using namespace std::literals;
  std::wstring debugStr = L"@SEEK  by: "s + std::to_wstring(GetCurrentThreadId()) + L", seekOffset: "s + std::to_wstring(seekOffset) + L", newOffset: "s + std::to_wstring(newOffset) + L"\n"s;
  OutputDebugStringW(debugStr.c_str());
  */
  seekOffset = newOffset;
#else
  static_assert(STREAM_SEEK_SET == FILE_BEGIN);
  static_assert(STREAM_SEEK_CUR == FILE_CURRENT);
  static_assert(STREAM_SEEK_END == FILE_END);
  static_assert(sizeof(LARGE_INTEGER) == sizeof(LARGE_INTEGER::QuadPart));
  static_assert(sizeof(LARGE_INTEGER) == sizeof(Int64));
  static_assert(sizeof(LARGE_INTEGER) == sizeof(UInt64));
  /*
  if (!util::IsValidHandle(fileHandle)) {
    return __HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE);
  }
  //*/
  if (!SetFilePointerEx(fileHandle, *reinterpret_cast<LARGE_INTEGER*>(&offset), reinterpret_cast<LARGE_INTEGER*>(newPosition), seekOrigin)) {
    return HRESULT_FROM_WIN32(GetLastError());
  }
  if (newPosition) {
    assert(reinterpret_cast<LARGE_INTEGER*>(newPosition)->QuadPart >= 0);
  }
#endif
  return S_OK;
}


STDMETHODIMP InFileStream::GetSize(UInt64* size) {
  if (!util::IsValidHandle(fileHandle)) {
    return __HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE);
  }
  if (size) {
    *size = fileSize;
  }
  return S_OK;
}
