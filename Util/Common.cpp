#include <array>
#include <climits>
#include <cstddef>
#include <string>

#include <Windows.h>

#include "Common.hpp"



namespace util {
  std::string ToLowerString(std::string_view string) {
    constexpr static auto LowerCharTable = ([]() constexpr {
      static_assert(CHAR_BIT == 8);
      std::array<char, 256> table{};
      for (int i = 0; i < 256; i++) {
        table[i] = 'A' <= i && i <= 'Z'
          ? i - 'A' + 'a'
          : i;
      }
      return table;
    })();

    std::string lowerString(string.size(), '\0');
    for (std::size_t i = 0; i < string.size(); i++) {
      lowerString[i] = LowerCharTable[static_cast<unsigned int>(string[i])];
    }
    return lowerString;
  }
}
