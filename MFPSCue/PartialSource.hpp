#pragma once

#include <memory>

#include "Source.hpp"


class PartialSource : public Source {
  std::shared_ptr<Source> mSource;
  SourceOffset mOffset;
  SourceSize mSize;

public:
  PartialSource(std::shared_ptr<Source> source, SourceOffset offset, SourceSize size);
  PartialSource(std::shared_ptr<Source> source, SourceOffset offset);

  SourceSize GetSize() override;
  NTSTATUS Read(SourceOffset offset, std::byte* buffer, std::size_t size, std::size_t* readSize) override;
};
