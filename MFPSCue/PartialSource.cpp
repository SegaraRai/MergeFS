#include <dokan/dokan.h>

#include <cstddef>
#include <memory>
#include <stdexcept>

#include "PartialSource.hpp"



PartialSource::PartialSource(std::shared_ptr<Source> source, SourceOffset offset, SourceSize size) :
  mSource(source),
  mOffset(offset),
  mSize(size)
{
  const auto sourceSize = source->GetSize();
  if (offset > sourceSize || offset + size > sourceSize) {
    throw std::out_of_range("source out of range");
  }
}


PartialSource::PartialSource(std::shared_ptr<Source> source, SourceOffset offset) :
  PartialSource(source, offset, source->GetSize() - offset)
{}


Source::SourceSize PartialSource::GetSize() {
  return mSize;
}


NTSTATUS PartialSource::Read(SourceOffset offset, std::byte* buffer, std::size_t size, std::size_t* readSize) {
  if (offset >= mSize) {
    if (readSize) {
      *readSize = 0;
    }
    return STATUS_SUCCESS;
  }
  if (offset + size > mSize) {
    size = static_cast<std::size_t>(mSize - offset);
  }
  if (!size) {
    if (readSize) {
      *readSize = 0;
    }
    return STATUS_SUCCESS;
  }
  return mSource->Read(offset + mOffset, buffer, size, readSize);
}
