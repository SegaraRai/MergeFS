#define NOMINMAX

#include <dokan/dokan.h>

#include <7z/CPP/Common/Common.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <Windows.h>

#include "NanaZ.hpp"
#include "COMError.hpp"
#include "PropVariantUtil.hpp"
#include "PropVariantWrapper.hpp"

using namespace std::literals;



namespace {
  constexpr int DefaultFormatPriority = 0;
  const std::unordered_map<std::wstring, int> gFormatPriorityMap{
    // Prioritize Udf over Iso
    {L"Udf"s, 100},
    {L"Iso"s, 0},
  };


  template<typename T>
  std::optional<T> GetPropertyN(Func_GetHandlerProperty2 GetHandlerProperty2, UInt32 index, PROPID propId) {
    PropVariantWrapper propVariant;
    COMError::CheckHRESULT(GetHandlerProperty2(index, propId, &propVariant));
    return FromPropVariantN<T>(propVariant);
  }


  template<typename T>
  T GetProperty(Func_GetHandlerProperty2 GetHandlerProperty2, UInt32 index, PROPID propId) {
    return GetPropertyN<T>(GetHandlerProperty2, index, propId).value();
  }
}



NanaZ::FormatStore::FormatStore(Func_GetIsArc GetIsArc, Func_GetNumberOfFormats GetNumberOfFormats, Func_GetHandlerProperty2 GetHandlerProperty2) {
  maxSignatureSize = 0;

  UInt32 numFormats = 1;
  COMError::CheckHRESULT(GetNumberOfFormats(&numFormats));

  formats.resize(numFormats);

  for (UInt32 formatIndex = 0; formatIndex < numFormats; formatIndex++) {
    auto& format = formats[formatIndex];

    // retrieve format.name
    format.name = GetProperty<std::wstring>(GetHandlerProperty2, formatIndex, NArchive::NHandlerPropID::kName);

    // retrieve format.clsid
    const auto clsidV = GetProperty<std::vector<std::byte>>(GetHandlerProperty2, formatIndex, NArchive::NHandlerPropID::kClassID);
    if (clsidV.size() != sizeof(CLSID)) {
      throw std::invalid_argument("invalid CLSID given");
    }
    //clsid = *reinterpret_cast<const CLSID*>(clsidV.data());
    std::memcpy(&format.clsid, clsidV.data(), sizeof(CLSID));

    // retrieve format.extensions
    const auto extensionN = GetPropertyN<std::wstring>(GetHandlerProperty2, formatIndex, NArchive::NHandlerPropID::kExtension);
    const auto addExtensionN = GetPropertyN<std::wstring>(GetHandlerProperty2, formatIndex, NArchive::NHandlerPropID::kAddExtension);
    if (extensionN) {
      std::wstring_view restExtension(extensionN.value());
      std::wstring_view restAddExtension;
      if (addExtensionN) {
        restAddExtension = addExtensionN.value();
      }
      while (!restExtension.empty()) {
        const auto extensionFirstSpace = restExtension.find_first_of(L' ');
        const std::wstring_view currentExtension = restExtension.substr(0, extensionFirstSpace);
        restExtension = extensionFirstSpace != std::wstring_view::npos ? restExtension.substr(extensionFirstSpace + 1) : L""sv;

        std::wstring_view currentAddExtension;
        if (!restAddExtension.empty()) {
          const auto addExtensionFirstSpace = restAddExtension.find_first_of(L' ');
          currentAddExtension = restAddExtension.substr(0, addExtensionFirstSpace);
          restAddExtension = addExtensionFirstSpace != std::wstring_view::npos ? restAddExtension.substr(addExtensionFirstSpace + 1) : L""sv;
        }
        format.extensions.emplace_back(currentExtension, currentAddExtension != L"*"sv ? currentAddExtension : L""sv);
      };
    }

    // retrieve flags
    format.flags = GetProperty<std::uint32_t>(GetHandlerProperty2, formatIndex, NArchive::NHandlerPropID::kFlags);

    // retrieve format.signatures
    std::size_t currentMaxSignatureSize = 0;
    if (auto singleSignatureN = GetPropertyN<std::vector<std::byte>>(GetHandlerProperty2, formatIndex, NArchive::NHandlerPropID::kSignature); singleSignatureN && !singleSignatureN.value().empty()) {
      currentMaxSignatureSize = singleSignatureN.value().size();
      format.signatures.emplace_back(std::move(singleSignatureN.value()));
    } else {
      const auto multiSignatureN = GetPropertyN<std::vector<std::byte>>(GetHandlerProperty2, formatIndex, NArchive::NHandlerPropID::kMultiSignature);
      if (multiSignatureN) {
        const auto multiSignature = multiSignatureN.value();
        const std::size_t multiSignatureSize = multiSignature.size();
        for (std::size_t i = 0; i < multiSignatureSize;) {
          const std::size_t currentSignatureSize = static_cast<unsigned char>(multiSignature[i++]);
          if (i + currentSignatureSize > multiSignatureSize) {
            throw std::invalid_argument("invalid multiSignature given");
          }
          format.signatures.emplace_back(multiSignature.cbegin() + i, multiSignature.cbegin() + i + currentSignatureSize);
          i += currentSignatureSize;

          if (currentMaxSignatureSize < currentSignatureSize) {
            currentMaxSignatureSize = currentSignatureSize;
          }
        }
      }
    }

    // retrieve format.signatureOffset
    format.signatureOffset = GetPropertyN<std::uint32_t>(GetHandlerProperty2, formatIndex, NArchive::NHandlerPropID::kSignatureOffset);
    if (format.signatureOffset && currentMaxSignatureSize) {
      currentMaxSignatureSize += format.signatureOffset.value();
    }

    // retrieve format.IsArc
    format.IsArc = nullptr;
    if (FAILED(GetIsArc(formatIndex, &format.IsArc))) {
      format.IsArc = nullptr;
    }

    if (maxSignatureSize < currentMaxSignatureSize) {
      maxSignatureSize = currentMaxSignatureSize;
    }
  }


  std::vector<int> formatPriorities(formats.size());
  for (std::size_t formatIndex = 0; formatIndex < formats.size(); formatIndex++) {
    const auto& format = formats[formatIndex];
    formatPriorities[formatIndex] = gFormatPriorityMap.count(format.name) ? gFormatPriorityMap.at(format.name) : DefaultFormatPriority;
  }

  orderedFormatIndices.resize(formats.size());
  std::iota(orderedFormatIndices.begin(), orderedFormatIndices.end(), 0);
  std::sort(orderedFormatIndices.begin(), orderedFormatIndices.end(), [&formatPriorities](const auto& a, const auto& b) {
    return formatPriorities[a] > formatPriorities[b];
  });
}


const NanaZ::Format& NanaZ::FormatStore::GetFormat(std::size_t index) const {
  return formats.at(index);
}


UInt32 NanaZ::FormatStore::IsArc(std::size_t index, const void* data, std::size_t size) const {
  return formats.at(index).IsArc(reinterpret_cast<const Byte*>(data), size);
}


std::vector<std::size_t> NanaZ::FormatStore::FindFormatByExtension(const std::wstring& extension) const {
  // TODO
  return {};
}


std::vector<std::size_t> NanaZ::FormatStore::FindFormatByStream(winrt::com_ptr<IInStream> inStream) const {
  constexpr std::size_t DefaultBufferSize = 1 << 20;    // see CArc::OpenStream2

  COMError::CheckHRESULT(inStream->Seek(0, STREAM_SEEK_SET, nullptr));

  std::size_t bufferSize = std::max(DefaultBufferSize, maxSignatureSize);
  auto buffer = std::make_unique<std::byte[]>(bufferSize);

  UInt32 readSize = 0;
  COMError::CheckHRESULT(inStream->Read(buffer.get(), static_cast<UInt32>(bufferSize), &readSize));

  COMError::CheckHRESULT(inStream->Seek(0, STREAM_SEEK_SET, nullptr));

  std::vector<std::size_t> formatIndices;
  for (const auto& formatIndex : orderedFormatIndices) {
    const auto& format = formats[formatIndex];

    // check format by IsArc if provided
    if (format.IsArc) {
      const auto isArcResult = format.IsArc(reinterpret_cast<const Byte*>(buffer.get()), readSize);
      if (isArcResult == k_IsArc_Res_NO || (isArcResult == k_IsArc_Res_NEED_MORE && readSize < bufferSize)) {
        continue;
      }
      formatIndices.emplace_back(formatIndex);
      continue;
    }

    // check format by signature
    std::size_t signatureOffset = format.signatureOffset.value_or(0);
    for (const auto& signature : format.signatures) {
      if (signature.size() + signatureOffset > bufferSize) {
        continue;
      }
      if (std::memcmp(signature.data(), buffer.get() + signatureOffset, signature.size()) == 0) {
        formatIndices.emplace_back(formatIndex);
        break;
      }
    }
  }

  return formatIndices;
}



NanaZ::NanaZ(LPCWSTR dllFilepath) :
  nanaZDll(dllFilepath),
  CreateObject(reinterpret_cast<Func_CreateObject>(nanaZDll.GetProc("CreateObject"))),
  GetIsArc(reinterpret_cast<Func_GetIsArc>(nanaZDll.GetProc("GetIsArc"))),
  GetNumberOfFormats(reinterpret_cast<Func_GetNumberOfFormats>(nanaZDll.GetProc("GetNumberOfFormats"))),
  GetHandlerProperty2(reinterpret_cast<Func_GetHandlerProperty2>(nanaZDll.GetProc("GetHandlerProperty2"))),
  formatStore(GetIsArc, GetNumberOfFormats, GetHandlerProperty2)
{}


const NanaZ::Format& NanaZ::GetFormat(std::size_t index) const {
  return formatStore.GetFormat(index);
}


UInt32 NanaZ::IsArc(std::size_t index, const void* data, std::size_t size) const {
  return formatStore.IsArc(index, data, size);
}


std::vector<std::size_t> NanaZ::FindFormatByExtension(const std::wstring& extension) const {
  return formatStore.FindFormatByExtension(extension);
}


std::vector<std::size_t> NanaZ::FindFormatByStream(winrt::com_ptr<IInStream> inStream) const {
  return formatStore.FindFormatByStream(inStream);
}
