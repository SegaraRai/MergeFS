#define NOMINMAX

#include <7z/CPP/Common/Common.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <utility>

#include <Windows.h>

#include "MemoryStream.hpp"



InMemoryStream::InMemoryStream(std::byte* data, std::size_t dataSize) :
  mutex(),
  data(data),
  dataSize(dataSize),
  seekOffset(0)
{}


STDMETHODIMP InMemoryStream::Read(void* data, UInt32 size, UInt32* processedSize) {
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
    static_assert(sizeof(std::size_t) >= sizeof(UInt32));
    readSize = static_cast<UInt32>(std::min<std::size_t>(std::min<std::size_t>(size, dataSize - seekOffset), std::numeric_limits<UInt32>::max()));
    seekOffset += readSize;
  }
  std::memcpy(data, this->data + oldSeekOffset, readSize);
  if (processedSize) {
    *processedSize = readSize;
  }
  // TODO: is partial read success?
  return S_OK;
}


STDMETHODIMP InMemoryStream::Seek(Int64 offset, UInt32 seekOrigin, UInt64* newPosition) {
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


STDMETHODIMP InMemoryStream::GetSize(UInt64* size) {
  if (!size) {
    return S_OK;
  }
  *size = dataSize;
  return S_OK;
}



OutFixedMemoryStream::OutFixedMemoryStream(std::byte* data, std::size_t maxDataSize) :
  mutex(),
  data(data),
  maxDataSize(maxDataSize),
  dataSize(0),
  seekOffset(0)
{}


STDMETHODIMP OutFixedMemoryStream::Write(const void* data, UInt32 size, UInt32* processedSize) {
  if (size == 0) {
    if (processedSize) {
      *processedSize = 0;
    }
    return S_OK;
  }
  UInt32 writeSize;
  UInt64 oldSeekOffset;
  {
    std::lock_guard lock(mutex);
    if (seekOffset >= maxDataSize) {
      return E_FAIL;
    }
    oldSeekOffset = seekOffset;
    static_assert(sizeof(std::size_t) >= sizeof(UInt32));
    writeSize = static_cast<UInt32>(std::min<std::size_t>(std::min<std::size_t>(size, maxDataSize - seekOffset), std::numeric_limits<UInt32>::max()));
    seekOffset += writeSize;
    dataSize = std::max(dataSize, seekOffset);
  }
  std::memcpy(this->data + oldSeekOffset, data, writeSize);
  if (processedSize) {
    *processedSize = writeSize;
  }
  // TODO: is partial write success?
  return S_OK;
}


STDMETHODIMP OutFixedMemoryStream::Seek(Int64 offset, UInt32 seekOrigin, UInt64* newPosition) {
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
    // allow over seeking
    //newOffset = dataSize;
  }
  if (newPosition) {
    *newPosition = newOffset;
  }
  seekOffset = newOffset;
  return S_OK;
}


STDMETHODIMP OutFixedMemoryStream::SetSize(UInt64 newSize) {
  if (newSize > maxDataSize) {
    return E_OUTOFMEMORY;
  }
  std::lock_guard lock(mutex);
  // TODO: zero fill?
  // TODO: set seekOffset?
  dataSize = newSize;
  return S_OK;
}


STDMETHODIMP OutFixedMemoryStream::GetSize(UInt64* size) {
  if (!size) {
    return S_OK;
  }
  std::lock_guard lock(mutex);
  *size = dataSize;
  return S_OK;
}
