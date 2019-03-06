#define NOMINMAX

#include <7z/CPP/Common/Common.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <mutex>

#include <Windows.h>

#include "NullStream.hpp"



InNullStream::InNullStream(UInt64 dataSize) :
  mutex(),
  dataSize(dataSize),
  seekOffset(0)
{}


STDMETHODIMP InNullStream::Read(void* data, UInt32 size, UInt32* processedSize) {
  UInt32 readSize;
  UInt64 oldSeekOffset;
  {
    std::lock_guard lock(mutex);
    if (size == 0 || seekOffset >= dataSize) {
      // over seeking seems to be allowed (cf. LimitedStreams.cpp)
      if (processedSize) {
        *processedSize = 0;
      }
      return S_OK;
    }
    oldSeekOffset = seekOffset;
    readSize = static_cast<UInt32>(std::min<UInt64>(std::min<UInt64>(size, dataSize - seekOffset), std::numeric_limits<UInt32>::max()));
    seekOffset += readSize;
  }
  std::memset(data, 0, readSize);
  if (processedSize) {
    *processedSize = readSize;
  }
  // TODO: is partial read success?
  return S_OK;
}


STDMETHODIMP InNullStream::Seek(Int64 offset, UInt32 seekOrigin, UInt64* newPosition) {
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
      newOffset = dataSize + offset;
      break;

    default:
      return STG_E_INVALIDFUNCTION;
  }
  if (newOffset < 0) {
    return __HRESULT_FROM_WIN32(ERROR_NEGATIVE_SEEK);
  } else if (static_cast<UInt64>(newOffset) > dataSize) {
    // over seeking seems to be allowed (cf. LimitedStreams.cpp)
    //newOffset = dataSize;
  }
  if (newPosition) {
    *newPosition = newOffset;
  }
  seekOffset = newOffset;
  return S_OK;
}


STDMETHODIMP InNullStream::GetSize(UInt64* size) {
  if (!size) {
    return S_OK;
  }
  *size = dataSize;
  return S_OK;
}
