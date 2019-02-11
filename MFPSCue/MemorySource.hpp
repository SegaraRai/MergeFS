#pragma once

#include <cstddef>
#include <memory>

#include "Source.hpp"


class MemorySource : public Source {
  std::unique_ptr<std::byte[]> mData;
  std::size_t mSize;

public:
  MemorySource(std::unique_ptr<std::byte[]>&& data, std::size_t size);
  MemorySource(const std::byte* data, std::size_t size);

  SourceSize GetSize() override;
  NTSTATUS Read(SourceOffset offset, std::byte* buffer, std::size_t size, std::size_t* readSize) override;
};
