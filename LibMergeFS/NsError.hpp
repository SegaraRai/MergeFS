#pragma once

#include <stdexcept>

#include <Windows.h>


class NsError : public std::runtime_error {
  const NTSTATUS status;

protected:
  NsError(NTSTATUS status, const char* string);

public:
  NsError(NTSTATUS status);

  NTSTATUS GetStatus() const;
  operator NTSTATUS() const;
};


class W32Error : public NsError {
  const DWORD error;

public:
  W32Error(DWORD error = GetLastError());

  DWORD GetError() const;
};
