#pragma once

#include <7z/CPP/Common/Common.h>
#include <7z/CPP/7zip/IStream.h>

#include "7zGUID.hpp"

#include <cstddef>
#include <mutex>

#include <Windows.h>
#include <winrt/base.h>


class InMemoryStream : public winrt::implements<InMemoryStream, IInStream, ISequentialInStream, IStreamGetSize> {
  std::mutex mutex;
  std::byte* data;
  std::size_t dataSize;
  std::size_t seekOffset;

public:
  InMemoryStream(std::byte* data, std::size_t dataSize);

  // IInStream
  STDMETHOD(Read)(void* data, UInt32 size, UInt32* processedSize);
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64* newPosition);

  // IStreamGetSize
  STDMETHOD(GetSize)(UInt64 *size);
};


class OutFixedMemoryStream : public winrt::implements<OutFixedMemoryStream, IOutStream, ISequentialOutStream, IStreamGetSize> {
  std::mutex mutex;
  std::byte* data;
  std::size_t maxDataSize;
  std::size_t dataSize;
  std::size_t seekOffset;

public:
  OutFixedMemoryStream(std::byte* data, std::size_t maxDataSize);

  OutFixedMemoryStream(const OutFixedMemoryStream&) = delete;
  OutFixedMemoryStream& operator=(const OutFixedMemoryStream&) = delete;
  OutFixedMemoryStream& operator=(OutFixedMemoryStream&&) = delete;

  // IOutStream
  STDMETHOD(Write)(const void* data, UInt32 size, UInt32* processedSize);
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64* newPosition);
  STDMETHOD(SetSize)(UInt64 newSize);

  // IStreamGetSize
  STDMETHOD(GetSize)(UInt64 *size);
};
