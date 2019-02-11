#pragma once

#include <Windows.h>


class DLL {
  HMODULE hModule;

public:
  DLL(const DLL&) = delete;
  DLL& operator=(const DLL&) = delete;

  DLL(LPCWSTR FileName);
  virtual ~DLL();

  FARPROC GetProc(LPCSTR ProcName) const;
};
