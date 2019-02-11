#include <stdexcept>
#include <string>

#include <Windows.h>

#include "DLL.hpp"

using namespace std::literals;



DLL::DLL(LPCWSTR FileName) :
  hModule(LoadLibraryW(FileName))
{
  if (!hModule) {
    throw std::runtime_error(u8"failed to load library");
  }
}


DLL::~DLL() {
  FreeLibrary(hModule);
}


FARPROC DLL::GetProc(LPCSTR ProcName) const {
  using namespace std::literals;

  const auto address = GetProcAddress(hModule, ProcName);
  if (!address) {
    throw std::runtime_error(u8"failed to get procedure address of "s + ProcName);
  }
  return address;
}
