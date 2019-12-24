#pragma once

#include <cstdint>

#include <guiddef.h>


class GUIDHash {
public:
  static std::uint64_t CalcGUIDHash(const GUID& guid) noexcept;

  std::uint64_t operator()(const GUID& guid) const noexcept;
};
