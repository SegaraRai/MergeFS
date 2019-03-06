#pragma once

#include <7z/CPP/Common/Common.h>
#include <7z/CPP/7zip/IStream.h>

#include "7zGUID.hpp"

#include <mutex>

#include <Windows.h>
#include <winrt/base.h>


namespace InSeekFilterStreamInternal {
  struct __declspec(uuid("{6DE456E8-266B-4E91-3000-100000000000}")) IInSeekFilterStream : public IUnknown {
    STDMETHOD(GetBaseStream)(IInStream** outBaseInStream) PURE;
  };
}


// InSeekFilterStream is a wrapper class for IInStream which uses another (shared) IInStream
// this class restores seek position of the base IInStream before calling operations of target IInStream
// typical usage: wrap IInStream retrieved from IInArchiveGetStream (in this case, use IInStream used for IInArchive for baseInStream)
class InSeekFilterStream final : public winrt::implements<InSeekFilterStream, InSeekFilterStreamInternal::IInSeekFilterStream, IInStream, ISequentialInStream> {
  std::mutex mutex;
  winrt::com_ptr<IInStream> inStream;
  winrt::com_ptr<IInStream> baseInStream;
  UInt64 baseInStreamSeekOffset;

public:
  InSeekFilterStream(winrt::com_ptr<IInStream> inStream, winrt::com_ptr<IInStream> baseInStream);
  InSeekFilterStream(winrt::com_ptr<IInStream> inStream);

  // IInStream
  STDMETHOD(Read)(void* data, UInt32 size, UInt32* processedSize);
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64* newPosition);

  // IInSeekFilterStream
  STDMETHOD(GetBaseStream)(IInStream** outBaseInStream);
};
