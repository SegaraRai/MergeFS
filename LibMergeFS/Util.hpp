#pragma once

#include <string>
#include <string_view>


std::wstring FilenameToKey(std::wstring_view filename, bool caseSensitive);
