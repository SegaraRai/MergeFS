#pragma once

#include <cstddef>
#include <string>

#include <Windows.h>


std::wstring ConvertFileContentToWString(const std::byte* data, std::size_t size);
std::string ConvertWStringToCodePage(std::wstring_view data, UINT codePage);
