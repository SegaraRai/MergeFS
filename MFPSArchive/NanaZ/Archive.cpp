#define INITGUID
#define NOMINMAX

#include <dokan/dokan.h>

#include <7z/CPP/Common/MyCom.h>
#include <7z/CPP/7zip/IPassword.h>
#include <7z/CPP/7zip/Archive/IArchive.h>

#include <cassert>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <winrt/base.h>

#include "../SDK/CaseSensitivity.hpp"

#include "Archive.hpp"
#include "ArchiveOpenCallback.hpp"
#include "COMError.hpp"
#include "MemoryArchiveExtractCallback.hpp"
#include "MemoryStream.hpp"
#include "PropVariantUtil.hpp"
#include "PropVariantWrapper.hpp"
#include "SeekFilterStream.hpp"

using namespace std::literals;



namespace {
  using PasswordCallback = MemoryArchiveExtractCallback::PasswordCallback;


  // TODO: make customizable
  constexpr UInt32 SevereErrorFlags = kpv_ErrorFlags_IsNotArc;



  FILETIME GetCreationTime(const DirectoryTree& directoryTree, const FILETIME& fallbackCreationTime) {
    if (directoryTree.creationTimeN) {
      return directoryTree.creationTimeN.value();
    }
    if (directoryTree.lastWriteTimeN) {
      return directoryTree.lastWriteTimeN.value();
    }
    if (directoryTree.lastAccessTimeN) {
      return directoryTree.lastAccessTimeN.value();
    }
    return fallbackCreationTime;
  }


  FILETIME GetLastAccessTime(const DirectoryTree& directoryTree, const FILETIME& fallbackLastAccessTime) {
    if (directoryTree.lastAccessTimeN) {
      return directoryTree.lastAccessTimeN.value();
    }
    if (directoryTree.lastWriteTimeN) {
      return directoryTree.lastWriteTimeN.value();
    }
    if (directoryTree.creationTimeN) {
      return directoryTree.creationTimeN.value();
    }
    return fallbackLastAccessTime;
  }


  FILETIME GetLastWriteTime(const DirectoryTree& directoryTree, const FILETIME& fallbackLastWriteTime) {
    if (directoryTree.lastWriteTimeN) {
      return directoryTree.lastWriteTimeN.value();
    }
    if (directoryTree.creationTimeN) {
      return directoryTree.creationTimeN.value();
    }
    if (directoryTree.lastAccessTimeN) {
      return directoryTree.lastAccessTimeN.value();
    }
    return fallbackLastWriteTime;
  }


  DirectoryTree* Insert(DirectoryTree& directoryTree, std::wstring_view filepath, DirectoryTree&& file, ULONGLONG& fileIndexCount, DirectoryTree::OnExisting onExisting, DirectoryTree::ExtractToMemory extractToMemory) {
    if (filepath.empty()) {
      assert(false);
      directoryTree = std::move(file);
      return &directoryTree;
    }

    const auto& fallbackCreationTime = directoryTree.creationTime;
    const auto& fallbackLastAccessTime = directoryTree.lastAccessTime;
    const auto& fallbackLastWriteTime = directoryTree.lastWriteTime;

    const auto firstDelimiterPos = filepath.find_first_of(Archive::DirectorySeparatorFromLibrary);
    const std::wstring rootDirectoryName(filepath.substr(0, firstDelimiterPos));

    if (firstDelimiterPos == std::wstring_view::npos) {
      // leaf
      auto filename = rootDirectoryName;
      if (onExisting != DirectoryTree::OnExisting::Replace) {
        const auto& baseFilename = rootDirectoryName;
        std::size_t count = 2;
        while (directoryTree.children.count(filename)) {
          auto& child = directoryTree.children.at(filename);
          if (!child.valid && child.type == DirectoryTree::Type::Directory && file.type == DirectoryTree::Type::Directory) {
            break;
          }
          if (onExisting == DirectoryTree::OnExisting::Skip) {
            return nullptr;
          }
          // TODO: make customizable
          filename = baseFilename + L"."s + std::to_wstring(count++);
        }
      }
      return &directoryTree.children.insert_or_assign(filename, std::move(file)).first->second;
    }

    if (!directoryTree.children.count(rootDirectoryName)) {
      // create directory
      directoryTree.children.emplace(rootDirectoryName, DirectoryTree{
        nullptr,
        directoryTree.caseSensitive,
        {0, CaseSensitivity::CiHash(directoryTree.caseSensitive), CaseSensitivity::CiEqualTo(directoryTree.caseSensitive)},
        false,
        true,
        directoryTree.onMemory,
        DirectoryTree::Type::Directory,
        FILE_ATTRIBUTE_DIRECTORY,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        0,
        1,
        fileIndexCount++,
        nullptr,
        nullptr,
        fallbackCreationTime,
        fallbackLastAccessTime,
        fallbackLastWriteTime,
        nullptr,
      });
    }

    auto& child = directoryTree.children.at(rootDirectoryName);
    return Insert(child, filepath.substr(firstDelimiterPos + 1), std::move(file), fileIndexCount, onExisting, extractToMemory);
  }


  winrt::com_ptr<IInArchive> CreateInArchiveFromInStream(NanaZ& nanaZ, winrt::com_ptr<IInStream> inStream, UInt64 maxCheckStartPosition, PasswordCallback passwordCallback) {
    auto formatIndices = nanaZ.FindFormatByStream(inStream);
    for (const auto& formatIndex : formatIndices) {
      const CLSID formatClsid = nanaZ.GetFormat(formatIndex).clsid;

      winrt::com_ptr<IInArchive> inArchive;
      COMError::CheckHRESULT(nanaZ.CreateObject(&formatClsid, &IID_IInArchive, inArchive.put_void()));

      auto archiveOpenCallback = winrt::make_self<ArchiveOpenCallback>(passwordCallback);

      if (FAILED(inArchive->Open(inStream.get(), &maxCheckStartPosition, archiveOpenCallback.get()))) {
        continue;
      }

      // check kpidErrorFlags
      {
        PropVariantWrapper propVariant;
        inArchive->GetArchiveProperty(kpidErrorFlags, &propVariant);
        const auto errorFlags = FromPropVariantN<UInt32>(propVariant).value_or(0);
        if (errorFlags & SevereErrorFlags) {
          continue;
        }
      }

      return inArchive;
    }

    return nullptr;
  }


  void InitializeDirectoryTree(DirectoryTree& directoryTree, const std::wstring& defaultFilepath, const std::wstring_view prefixFilter, const std::wstring& passwordFilepathPrefix, NanaZ& nanaZ, UInt64 maxCheckStartPosition, DirectoryTree::OnExisting onExisting, DirectoryTree::ExtractToMemory extractToMemory, Archive::ArchiveNameCallback archiveNameCallback, Archive::PasswordWithFilepathCallback passwordCallback, UInt64& fileIndexCount) {
    const auto& fallbackCreationTime = directoryTree.creationTime;
    const auto& fallbackLastAccessTime = directoryTree.lastAccessTime;
    const auto& fallbackLastWriteTime = directoryTree.lastWriteTime;

    winrt::com_ptr<IInArchiveGetStream> inArchiveGetStream;
    if (FAILED(directoryTree.inArchive->QueryInterface(IID_IInArchiveGetStream, inArchiveGetStream.put_void()))) {
      inArchiveGetStream = nullptr;
    }

    UInt32 numItems = 0;
    COMError::CheckHRESULT(directoryTree.inArchive->GetNumberOfItems(&numItems));

    std::vector<UInt32> extractToMemoryIndices;
    std::vector<std::tuple<UInt32, DirectoryTree&, std::wstring>> extractToMemoryObjects;

    std::vector<std::tuple<UInt32, DirectoryTree&, std::wstring>> openAsArchiveObjects;

    for (UInt32 index = 0; index < numItems; index++) {
      try {
        DirectoryTree contentDirectoryTree{
          directoryTree.streamMutex,
          directoryTree.caseSensitive,
          {0, CaseSensitivity::CiHash(directoryTree.caseSensitive), CaseSensitivity::CiEqualTo(directoryTree.caseSensitive)},
          true,
        };

        {
          PropVariantWrapper propVariant;
          directoryTree.inArchive->GetProperty(index, kpidCTime, &propVariant);
          contentDirectoryTree.creationTimeN = FromPropVariantN<FILETIME>(propVariant);
        }
        {
          PropVariantWrapper propVariant;
          directoryTree.inArchive->GetProperty(index, kpidATime, &propVariant);
          contentDirectoryTree.lastAccessTimeN = FromPropVariantN<FILETIME>(propVariant);
        }
        {
          PropVariantWrapper propVariant;
          directoryTree.inArchive->GetProperty(index, kpidMTime, &propVariant);
          contentDirectoryTree.lastWriteTimeN = FromPropVariantN<FILETIME>(propVariant);
        }

        bool directory = false;
        {
          // this may be null on tar.xz file
          PropVariantWrapper propVariant;
          directoryTree.inArchive->GetProperty(index, kpidIsDir, &propVariant);
          directory = FromPropVariantN<bool>(propVariant).value_or(false);
        }

        {
          // kpidAttrib is Windows' file attribute flags
          PropVariantWrapper propVariant;
          directoryTree.inArchive->GetProperty(index, kpidAttrib, &propVariant);
          contentDirectoryTree.fileAttributes = FromPropVariantN<UInt32>(propVariant).value_or(directory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL);
        }

        contentDirectoryTree.type = directory ? DirectoryTree::Type::Directory : DirectoryTree::Type::File;

        contentDirectoryTree.contentAvailable = true;

        if (contentDirectoryTree.type == DirectoryTree::Type::File) {
          PropVariantWrapper propVariant;
          directoryTree.inArchive->GetProperty(index, kpidSize, &propVariant);
          const auto fileSizeN = FromPropVariantN<UInt64>(propVariant);
          contentDirectoryTree.fileSize = fileSizeN.value_or(0);
          contentDirectoryTree.contentAvailable = !!fileSizeN;
        } else {
          contentDirectoryTree.fileSize = 0;
        }

        // TODO
        contentDirectoryTree.numberOfLinks = 1;

        contentDirectoryTree.fileIndex = fileIndexCount++;

        std::optional<std::wstring> contentFilepathN;
        {
          PropVariantWrapper propVariant;
          directoryTree.inArchive->GetProperty(index, kpidPath, &propVariant);
          contentFilepathN = FromPropVariantN<std::wstring>(propVariant);
        }

        const std::wstring contentFilepath = contentFilepathN.value_or(defaultFilepath);

        if (!prefixFilter.empty()) {
          // TODO: prefixFilterが\で終わっていれば\前のもの（prefixFilterが表すディレクトリ自身）も含めるようにするべき？
          if (contentFilepath.size() < prefixFilter.size()) {
            continue;
          }
          if (std::wstring_view(contentFilepath).substr(0, prefixFilter.size()) != prefixFilter) {
            continue;
          }
        }

        // read data
        bool extractCurrentFileToMemory = extractToMemory == DirectoryTree::ExtractToMemory::Always;

        if (!extractCurrentFileToMemory && contentDirectoryTree.type == DirectoryTree::Type::File) {
          winrt::com_ptr<IInStream> contentInStream;

          do {
            if (!inArchiveGetStream) {
              break;
            }

            winrt::com_ptr<ISequentialInStream> sequentialInStream;
            inArchiveGetStream->GetStream(index, sequentialInStream.put());
            if (!sequentialInStream) {
              break;
            }

            sequentialInStream->QueryInterface(IID_IInStream, contentInStream.put_void());
          } while (false);

          if (contentInStream) {
            contentDirectoryTree.inStream = winrt::make_self<InSeekFilterStream>(contentInStream, directoryTree.inStream);
          } else {
            if (extractToMemory == DirectoryTree::ExtractToMemory::Never) {
              throw COMError(E_FAIL);
            }
            if constexpr (std::numeric_limits<decltype(directoryTree.fileSize)>::max() < std::numeric_limits<std::size_t>::max()) {
              if (directoryTree.fileSize >= std::numeric_limits<std::size_t>::max()) {
                throw COMError(E_OUTOFMEMORY);
              }
            }
            extractCurrentFileToMemory = true;
          }
        }

        contentDirectoryTree.creationTime = GetCreationTime(contentDirectoryTree, fallbackCreationTime);
        contentDirectoryTree.lastAccessTime = GetLastAccessTime(contentDirectoryTree, fallbackLastAccessTime);
        contentDirectoryTree.lastWriteTime = GetLastWriteTime(contentDirectoryTree, fallbackLastWriteTime);

        // add to tree
        auto ptrInsertedDirectoryTree = Insert(directoryTree, contentFilepath, std::move(contentDirectoryTree), fileIndexCount, onExisting, extractToMemory);
        if (!ptrInsertedDirectoryTree) {
          throw COMError(E_FAIL);
        }

        auto& insertedDirectoryTree = *ptrInsertedDirectoryTree;

        if (extractCurrentFileToMemory) {
          if (!insertedDirectoryTree.contentAvailable) {
            assert(!insertedDirectoryTree.inStream);
            insertedDirectoryTree.inStream = winrt::make_self<InMemoryStream>(nullptr, 0);
          } else {
            insertedDirectoryTree.onMemory = true;
            extractToMemoryIndices.emplace_back(index);
            extractToMemoryObjects.emplace_back(index, insertedDirectoryTree, contentFilepath);
          }
        }

        if (archiveNameCallback) {
          if (!directory && insertedDirectoryTree.contentAvailable) {
            openAsArchiveObjects.emplace_back(index, insertedDirectoryTree, contentFilepath);
          }
        }
      } catch (...) {}
    }

    // extract into memory and create IInStream from memory data
    if (!extractToMemoryIndices.empty()) {
      winrt::com_ptr<MemoryArchiveExtractCallback> memoryArchiveExtractCallback;
      PasswordCallback contentPasswordCallback;
      if (passwordCallback) {
        // TODO: fix passwordFilepath
        const std::wstring passwordFilepath = passwordFilepathPrefix;
        contentPasswordCallback = [passwordCallback, passwordFilepath]() -> std::optional<std::wstring> {
          return passwordCallback(passwordFilepath);
        };
      }

      std::vector<UInt64> filesizes(extractToMemoryIndices.size());
      std::vector<std::pair<UInt32, UInt64>> indexAndFilesizes(extractToMemoryIndices.size());
      for (std::size_t i = 0; i < extractToMemoryIndices.size(); i++) {
        const auto& [index, contentDirectoryTree, contentFilepath] = extractToMemoryObjects.at(i);
        filesizes.at(i) = contentDirectoryTree.fileSize;
        indexAndFilesizes.at(i) = std::make_pair(static_cast<UInt32>(index), static_cast<UInt64>(contentDirectoryTree.fileSize));
        assert(contentDirectoryTree.contentAvailable);
      }

      const auto totalExtractionMemorySize = MemoryArchiveExtractCallback::CalcMemorySize(filesizes);
      directoryTree.extractionMemory = std::make_unique<std::byte[]>(totalExtractionMemorySize);

      memoryArchiveExtractCallback = std::move(winrt::make_self<MemoryArchiveExtractCallback>(directoryTree.extractionMemory.get(), totalExtractionMemorySize, indexAndFilesizes, contentPasswordCallback));

      COMError::CheckHRESULT(directoryTree.inArchive->Extract(extractToMemoryIndices.data(), static_cast<UInt32>(extractToMemoryIndices.size()), FALSE, memoryArchiveExtractCallback.get()));

      for (const auto& [index, contentDirectoryTree, contentFilepath] : extractToMemoryObjects) {
        if (!memoryArchiveExtractCallback->Succeeded(index)) {
          contentDirectoryTree.contentAvailable = false;
          contentDirectoryTree.fileSize = 0;
          contentDirectoryTree.inStream = winrt::make_self<InMemoryStream>(nullptr, 0);
          contentDirectoryTree.streamMutex = std::make_shared<std::mutex>();
          continue;
        }

        const std::size_t fileSize = memoryArchiveExtractCallback->GetSize(index);
        assert(fileSize == contentDirectoryTree.fileSize);

        contentDirectoryTree.fileSize = fileSize;
        contentDirectoryTree.inStream = winrt::make_self<InMemoryStream>(memoryArchiveExtractCallback->GetData(index), fileSize);
        contentDirectoryTree.streamMutex = std::make_shared<std::mutex>();
      }
    }

    // open as archive
    for (const auto& [index, contentDirectoryTree, contentFilepath] : openAsArchiveObjects) {
      assert(contentDirectoryTree.valid);
      assert(contentDirectoryTree.type == DirectoryTree::Type::File);
      assert(contentDirectoryTree.inStream);

      if (!contentDirectoryTree.contentAvailable) {
        continue;
      }

      std::optional<std::pair<std::wstring, bool>> asArchiveFilepathOption;
      std::size_t count = 0;
      do {
        asArchiveFilepathOption = archiveNameCallback(contentFilepath, count++);
      } while (asArchiveFilepathOption && !asArchiveFilepathOption.value().second && directoryTree.Exists(asArchiveFilepathOption.value().first));
      if (!asArchiveFilepathOption) {
        continue;
      }
      const std::wstring& asArchiveFilepath = asArchiveFilepathOption.value().first;

      // TODO: fix passwordFilepath
      const std::wstring passwordFilepath = contentFilepath;
      PasswordCallback contentPasswordCallback = [passwordCallback, passwordFilepath]() -> std::optional<std::wstring> {
        return passwordCallback(passwordFilepath);
      };


      auto originalContentInStream = contentDirectoryTree.inStream;
      auto cloneContentInStream = winrt::make_self<InSeekFilterStream>(originalContentInStream);

      auto contentInArchive = CreateInArchiveFromInStream(nanaZ, cloneContentInStream, maxCheckStartPosition, contentPasswordCallback);
      if (!contentInArchive) {
        continue;
      }

      DirectoryTree cloneContentDirectoryTree{
        contentDirectoryTree.streamMutex,
        contentDirectoryTree.caseSensitive,
        {0, CaseSensitivity::CiHash(contentDirectoryTree.caseSensitive), CaseSensitivity::CiEqualTo(contentDirectoryTree.caseSensitive)},
        true,
        contentDirectoryTree.contentAvailable,
        contentDirectoryTree.onMemory,
        DirectoryTree::Type::Archive,
        contentDirectoryTree.fileAttributes,
        contentDirectoryTree.creationTimeN,
        contentDirectoryTree.lastAccessTimeN,
        contentDirectoryTree.lastWriteTimeN,
        contentDirectoryTree.fileSize,
        1,    // TODO: numberOfLinks
        fileIndexCount++,
        cloneContentInStream,
        contentInArchive,
        contentDirectoryTree.creationTime,
        contentDirectoryTree.lastAccessTime,
        contentDirectoryTree.lastWriteTime,
        nullptr,
      };

      // modify source inStream in order to completely separate seek positions
      contentDirectoryTree.inStream = winrt::make_self<InSeekFilterStream>(originalContentInStream);

      // filename collision will not occur
      auto ptrInsertedCloneContentDirectoryTree = Insert(directoryTree, asArchiveFilepath, std::move(cloneContentDirectoryTree), fileIndexCount, DirectoryTree::OnExisting::Replace, extractToMemory);

      if (ptrInsertedCloneContentDirectoryTree) {
        InitializeDirectoryTree(*ptrInsertedCloneContentDirectoryTree, defaultFilepath, L""sv, passwordFilepathPrefix + L"\\"s + contentFilepath, nanaZ, maxCheckStartPosition, onExisting, extractToMemory, archiveNameCallback, passwordCallback, fileIndexCount);
      }
    }
  }
}



DWORD DirectoryTree::FilterArchiveFileAttributes(const DirectoryTree& directoryTree) {
  const DWORD originalFileAttributes = directoryTree.fileAttributes;
  if (directoryTree.type != Type::Archive) {
    return originalFileAttributes;
  }
  return originalFileAttributes == FILE_ATTRIBUTE_NORMAL
    ? FILE_ATTRIBUTE_DIRECTORY
    : originalFileAttributes | FILE_ATTRIBUTE_DIRECTORY;
}


const DirectoryTree* DirectoryTree::Get(std::wstring_view filepath) const {
  if (filepath.empty()) {
    return this;
  }
  switch (type) {
    case Type::File:
      return nullptr;

    case Type::Directory:
    case Type::Archive:
    {
      const auto firstDelimiterPos = filepath.find_first_of(DirectorySeparator);
      auto itrChild = children.find(std::wstring(filepath.substr(0, firstDelimiterPos)));
      if (itrChild == children.end()) {
        return nullptr;
      }
      if (firstDelimiterPos == std::wstring_view::npos) {
        return &itrChild->second;
      }
      return itrChild->second.Get(filepath.substr(firstDelimiterPos + 1));
    }
  }
  throw std::logic_error("invalid type");
}


bool DirectoryTree::Exists(std::wstring_view filepath) const {
  return Get(filepath) != nullptr;
}



Archive::Archive(NanaZ& nanaZ, winrt::com_ptr<IInStream> inStream, const BY_HANDLE_FILE_INFORMATION& byHandleFileInformation, const std::wstring& defaultFilepath, std::wstring_view prefixFilter, bool caseSensitive, UInt64 maxCheckStartPosition, OnExisting onExisting, ExtractToMemory extractToMemory, ArchiveNameCallback archiveNameCallback, PasswordWithFilepathCallback passwordCallback) :
  DirectoryTree{
    std::make_shared<std::mutex>(),
    caseSensitive,
    {0, CaseSensitivity::CiHash(caseSensitive), CaseSensitivity::CiEqualTo(caseSensitive)},
    true,
    true,
    false,
    Type::Archive,
    byHandleFileInformation.dwFileAttributes,
    byHandleFileInformation.ftCreationTime,
    byHandleFileInformation.ftLastAccessTime,
    byHandleFileInformation.ftLastWriteTime,
    (static_cast<ULONGLONG>(byHandleFileInformation.nFileSizeHigh) << 32) | byHandleFileInformation.nFileSizeLow,
    1,
    0,
    inStream,
    nullptr,
    byHandleFileInformation.ftCreationTime,
    byHandleFileInformation.ftLastAccessTime,
    byHandleFileInformation.ftLastWriteTime,
    nullptr,
  },
  nanaZ(nanaZ)
{
  PasswordCallback rootArchivePasswordCallback;
  if (passwordCallback) {
    rootArchivePasswordCallback = [passwordCallback]() -> std::optional<std::wstring> {
      return passwordCallback(L""s);
    };
  }
  this->inArchive = CreateInArchiveFromInStream(nanaZ, inStream, maxCheckStartPosition, rootArchivePasswordCallback);
  if (!this->inArchive) {
    throw std::runtime_error("cannot open stream as archive");
  }
  UInt64 fileIndexCount = this->fileIndex + 1;
  InitializeDirectoryTree(*this, defaultFilepath, prefixFilter, L""s, nanaZ, maxCheckStartPosition, onExisting, extractToMemory, archiveNameCallback, passwordCallback, fileIndexCount);
}


const DirectoryTree* Archive::Get(std::wstring_view filepath) const {
  return DirectoryTree::Get(filepath);
}


bool Archive::Exists(std::wstring_view filepath) const {
  return DirectoryTree::Exists(filepath);
}
