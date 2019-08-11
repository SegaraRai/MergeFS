#include <dokan/dokan.h>

#include <cassert>
#include <string>

#include <Windows.h>

#include "COMError.hpp"

using namespace std::literals;



namespace {
  std::string UInt32ToHexString(std::uint_fast32_t value) {
    constexpr auto Hex = "0123456789ABCDEF";

    return std::string({
      Hex[(value >> 28) & 0x0F],
      Hex[(value >> 24) & 0x0F],
      Hex[(value >> 20) & 0x0F],
      Hex[(value >> 16) & 0x0F],
      Hex[(value >> 12) & 0x0F],
      Hex[(value >>  8) & 0x0F],
      Hex[(value >>  4) & 0x0F],
      Hex[(value >>  0) & 0x0F],
    });
  }
}



NTSTATUS COMError::NTSTATUSFromHRESULT(HRESULT hResult) noexcept {
  if (SUCCEEDED(hResult)) {
    return STATUS_SUCCESS;
  }
  // TODO: should we check Customer flag?
  if (hResult & FACILITY_NT_BIT) {
    // NTSTATUS mapped
    // maybe this works? see HRESULT_FROM_NT
    return hResult & ~static_cast<NTSTATUS>(FACILITY_NT_BIT);
  }
  if (HRESULT_FACILITY(hResult) == FACILITY_WIN32) {
    // Win32 error coded mapped
    return Win32Error::NTSTATUSFromWin32(HRESULT_CODE(hResult));
  }
  return STATUS_UNSUCCESSFUL;
}


void COMError::CheckHRESULT(HRESULT hResult) {
  if (FAILED(hResult)) {
    throw COMError(hResult);
  }
}


COMError::COMError(HRESULT hResult) :
  // HRESULT is usually represented in hexadecimal
  NtstatusError(NTSTATUSFromHRESULT(hResult), "COM Error 0x"s + UInt32ToHexString(hResult)),
  hResult(hResult)
{
  assert(FAILED(hResult));
}


COMError::COMError(HRESULT hResult, const std::string& errorMessage) :
  NtstatusError(NTSTATUSFromHRESULT(hResult), errorMessage),
  hResult(hResult)
{
  assert(FAILED(hResult));
}


COMError::COMError(HRESULT hResult, const char* errorMessage) :
  NtstatusError(NTSTATUSFromHRESULT(hResult), errorMessage),
  hResult(hResult)
{
  assert(FAILED(hResult));
}


HRESULT COMError::GetHRESULT() const noexcept {
  return hResult;
}
