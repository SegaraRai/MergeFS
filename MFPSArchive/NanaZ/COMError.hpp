#pragma once

#include <dokan/dokan.h>

#include "../SDK/Plugin/SourceCpp.hpp"

#include <string>

#include <Windows.h>


class COMError : public NtstatusError {
  const HRESULT hResult;

public:
  static NTSTATUS NTSTATUSFromHRESULT(HRESULT hResult) noexcept;
  static void CheckHRESULT(HRESULT hResult);

  COMError(HRESULT hResult);
  COMError(HRESULT hResult, const std::string& errorMessage);
  COMError(HRESULT hResult, const char* errorMessage);

  HRESULT GetHRESULT() const noexcept;
};
