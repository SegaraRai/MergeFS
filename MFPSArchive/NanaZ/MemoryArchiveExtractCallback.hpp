#pragma once

#include <7z/CPP/Common/Common.h>
#include <7z/CPP/7zip/Archive/IArchive.h>
#include <7z/CPP/7zip/IPassword.h>
#include <7z/CPP/7zip/IProgress.h>

#include "7zGUID.hpp"
#include "MemoryStream.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <Windows.h>
#include <winrt/base.h>


class MemoryArchiveExtractCallback : public winrt::implements<MemoryArchiveExtractCallback, IArchiveExtractCallback, IProgress, ICryptoGetTextPassword> {
public:
  using PasswordCallback = std::function<std::optional<std::wstring>()>;

private:
  struct ObjectInfo {
    std::byte* data;
    std::size_t maxDataSize;
    winrt::com_ptr<OutFixedMemoryStream> outFixedMemoryStream;
    std::size_t extractedSize;
    Int32 operationResult;
  };

  std::unordered_map<UInt32, ObjectInfo> indexToInfoMap;
  PasswordCallback passwordCallback;
  UInt32 processingIndex;
  bool extracting;

public:
  static std::size_t CalcMemorySize(const std::vector<UInt64>& filesizes);

  MemoryArchiveExtractCallback(std::byte* storageBuffer, std::size_t storageBufferSize, const std::vector<std::pair<UInt32, UInt64>>& indexAndFilesizes, PasswordCallback passwordCallback = nullptr);

  std::byte* GetData(UInt32 index) const;
  std::size_t GetSize(UInt32 index) const;
  bool Succeeded(UInt32 index) const;

  // IArchiveExtractCallback
  STDMETHOD(GetStream)(UInt32 index, ISequentialOutStream** outStream, Int32 askExtractMode);
  STDMETHOD(PrepareOperation)(Int32 askExtractMode);
  STDMETHOD(SetOperationResult)(Int32 opRes);

  // IProgress
  STDMETHOD(SetTotal)(UInt64 total);
  STDMETHOD(SetCompleted)(const UInt64* completeValue);

  // ICryptoGetTextPassword
  STDMETHOD(CryptoGetTextPassword)(BSTR* password);
};
