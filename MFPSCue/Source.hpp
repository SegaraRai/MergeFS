#pragma once

#include <cstddef>

#include <Windows.h>


class Source {
public:
  using SourceSize = unsigned long long;
  using SourceOffset = SourceSize;

  virtual ~Source() = default;

  virtual SourceSize GetSize() = 0;
  virtual NTSTATUS Read(SourceOffset offset, std::byte* buffer, std::size_t size, std::size_t* readSize) = 0;
};
