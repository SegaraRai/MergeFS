#include <stdexcept>
#include <string>

#include <Windows.h>

#include "DLL.hpp"

using namespace std::literals;



DLL::DLL(LPCWSTR FileName) :
  hModule(LoadLibraryW(FileName))
{
  if (!hModule) {
    throw std::runtime_error("failed to load library");
  }
}


DLL::~DLL() {
  FreeLibrary(hModule);
}


FARPROC DLL::GetProc(LPCSTR ProcName) const {
  const auto address = GetProcAddress(hModule, ProcName);
  if (!address) {
    throw std::runtime_error("failed to get procedure address of "s + ProcName);
  }
  return address;
}
