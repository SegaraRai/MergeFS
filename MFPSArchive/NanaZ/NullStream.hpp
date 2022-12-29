#pragma once

#include <7z/CPP/Common/Common.h>
#include <7z/CPP/7zip/IStream.h>

#include "7zGUID.hpp"

#include <mutex>

#include <Windows.h>
#include <winrt/base.h>


class InNullStream : public winrt::implements<InNullStream, IInStream, ISequentialInStream, IStreamGetSize> {
  std::mutex mutex;
  UInt64 dataSize;
  UInt64 seekOffset;

public:
  InNullStream(UInt64 dataSize);

  // IInStream
  STDMETHOD(Read)(void* data, UInt32 size, UInt32* processedSize);
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64* newPosition);

  // IStreamGetSize
  STDMETHOD(GetSize)(UInt64 *size);
};
