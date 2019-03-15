#include <stdexcept>
#include <string>
#include <vector>
#include <Windows.h>

#include "CommandLineArgs.hpp"


std::vector<std::wstring> ParseCommandLineArgs(LPCWSTR commandLineString) {
  int numArgs = 0;
  auto args = CommandLineToArgvW(commandLineString, &numArgs);
  if (!args) {
    throw std::runtime_error("CommandLineToArgvW failed");
  }
  std::vector<std::wstring> vArgs(numArgs);
  for (int i = 0; i < numArgs; i++) {
    vArgs[i] = std::wstring(args[i]);
  }
  LocalFree(args);
  return vArgs;
}


std::vector<std::wstring> ParseCommandLineArgs(const std::wstring& commandLineString) {
  return ParseCommandLineArgs(commandLineString.c_str());
}


const std::vector<std::wstring>& GetCurrentCommandLineArgs() {
  static const std::vector<std::wstring> args = ParseCommandLineArgs(GetCommandLineW());
  return args;
}
