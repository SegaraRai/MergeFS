#pragma once

#include <string>
#include <string_view>


std::wstring LocaleStringtoWString(std::string_view localeString);
std::wstring UTF8toWString(std::string_view utf8String);
std::string WStringtoUTF8(std::wstring_view wString);
