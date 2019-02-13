#include <7z/CPP/Common/Common.h>

#include "MemoryArchiveExtractCallback.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>

#include <Windows.h>
#include <winrt/base.h>


MemoryArchiveExtractCallback::MemoryArchiveExtractCallback(std::byte* storageBuffer, std::size_t storageBufferSize, const std::vector<std::pair<UInt32, UInt64>>& indexAndFilesizes, PasswordCallback passwordCallback) :
  passwordCallback(passwordCallback),
  processingIndex(-1),
  extracting(false)
{
  UInt64 offset = 0;
  for (std::size_t i = 0; i < indexAndFilesizes.size(); i++) {
    const auto& [index, fileSize] = indexAndFilesizes[i];
    auto currentFileDataBuffer = storageBuffer + offset;

    winrt::com_ptr<OutFixedMemoryStream> outFixedMemoryStream;
    outFixedMemoryStream.attach(new OutFixedMemoryStream(currentFileDataBuffer, fileSize));

    indexToInfoMap.emplace(index, ObjectInfo{
      currentFileDataBuffer,
      static_cast<std::size_t>(fileSize),
      outFixedMemoryStream,
      0,
      NArchive::NExtract::NOperationResult::kUnavailable,
    });

    offset += fileSize;
  }
}


std::byte* MemoryArchiveExtractCallback::GetData(UInt32 index) const {
  return indexToInfoMap.at(index).data;
}


std::size_t MemoryArchiveExtractCallback::GetSize(UInt32 index) const {
  return indexToInfoMap.at(index).extractedSize;
}


bool MemoryArchiveExtractCallback::Succeeded(UInt32 index) const {
  return indexToInfoMap.at(index).operationResult == NArchive::NExtract::NOperationResult::kOK;
}


STDMETHODIMP MemoryArchiveExtractCallback::GetStream(UInt32 index, ISequentialOutStream** outStream, Int32 askExtractMode) {
  processingIndex = index;
  if (!indexToInfoMap.count(index)) {
    return E_FAIL;
  }
  if (!outStream) {
    return S_OK;
  }
  ISequentialOutStream* sequentialOutStream = indexToInfoMap.at(index).outFixedMemoryStream.as<ISequentialOutStream>().get();
  sequentialOutStream->AddRef();
  *outStream = sequentialOutStream;
  return S_OK;
}


STDMETHODIMP MemoryArchiveExtractCallback::PrepareOperation(Int32 askExtractMode) {
  if (!indexToInfoMap.count(processingIndex)) {
    return S_OK;
  }
  extracting = askExtractMode == NArchive::NExtract::NAskMode::kExtract;
  return S_OK;
}


STDMETHODIMP MemoryArchiveExtractCallback::SetOperationResult(Int32 opRes) {
  if (!indexToInfoMap.count(processingIndex)) {
    return S_OK;
  }
  if (!extracting) {
    return S_OK;
  }
  auto& info = indexToInfoMap.at(processingIndex);
  info.operationResult = opRes;
  UInt64 streamSize;
  info.outFixedMemoryStream->GetSize(&streamSize);
  info.extractedSize = streamSize;
  return S_OK;
}


STDMETHODIMP MemoryArchiveExtractCallback::SetTotal([[maybe_unused]] UInt64 total) {
  return S_OK;
}


STDMETHODIMP MemoryArchiveExtractCallback::SetCompleted([[maybe_unused]] const UInt64* completeValue) {
  return S_OK;
}


STDMETHODIMP MemoryArchiveExtractCallback::CryptoGetTextPassword(BSTR* password) {
  try {
    if (!passwordCallback) {
      return E_ABORT;
    }
    const auto passwordN = passwordCallback();
    if (!passwordN) {
      return E_ABORT;
    }
    *password = ::SysAllocString(passwordN.value().c_str());
    return *password ? S_OK : E_OUTOFMEMORY;
  } catch (...) {}
  return E_FAIL;
}
