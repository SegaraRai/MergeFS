#include <dokan/dokan.h>

#include <7z/CPP/Common/Common.h>

#include <mutex>

#include <Windows.h>
#include <winrt/base.h>

#include "SeekFilterStream.hpp"
#include "COMError.hpp"



InSeekFilterStream::InSeekFilterStream(winrt::com_ptr<IInStream> inStream, winrt::com_ptr<IInStream> baseInStream) :
  inStream(inStream),
  baseInStream(baseInStream),
  baseInStreamSeekOffset(0)
{
  COMError::CheckHRESULT(baseInStream->Seek(0, STREAM_SEEK_CUR, &baseInStreamSeekOffset));
}


InSeekFilterStream::InSeekFilterStream(winrt::com_ptr<IInStream> inStream) :
  InSeekFilterStream(inStream, inStream)
{}


STDMETHODIMP InSeekFilterStream::Read(void* data, UInt32 size, UInt32* processedSize) {
  std::lock_guard lock(mutex);
  UInt64 newSeekOffset = -1;
  if (const auto hResult = baseInStream->Seek(baseInStreamSeekOffset, STREAM_SEEK_SET, &newSeekOffset); FAILED(hResult)) {
    return hResult;
  }
  if (newSeekOffset != baseInStreamSeekOffset) {
    return E_FAIL;
  }
  if (const auto hResult = inStream->Read(data, size, processedSize); FAILED(hResult)) {
    return hResult;
  }
  if (const auto hResult = baseInStream->Seek(0, STREAM_SEEK_CUR, &newSeekOffset); FAILED(hResult)) {
    return hResult;
  }
  baseInStreamSeekOffset = newSeekOffset;
  return S_OK;
}


STDMETHODIMP InSeekFilterStream::Seek(Int64 offset, UInt32 seekOrigin, UInt64* newPosition) {
  std::lock_guard lock(mutex);
  if (seekOrigin == STREAM_SEEK_CUR && offset == 0) {
    return inStream->Seek(offset, seekOrigin, newPosition);
  }
  UInt64 newSeekOffset = -1;
  if (const auto hResult = baseInStream->Seek(baseInStreamSeekOffset, STREAM_SEEK_SET, &newSeekOffset); FAILED(hResult)) {
    return hResult;
  }
  if (newSeekOffset != baseInStreamSeekOffset) {
    return E_FAIL;
  }
  if (const auto hResult = inStream->Seek(offset, seekOrigin, newPosition); FAILED(hResult)) {
    return hResult;
  }
  if (const auto hResult = baseInStream->Seek(0, STREAM_SEEK_CUR, &newSeekOffset); FAILED(hResult)) {
    return hResult;
  }
  baseInStreamSeekOffset = newSeekOffset;
  return S_OK;
}
