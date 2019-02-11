#include "GUIDUtil.hpp"

#include <cstdint>
#include <cstring>



static_assert(CHAR_BIT == 8);
static_assert(sizeof(std::uint64_t) == 8);
static_assert(sizeof(GUID) == 16);


std::uint64_t GUIDHash::operator()(const GUID& guid) const noexcept {
  const auto ptrUi6 = reinterpret_cast<const std::uint64_t*>(&guid);
  return ptrUi6[0] ^ ptrUi6[1];
}
