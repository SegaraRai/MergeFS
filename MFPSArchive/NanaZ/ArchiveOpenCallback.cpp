#define INITGUID

#include <7z/CPP/Common/Common.h>

#include <Windows.h>

#include "ArchiveOpenCallback.hpp"



ArchiveOpenCallback::ArchiveOpenCallback(PasswordCallback passwordCallback) :
  passwordCallback(passwordCallback)
{}


STDMETHODIMP ArchiveOpenCallback::SetTotal([[maybe_unused]] const UInt64* files, [[maybe_unused]] const UInt64* bytes) {
  return S_OK;
}


STDMETHODIMP ArchiveOpenCallback::SetCompleted([[maybe_unused]] const UInt64* files, [[maybe_unused]] const UInt64* bytes) {
  return S_OK;
}


STDMETHODIMP ArchiveOpenCallback::CryptoGetTextPassword(BSTR* password) {
  try {
    if (!passwordCallback) {
      return E_ABORT;
    }
    const auto passwordN = passwordCallback();
    if (!passwordN) {
      return E_ABORT;
    }
    *password = ::SysAllocString(passwordN.value().c_str());
    return *password ? S_OK : E_OUTOFMEMORY;
  } catch (...) {}
  return E_FAIL;
}
