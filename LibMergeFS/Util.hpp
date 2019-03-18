#pragma once

#include <string>
#include <string_view>


std::wstring FilenameToKey(std::wstring_view filename, bool caseSensitive);
std::wstring_view GetBaseName(std::wstring_view filename);
