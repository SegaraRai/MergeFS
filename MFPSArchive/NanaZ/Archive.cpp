#define INITGUID

#include <dokan/dokan.h>

#include <7z/CPP/Common/MyCom.h>
#include <7z/CPP/7zip/IPassword.h>
#include <7z/CPP/7zip/Archive/IArchive.h>

#include "../SDK/CaseSensitivity.hpp"

#include "Archive.hpp"
#include "ArchiveOpenCallback.hpp"
#include "COMError.hpp"
#include "MemoryArchiveExtractCallback.hpp"
#include "MemoryStream.hpp"
#include "PropVariantUtil.hpp"
#include "PropVariantWrapper.hpp"
#include "SeekFilterStream.hpp"

#include <cassert>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <winrt/base.h>

using namespace std::literals;



namespace {
  using PasswordCallback = MemoryArchiveExtractCallback::PasswordCallback;



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


  FILETIME GetLastAccessTimeN(const DirectoryTree& directoryTree, const FILETIME& fallbackLastAccessTime) {
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


  FILETIME GetLastWriteTimeN(const DirectoryTree& directoryTree, const FILETIME& fallbackLastWriteTime) {
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


  DirectoryTree* Insert(DirectoryTree& directoryTree, std::wstring_view filepath, DirectoryTree&& file, ULONGLONG& fileIndexCount, DirectoryTree::OnExistingMode onExistingMode, const FILETIME& fallbackCreationTime, const FILETIME& fallbackLastAccessTime, const FILETIME& fallbackLastWriteTime) {
    if (filepath.empty()) {
      assert(false);
      directoryTree = std::move(file);
      return &directoryTree;
    }

    const auto firstDelimiterPos = filepath.find_first_of(Archive::DirectorySeparatorFromLibrary);
    const std::wstring rootDirectoryName(filepath.substr(0, firstDelimiterPos));

    if (firstDelimiterPos == std::wstring_view::npos) {
      // leaf
      auto filename = rootDirectoryName;      
      if (onExistingMode != DirectoryTree::OnExistingMode::Replace) {
        const auto& baseFilename = rootDirectoryName;
        std::size_t count = 2;
        while (directoryTree.children.count(filename)) {
          auto& child = directoryTree.children.at(filename);
          if (!child.valid && child.type == DirectoryTree::Type::Directory && file.type == DirectoryTree::Type::Directory) {
            break;
          }
          if (onExistingMode == DirectoryTree::OnExistingMode::Skip) {
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
        decltype(DirectoryTree::children)(0, CaseSensitivity::CiHash(directoryTree.caseSensitive), CaseSensitivity::CiEqualTo(directoryTree.caseSensitive)),
        directoryTree.useOnMemoryExtraction,
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
    return Insert(child, filepath.substr(firstDelimiterPos + 1), std::move(file), fileIndexCount, onExistingMode, fallbackCreationTime, fallbackLastAccessTime, fallbackLastWriteTime);
  }


  winrt::com_ptr<IInArchive> CreateInArchiveFromInStream(NanaZ& nanaZ, winrt::com_ptr<IInStream> inStream, UInt64 maxCheckStartPosition, PasswordCallback passwordCallback) {
    auto formatIndices = nanaZ.FindFormatByStream(inStream);
    for (const auto& formatIndex : formatIndices) {
      winrt::com_ptr<IInArchive> inArchive;

      const CLSID formatClsid = nanaZ.GetFormat(formatIndex).clsid;
      COMError::CheckHRESULT(nanaZ.CreateObject(&formatClsid, &IID_IInArchive, inArchive.put_void()));

      winrt::com_ptr<IArchiveOpenCallback> archiveOpenCallback;
      archiveOpenCallback.attach(new ArchiveOpenCallback(passwordCallback));

      if (FAILED(inArchive->Open(inStream.get(), &maxCheckStartPosition, archiveOpenCallback.get()))) {
        continue;
      }

      // check kpidErrorFlags
      {
        PropVariantWrapper propVariant;
        inArchive->GetArchiveProperty(kpidErrorFlags, &propVariant);
        const auto errorFlags = FromPropVariantN<UInt32>(propVariant).value_or(0);
        if (errorFlags) {
          continue;
        }
      }
      
      return inArchive;
    }

    return nullptr;
  }


  void InitializeDirectoryTree(DirectoryTree& directoryTree, const std::wstring& defaultFilepath, const std::wstring& passwordFilepathPrefix, NanaZ& nanaZ, UInt64 maxCheckStartPosition, DirectoryTree::OnExistingMode onExistingMode, Archive::ArchiveNameCallback archiveNameCallback, Archive::PasswordWithFilepathCallback passwordCallback, UInt64& fileIndexCount, const FILETIME& fallbackCreationTime, const FILETIME& fallbackLastAccessTime, const FILETIME& fallbackLastWriteTime) {
    winrt::com_ptr<IInArchiveGetStream> inArchiveGetStream;
    if (FAILED(directoryTree.inArchive->QueryInterface(IID_IInArchiveGetStream, inArchiveGetStream.put_void()))) {
      inArchiveGetStream = nullptr;
    }

    UInt32 numItems = 0;
    COMError::CheckHRESULT(directoryTree.inArchive->GetNumberOfItems(&numItems));

    std::vector<UInt32> extractToMemoryIndices;
    std::vector<std::tuple<UInt32, DirectoryTree&, std::wstring>> extractToMemoryObjects;
    UInt64 totalExtractionMemorySize = 0;

    std::vector<std::tuple<UInt32, DirectoryTree&, std::wstring>> openAsArchiveObjects;

    for (UInt32 index = 0; index < numItems; index++) {
      try {
        DirectoryTree contentDirectoryTree{
          directoryTree.streamMutex,
          directoryTree.caseSensitive,
          decltype(DirectoryTree::children)(0, CaseSensitivity::CiHash(directoryTree.caseSensitive), CaseSensitivity::CiEqualTo(directoryTree.caseSensitive)),
          directoryTree.useOnMemoryExtraction,
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

        // read data
        bool extractToMemory = directoryTree.useOnMemoryExtraction == DirectoryTree::UseOnMemoryExtractionMode::Always;

        if (!extractToMemory && contentDirectoryTree.type == DirectoryTree::Type::File) {
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
            winrt::com_ptr<InSeekFilterStream> contentInSeekFilterStream;
            contentInSeekFilterStream.attach(new InSeekFilterStream(contentInStream, directoryTree.inStream));

            contentDirectoryTree.inStream = contentInSeekFilterStream;
          } else {
            if (directoryTree.useOnMemoryExtraction == DirectoryTree::UseOnMemoryExtractionMode::Never) {
              throw COMError(E_FAIL);
            }
            extractToMemory = true;
          }
        }

        contentDirectoryTree.creationTime = GetCreationTime(contentDirectoryTree, fallbackCreationTime);
        contentDirectoryTree.lastAccessTime = GetCreationTime(contentDirectoryTree, fallbackLastAccessTime);
        contentDirectoryTree.lastWriteTime = GetCreationTime(contentDirectoryTree, fallbackLastWriteTime);

        // add to tree
        auto ptrInsertedDirectoryTree = Insert(directoryTree, contentFilepath, std::move(contentDirectoryTree), fileIndexCount, onExistingMode, fallbackCreationTime, fallbackLastAccessTime, fallbackLastWriteTime);
        if (!ptrInsertedDirectoryTree) {
          throw COMError(E_FAIL);
        }

        auto& insertedDirectoryTree = *ptrInsertedDirectoryTree;

        if (extractToMemory) {
          if (!insertedDirectoryTree.contentAvailable) {
            assert(!insertedDirectoryTree.inStream);
            insertedDirectoryTree.inStream.attach(new InMemoryStream(nullptr, 0));
          } else {
            insertedDirectoryTree.onMemory = true;
            extractToMemoryIndices.emplace_back(index);
            extractToMemoryObjects.emplace_back(index, insertedDirectoryTree, contentFilepath);
            totalExtractionMemorySize += contentDirectoryTree.fileSize;
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

      directoryTree.extractionMemory = std::make_unique<std::byte[]>(totalExtractionMemorySize);

      std::vector<std::pair<UInt32, UInt64>> indexAndFilesizes(extractToMemoryIndices.size());
      for (std::size_t i = 0; i < extractToMemoryIndices.size(); i++) {
        const auto& [index, contentDirectoryTree, contentFilepath] = extractToMemoryObjects.at(i);
        indexAndFilesizes.at(i) = std::make_pair(static_cast<UInt32>(index), static_cast<UInt64>(contentDirectoryTree.fileSize));
        assert(contentDirectoryTree.contentAvailable);
      }

      memoryArchiveExtractCallback.attach(new MemoryArchiveExtractCallback(directoryTree.extractionMemory.get(), totalExtractionMemorySize, indexAndFilesizes, contentPasswordCallback));

      COMError::CheckHRESULT(directoryTree.inArchive->Extract(extractToMemoryIndices.data(), static_cast<UInt32>(extractToMemoryIndices.size()), FALSE, memoryArchiveExtractCallback.get()));

      for (const auto& [index, contentDirectoryTree, contentFilepath] : extractToMemoryObjects) {
        const std::size_t fileSize = contentDirectoryTree.fileSize;

        winrt::com_ptr<InMemoryStream> contentInStream;
        contentInStream.attach(new InMemoryStream(memoryArchiveExtractCallback->GetData(index), fileSize));

        contentDirectoryTree.inStream = contentInStream;

        contentDirectoryTree.streamMutex = std::make_shared<std::mutex>();
      }
    }

    // open as archive
    for (const auto& [index, contentDirectoryTree, contentFilepath] : openAsArchiveObjects) {
      assert(contentDirectoryTree.contentAvailable);

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

      winrt::com_ptr<InSeekFilterStream> cloneContentInStream;
      cloneContentInStream.attach(new InSeekFilterStream(originalContentInStream));

      auto contentInArchive = CreateInArchiveFromInStream(nanaZ, cloneContentInStream, maxCheckStartPosition, contentPasswordCallback);
      if (!contentInArchive) {
        continue;
      }

      DirectoryTree cloneContentDirectoryTree{
        contentDirectoryTree.streamMutex,
        contentDirectoryTree.caseSensitive,
        decltype(DirectoryTree::children)(0, CaseSensitivity::CiHash(contentDirectoryTree.caseSensitive), CaseSensitivity::CiEqualTo(contentDirectoryTree.caseSensitive)),
        contentDirectoryTree.useOnMemoryExtraction,
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
      contentDirectoryTree.inStream.attach(new InSeekFilterStream(originalContentInStream));

      // filename collision will not occur
      auto ptrInsertedCloneContentDirectoryTree = Insert(directoryTree, asArchiveFilepath, std::move(cloneContentDirectoryTree), fileIndexCount, DirectoryTree::OnExistingMode::Replace, fallbackCreationTime, fallbackLastAccessTime, fallbackLastWriteTime);

      if (ptrInsertedCloneContentDirectoryTree) {
        InitializeDirectoryTree(*ptrInsertedCloneContentDirectoryTree, defaultFilepath, passwordFilepathPrefix + L"\\"s + contentFilepath, nanaZ, maxCheckStartPosition, onExistingMode, archiveNameCallback, passwordCallback, fileIndexCount, fallbackCreationTime, fallbackLastAccessTime, fallbackLastWriteTime);
      }
    }
  }
}



DWORD DirectoryTree::FilterArchiveFileAttributes(const DirectoryTree& directoryTree) {
  DWORD originalFileAttributes = directoryTree.fileAttributes;
  if (directoryTree.type != Type::Archive) {
    return originalFileAttributes;
  }
  return originalFileAttributes == FILE_ATTRIBUTE_NORMAL ? FILE_ATTRIBUTE_DIRECTORY : originalFileAttributes | FILE_ATTRIBUTE_DIRECTORY;
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
  throw std::logic_error(u8"invalid type");
}


bool DirectoryTree::Exists(std::wstring_view filepath) const {
  return Get(filepath) != nullptr;
}



Archive::Archive(NanaZ& nanaZ, winrt::com_ptr<IInStream> inStream, const BY_HANDLE_FILE_INFORMATION& byHandleFileInformation, const std::wstring& defaultFilepath, bool caseSensitive, UInt64 maxCheckStartPosition, OnExistingMode onExistingMode, UseOnMemoryExtractionMode useOnMemoryExtraction, ArchiveNameCallback archiveNameCallback, PasswordWithFilepathCallback passwordCallback) :
  DirectoryTree{
    std::make_shared<std::mutex>(),
    caseSensitive,
    decltype(DirectoryTree::children)(0, CaseSensitivity::CiHash(caseSensitive), CaseSensitivity::CiEqualTo(caseSensitive)),
    useOnMemoryExtraction,
    true,
    true,
    false,
    Type::Archive,
    FILE_ATTRIBUTE_NORMAL,
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
  UInt64 fileIndexCount = this->fileIndex + 1;
  InitializeDirectoryTree(*this, defaultFilepath, L""s, nanaZ, maxCheckStartPosition, onExistingMode, archiveNameCallback, passwordCallback, fileIndexCount, byHandleFileInformation.ftCreationTime, byHandleFileInformation.ftLastAccessTime, byHandleFileInformation.ftLastWriteTime);
}


const DirectoryTree* Archive::Get(std::wstring_view filepath) const {
  return DirectoryTree::Get(filepath);
}


bool Archive::Exists(std::wstring_view filepath) const {
  return DirectoryTree::Exists(filepath);
}
