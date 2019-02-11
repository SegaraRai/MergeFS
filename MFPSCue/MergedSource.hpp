#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "Source.hpp"


class MergedSource : public Source {
  std::vector<std::shared_ptr<Source>> mSources;
  std::vector<SourceOffset> mSourceOffsets;
  std::vector<SourceSize> mSourceSizes;
  SourceSize mTotalSourceSize;

public:
  MergedSource(const std::vector<std::shared_ptr<Source>>& sources);

  SourceSize GetSize() override;
  NTSTATUS Read(SourceOffset offset, std::byte* buffer, std::size_t size, std::size_t* readSize) override;
};
