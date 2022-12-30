#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>


namespace FileNaming {
  namespace Internal {
    constexpr std::array<char, 41> InvalidCharactersA = {
      '\x00',
      '\x01',
      '\x02',
      '\x03',
      '\x04',
      '\x05',
      '\x06',
      '\x07',
      '\x08',
      '\x09',
      '\x0A',
      '\x0B',
      '\x0C',
      '\x0D',
      '\x0E',
      '\x0F',
      '\x10',
      '\x11',
      '\x12',
      '\x13',
      '\x14',
      '\x15',
      '\x16',
      '\x17',
      '\x18',
      '\x19',
      '\x1A',
      '\x1B',
      '\x1C',
      '\x1D',
      '\x1E',
      '\x1F',
      '<',
      '>',
      ':',
      '"',
      '/',
      '\\',
      '|',
      '?',
      '*',
    };
  }


  template<typename T>
  constexpr std::array<T, Internal::InvalidCharactersA.size()> InvalidCharacters = ([]() constexpr {
    std::array<T, Internal::InvalidCharactersA.size()> ret{};
    for (std::size_t i = 0; i < Internal::InvalidCharactersA.size(); i++) {
      ret[i] = static_cast<T>(Internal::InvalidCharactersA[i]);
    }
    return ret;
  })();


  template<typename T, std::size_t Size>
  constexpr std::array<T, Size> GenerateReplaceInvalidCharacterTable(T replacement) {
    std::array<T, Size> table{};
    for (std::size_t i = 0; i < Size; i++) {
      table[i] = static_cast<T>(i);
    }
    for (const auto& c : InvalidCharacters<unsigned char>) {
      table[c] = replacement;
    }
    return table;
  }


  template<typename T, std::size_t Size, std::size_t ExcludeListSize>
  constexpr std::array<T, Size> GenerateReplaceInvalidCharacterTable(T replacement, const std::array<T, ExcludeListSize>& excludeList) {
    auto table = GenerateReplaceInvalidCharacterTable<T, Size>(replacement);
    for (const auto& c : excludeList) {
      table[static_cast<std::size_t>(c)] = c;
    }
    return table;
  }


  template<typename T1, typename T2>
  std::basic_string<T1> ReplaceCharacters(std::basic_string_view<T1> src, const T2& replaceTable) {
    std::basic_string<T1> dest(src);
    for (auto& c : dest) {
      if (static_cast<std::size_t>(c) < replaceTable.size()) {
        c = replaceTable[static_cast<std::size_t>(c)];
      }
    }
    return dest;
  }


  template<wchar_t Replacement>
  std::wstring ReplaceInvalidCharacters(std::wstring_view src) {
    static constexpr auto table = GenerateReplaceInvalidCharacterTable<wchar_t, 128>(Replacement);
    return ReplaceCharacters(src, table);
  }

  template<wchar_t Replacement>
  std::wstring ReplaceInvalidCharactersExcludeForwardSlash(std::wstring_view src) {
    static constexpr std::array<wchar_t, 1> ExcludeList = {'/'};
    static constexpr auto table = GenerateReplaceInvalidCharacterTable<wchar_t, 128>(Replacement, ExcludeList);
    return ReplaceCharacters(src, table);
  }

  template<wchar_t Replacement>
  std::wstring ReplaceInvalidCharactersExcludeBackslash(std::wstring_view src) {
    static constexpr std::array<wchar_t, 1> ExcludeList = {'\\'};
    static constexpr auto table = GenerateReplaceInvalidCharacterTable<wchar_t, 128>(Replacement, ExcludeList);
    return ReplaceCharacters(src, table);
  }

  template<wchar_t Replacement>
  std::wstring ReplaceInvalidCharactersExcludeSlashes(std::wstring_view src) {
    static constexpr std::array<wchar_t, 2> ExcludeList = {'/', '\\'};
    static constexpr auto table = GenerateReplaceInvalidCharacterTable<wchar_t, 128>(Replacement, ExcludeList);
    return ReplaceCharacters(src, table);
  }
}
