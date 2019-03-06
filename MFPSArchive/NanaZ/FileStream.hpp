#pragma once

#include "7zGUID.hpp"

#include <7z/CPP/Common/Common.h>
#include <7z/CPP/Common/MyCom.h>
#include <7z/CPP/7zip/IStream.h>

#include <mutex>
#include <unordered_map>

#include <Windows.h>
#include <winrt/base.h>

#define FILESTREAM_MANAGE_SEEK


class InFileStream final : public winrt::implements<InFileStream, IInStream, ISequentialInStream, IStreamGetSize> {
  bool needClose;
  HANDLE fileHandle;
  BY_HANDLE_FILE_INFORMATION byHandleFileInformation;
#ifdef FILESTREAM_MANAGE_SEEK
  std::mutex mutex;
  UInt64 seekOffset;
#endif
  UInt64 fileSize;

  void Init();

public:
  InFileStream(LPCWSTR filepath);
  InFileStream(HANDLE hFile);

  ~InFileStream();

  InFileStream(const InFileStream&) = delete;
  InFileStream& operator=(const InFileStream&) = delete;
  InFileStream& operator=(InFileStream&&) = delete;

  InFileStream(InFileStream&& other);

  // IInStream
  STDMETHOD(Read)(void* data, UInt32 size, UInt32* processedSize);
  STDMETHOD(Seek)(Int64 offset, UInt32 seekOrigin, UInt64* newPosition);

  // IStreamGetSize
  STDMETHOD(GetSize)(UInt64 *size);
};
