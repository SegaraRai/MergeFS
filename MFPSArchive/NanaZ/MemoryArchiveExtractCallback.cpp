#define NOMINMAX

#include <7z/CPP/Common/Common.h>

#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <string>

#include <Windows.h>
#include <winrt/base.h>

#include "MemoryArchiveExtractCallback.hpp"
#include "COMPtr.hpp"

using namespace std::literals;



namespace {
  constexpr std::size_t ExtraMemorySize = 0;
  constexpr std::size_t MemoryAlignment = 4096;
}



std::size_t MemoryArchiveExtractCallback::CalcMemorySize(const std::vector<UInt64>& filesizes) {
  std::size_t totalMemorySize = MemoryAlignment;

  for (const auto& filesize : filesizes) {
    if constexpr (std::numeric_limits<UInt64>::max() >= std::numeric_limits<std::size_t>::max() - ExtraMemorySize) {
      if (filesize > std::numeric_limits<std::size_t>::max() - ExtraMemorySize) {
        throw std::runtime_error(u8"too large file");
      }
    }

    std::size_t memorySize = static_cast<std::size_t>(filesize + ExtraMemorySize);
    if constexpr (MemoryAlignment > 1) {
      memorySize = (memorySize + MemoryAlignment - 1) / MemoryAlignment * MemoryAlignment;
    }

    if (totalMemorySize > std::numeric_limits<std::size_t>::max() - filesize) {
      throw std::runtime_error(u8"no memory");
    }

    totalMemorySize += memorySize;
  }

  return totalMemorySize;
}


MemoryArchiveExtractCallback::MemoryArchiveExtractCallback(std::byte* storageBuffer, std::size_t storageBufferSize, const std::vector<std::pair<UInt32, UInt64>>& indexAndFilesizes, PasswordCallback passwordCallback) :
  passwordCallback(passwordCallback),
  processingIndex(-1),
  extracting(false)
{
  std::byte* currentFileDataBuffer = storageBuffer;

  [[maybe_unused]] bool extraMemorySpaceForFirstTime = false;

  if constexpr (MemoryAlignment > 1) {
    void* alignedFileDataBuffer = currentFileDataBuffer;
    std::size_t alignedMemorySize = storageBufferSize;
    if (std::align(MemoryAlignment, storageBufferSize - MemoryAlignment, alignedFileDataBuffer, alignedMemorySize)) {
      currentFileDataBuffer = static_cast<std::byte*>(alignedFileDataBuffer);
    } else {
      assert(false);
      extraMemorySpaceForFirstTime = true;
    }
  }

  for (std::size_t i = 0; i < indexAndFilesizes.size(); i++) {
    const auto& [index, filesize] = indexAndFilesizes[i];

    std::size_t memorySize = static_cast<std::size_t>(filesize + ExtraMemorySize);
    if constexpr (MemoryAlignment > 1) {
      memorySize = (memorySize + MemoryAlignment - 1) / MemoryAlignment * MemoryAlignment;
    }

    void* alignedFileDataBuffer = currentFileDataBuffer;
    std::size_t alignedMemorySize = memorySize;
    if constexpr (MemoryAlignment > 1) {
      if (extraMemorySpaceForFirstTime) {
        alignedMemorySize += MemoryAlignment;
        extraMemorySpaceForFirstTime = false;
      }
      if (!std::align(MemoryAlignment, static_cast<std::size_t>(filesize), alignedFileDataBuffer, alignedMemorySize)) {
#ifdef _DEBUG
        const std::wstring debugStr = L"no aligned memory space for "s + std::to_wstring(i) + L"; offset: "s + std::to_wstring(currentFileDataBuffer - storageBuffer) + L", filesize: "s + std::to_wstring(filesize) + L", memsize: "s + std::to_wstring(memorySize) + L"\n"s;
        OutputDebugStringW(debugStr.c_str());
#endif
        alignedFileDataBuffer = currentFileDataBuffer;
        alignedMemorySize = memorySize;
      } else {
#ifdef _DEBUG
        const std::wstring debugStr = L"aligned memory space for "s + std::to_wstring(i) + L"; offset: "s + std::to_wstring(currentFileDataBuffer - storageBuffer) + L", alignedOffset: "s + std::to_wstring(static_cast<std::byte*>(alignedFileDataBuffer) - storageBuffer) + L", filesize: "s + std::to_wstring(filesize) + L", memsize: "s + std::to_wstring(memorySize) + L", alignedMemsize: "s + std::to_wstring(alignedMemorySize) + L"\n"s;
        OutputDebugStringW(debugStr.c_str());
#endif
      }
    }

    const std::size_t availableMemorySize = alignedMemorySize / MemoryAlignment * MemoryAlignment;
    assert(availableMemorySize >= filesize + ExtraMemorySize);

    auto outFixedMemoryStream = CreateCOMPtr(new OutFixedMemoryStream(static_cast<std::byte*>(alignedFileDataBuffer), availableMemorySize));

    indexToInfoMap.emplace(index, ObjectInfo{
      currentFileDataBuffer,
      memorySize,
      outFixedMemoryStream,
      0,
      NArchive::NExtract::NOperationResult::kUnavailable,
    });

    currentFileDataBuffer = static_cast<std::byte*>(alignedFileDataBuffer) + availableMemorySize;
  }

  if (currentFileDataBuffer > storageBuffer + storageBufferSize) {
    throw std::runtime_error("insufficient memory space provided");
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
  info.extractedSize = static_cast<std::size_t>(streamSize);
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
