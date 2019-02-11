#pragma once

#include <cstddef>
#include <memory>

#include "MemorySource.hpp"
#include "Source.hpp"


class OnMemorySourceWrapper : public Source {
  SourceSize mSize;
  std::shared_ptr<MemorySource> mMemorySource;

public:
  OnMemorySourceWrapper(std::shared_ptr<Source> source);

  SourceSize GetSize() override;
  NTSTATUS Read(SourceOffset offset, std::byte* buffer, std::size_t size, std::size_t* readSize) override;
};
