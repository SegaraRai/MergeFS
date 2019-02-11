#include <dokan/dokan.h>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <utility>

#include "OnMemorySourceWrapper.hpp"



OnMemorySourceWrapper::OnMemorySourceWrapper(std::shared_ptr<Source> source) :
  mSize(source->GetSize())
{
  auto upData = std::make_unique<std::byte[]>(mSize);
  std::size_t readSize = 0;
  if (source->Read(0, upData.get(), mSize, &readSize) != STATUS_SUCCESS || readSize != mSize) {
    throw std::runtime_error("failed to read from source");
  }
  mMemorySource = std::make_shared<MemorySource>(std::move(upData), mSize);
}


Source::SourceSize OnMemorySourceWrapper::GetSize() {
  return mSize;
}


NTSTATUS OnMemorySourceWrapper::Read(SourceOffset offset, std::byte* buffer, std::size_t size, std::size_t* readSize) {
  return mMemorySource->Read(offset, buffer, size, readSize);
}
