#pragma once

#include <cstdint>

#include <Rpc.h>


class GUIDHash {
public:
  std::uint64_t operator()(const GUID& guid) const noexcept;
};
