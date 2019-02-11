#pragma once

#include <7z/CPP/7zip/Archive/IArchive.h>

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <Windows.h>
#include <winrt/base.h>

#include "DLL.hpp"


class NanaZ {
public:
  struct Format {
    CLSID clsid;
    std::wstring name;
    std::vector<std::pair<std::wstring, std::wstring>> extensions;
    UInt32 flags;
    std::vector<std::vector<std::byte>> signatures;
    std::optional<std::size_t> signatureOffset;
    Func_IsArc IsArc;
  };

  class FormatStore {
    std::vector<Format> formats;
    std::vector<std::size_t> orderedFormatIndices;
    std::size_t maxSignatureSize;

  public:
    FormatStore(Func_GetIsArc GetIsArc, Func_GetNumberOfFormats GetNumberOfFormats, Func_GetHandlerProperty2 GetHandlerProperty2);

    const Format& GetFormat(std::size_t index) const;
    UInt32 IsArc(std::size_t index, const void* data, std::size_t size) const;
    std::vector<std::size_t> FindFormatByExtension(const std::wstring& extension) const;
    std::vector<std::size_t> FindFormatByStream(winrt::com_ptr<IInStream> inStream) const;
  };

private:
  const DLL nanaZDll;

public:
  const Func_CreateObject CreateObject;
  const Func_GetIsArc GetIsArc;
  const Func_GetNumberOfFormats GetNumberOfFormats;
  const Func_GetHandlerProperty2 GetHandlerProperty2;

private:
  FormatStore formatStore;

public:
  NanaZ(LPCWSTR dllFilepath);

  const Format& GetFormat(std::size_t index) const;
  UInt32 IsArc(std::size_t index, const void* data, std::size_t size) const;
  std::vector<std::size_t> FindFormatByExtension(const std::wstring& extension) const;
  std::vector<std::size_t> FindFormatByStream(winrt::com_ptr<IInStream> inStream) const;
};
