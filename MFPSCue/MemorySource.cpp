#include <dokan/dokan.h>

#include <cstddef>
#include <cstring>
#include <memory>
#include <utility>

#include "MemorySource.hpp"



MemorySource::MemorySource(std::unique_ptr<std::byte[]>&& data, std::size_t size) :
  mData(std::move(data)),
  mSize(size)
{}


MemorySource::MemorySource(const std::byte* data, std::size_t size) :
  mData(std::make_unique<std::byte[]>(size)),
  mSize(size)
{
  std::memcpy(mData.get(), data, size);
}


Source::SourceSize MemorySource::GetSize() {
  return mSize;
}


NTSTATUS MemorySource::Read(SourceOffset offset, std::byte* buffer, std::size_t size, std::size_t* readSize) {
  if (offset >= mSize) {
    if (readSize) {
      *readSize = 0;
    }
    return STATUS_SUCCESS;
  }
  if (offset + size > mSize) {
    size = mSize - offset;
  }
  if (buffer && size) {
    std::memcpy(buffer, mData.get() + offset, size);
  }
  if (readSize) {
    *readSize = size;
  }
  return STATUS_SUCCESS;
}
