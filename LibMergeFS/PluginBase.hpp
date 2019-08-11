#pragma once

#define FROMLIBMERGEFS

#include "../dokan/dokan/dokan.h"

#include "../SDK/Plugin/Common.h"

#include "GUIDUtil.hpp"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>


class PluginBase {
  static std::shared_ptr<std::unordered_set<GUID, GUIDHash>> gPtrPluginGuidSet;

  class DLL {
    const HMODULE hModule;

  public:
    DLL(DLL&&) = delete;
    DLL(const DLL&) = delete;

    DLL(LPCWSTR FileName);
    ~DLL();

    HMODULE GetHandle() const noexcept;
    operator HMODULE() const noexcept;

    template<typename T>
    T GetProc(LPCSTR ProcName) const {
      using namespace std::literals;

      const auto address = GetProcAddress(hModule, ProcName);
      if (!address) {
        throw std::runtime_error("failed to get procedure address of "s + ProcName);
      }
      return reinterpret_cast<T>(address);
    }
  };

public:
  using PLUGIN_TYPE = ::PLUGIN_TYPE;
  using PLUGIN_INITCODE = ::PLUGIN_INITCODE;
  using PLUGIN_INITIALIZE_INFO = ::PLUGIN_INITIALIZE_INFO;
  using PLUGIN_INFO = ::PLUGIN_INFO;

  using PSInitialize = decltype(&External::Plugin::SInitialize);
  using PSGetPluginInfo = decltype(&External::Plugin::SGetPluginInfo);

  class PluginInitError : std::runtime_error {
  public:
    const PLUGIN_INITCODE errorCode;
    const DWORD w32Code;
    const NTSTATUS status;

    PluginInitError(PLUGIN_INITCODE errorCode);

    operator PluginBase::PLUGIN_INITCODE() const noexcept;
  };

private:
  std::shared_ptr<std::unordered_set<GUID, GUIDHash>> ptrPluginGuidSet;

public:
  const std::wstring pluginFilePath;
  const PLUGIN_TYPE pluginType;

private:
  const PLUGIN_INITIALIZE_INFO initializeInfo;

protected:
  const DLL dll;

private:
  const PSGetPluginInfo SGetPluginInfo;
  const PSInitialize SInitialize;

public:
  const PLUGIN_INFO pluginInfo;

protected:
  PluginBase(std::wstring_view pluginFilePath, PLUGIN_TYPE pluginType);
  virtual ~PluginBase();
};
