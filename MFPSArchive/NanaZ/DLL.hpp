#pragma once

#include <Windows.h>


class DLL final {
  HMODULE hModule;

public:
  DLL(const DLL&) = delete;
  DLL& operator=(const DLL&) = delete;

  DLL(LPCWSTR FileName);
  ~DLL();

  FARPROC GetProc(LPCSTR ProcName) const;
};
