#define NOMINMAX

#include <dokan/dokan.h>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <vector>

#include "MergedSource.hpp"



MergedSource::MergedSource(const std::vector<std::shared_ptr<Source>>& sources) :
  mTotalSourceSize(0)
{
  for (const auto& source : sources) {
    const auto currentSourceSize = source->GetSize();
    mSources.emplace_back(source);
    mSourceSizes.emplace_back(currentSourceSize);
    mSourceOffsets.emplace_back(mTotalSourceSize);
    mTotalSourceSize += currentSourceSize;
  }
  mSourceOffsets.emplace_back(mTotalSourceSize);
}


Source::SourceSize MergedSource::GetSize() {
  return mTotalSourceSize;
}


NTSTATUS MergedSource::Read(SourceOffset offset, std::byte* buffer, std::size_t size, std::size_t* readSize) {
  if (offset > mTotalSourceSize) {
    if (readSize) {
      *readSize = 0;
    }
    return STATUS_SUCCESS;
  }
  if (offset + size > mTotalSourceSize) {
    size = static_cast<std::size_t>(mTotalSourceSize - offset);
  }
  if (!size) {
    if (readSize) {
      *readSize = 0;
    }
    return STATUS_SUCCESS;
  }
  
  // linear search but does not matter in most cases
  std::size_t firstSourceIndex = 0;
  while (mSourceOffsets[firstSourceIndex + 1] <= offset) {
    firstSourceIndex++;
  }

  std::size_t tempReadSize = 0;
  std::size_t sourceIndex = firstSourceIndex;
  while (tempReadSize != size) {
    auto& currentSource = *mSources[sourceIndex];

    SourceOffset currentOffset = offset + tempReadSize - mSourceOffsets[sourceIndex];
    const std::size_t sizeToRead = static_cast<std::size_t>(std::min<SourceSize>(size - tempReadSize, mSourceSizes[sourceIndex] - currentOffset));
    std::size_t currentReadSize = 0;
    if (const auto status = currentSource.Read(currentOffset, buffer + tempReadSize, sizeToRead, &currentReadSize);  status != STATUS_SUCCESS) {
      return status;
    }
    tempReadSize += currentReadSize;
    if (currentReadSize != sizeToRead) {
      break;
    }
    sourceIndex++;
  }

  if (readSize) {
    *readSize = tempReadSize;
  }

  return STATUS_SUCCESS;
}
