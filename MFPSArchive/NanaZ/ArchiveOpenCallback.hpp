#pragma once

#include <7z/CPP/Common/Common.h>
#include <7z/CPP/7zip/IPassword.h>
#include <7z/CPP/7zip/Archive/IArchive.h>

#include "7zGUID.hpp"

#include <functional>
#include <optional>
#include <string>

#include <Windows.h>
#include <winrt/base.h>


class ArchiveOpenCallback : public winrt::implements<ArchiveOpenCallback, IArchiveOpenCallback, ICryptoGetTextPassword> {
public:
  using PasswordCallback = std::function<std::optional<std::wstring>()>;

private:
  PasswordCallback passwordCallback;

public:
  ArchiveOpenCallback(PasswordCallback passwordCallback = nullptr);

  // IArchiveOpenCallback
  STDMETHOD(SetTotal)(const UInt64* files, const UInt64* bytes);
  STDMETHOD(SetCompleted)(const UInt64* files, const UInt64* bytes);

  // ICryptoGetTextPassword
  STDMETHOD(CryptoGetTextPassword)(BSTR* password);
};
