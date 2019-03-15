#pragma once

#include <string>
#include <vector>
#include <Windows.h>


std::vector<std::wstring> ParseCommandLineArgs(LPCWSTR commandLineString);
std::vector<std::wstring> ParseCommandLineArgs(const std::wstring& commandLineString);

const std::vector<std::wstring>& GetCurrentCommandLineArgs();
