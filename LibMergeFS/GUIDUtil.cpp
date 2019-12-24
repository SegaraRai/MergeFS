#include "GUIDUtil.hpp"

#include <cstdint>
#include <climits>

#include <guiddef.h>



static_assert(CHAR_BIT == 8);
static_assert(sizeof(std::uint64_t) == 8);
static_assert(sizeof(GUID) == 16);


std::uint64_t GUIDHash::CalcGUIDHash(const GUID& guid) noexcept {
  const auto ptrUi64 = reinterpret_cast<const std::uint64_t*>(&guid);
  return ptrUi64[0] ^ ptrUi64[1];
}


std::uint64_t GUIDHash::operator()(const GUID& guid) const noexcept {
  return CalcGUIDHash(guid);
}
