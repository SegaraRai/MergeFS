#pragma comment(lib, "dokan1.lib")
#pragma comment(lib, "dokannp1.lib")

#define NOMINMAX

#define FROMLIBMERGEFS

#include "../SDK/LibMergeFS.h"

#include "MountStore.hpp"

#include <Windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace std::literals;

using PLUGIN_ID = MountStore::PLUGIN_ID;
using MOUNT_ID = MountStore::MOUNT_ID;


namespace {
  constexpr GUID EmptyGUID{};

  
  std::optional<MountStore> gMountStoreN;
  DWORD gLastError = MERGEFS_ERROR_SUCCESS;


  template<typename T>
  DWORD WrapExceptionImpl(const T& func, DWORD* mergefsError) noexcept {
    static_assert(std::is_same_v<decltype(func()), DWORD>);

    try {
      *mergefsError = func();
      return ERROR_SUCCESS;
    } catch (Mount::DokanMainError&) {
      *mergefsError = MERGEFS_ERROR_DOKAN_MAIN_ERROR;
      return ERROR_SUCCESS;
    } catch (std::invalid_argument&) {
      return ERROR_INVALID_PARAMETER;
    } catch (std::domain_error&) {
      return ERROR_INVALID_PARAMETER;
    } catch (std::length_error&) {
      return ERROR_INVALID_PARAMETER;
    } catch (std::out_of_range&) {
      return ERROR_NOACCESS;
    } catch (std::bad_optional_access&) {
      return ERROR_NOACCESS;
    } catch (std::range_error&) {
      return ERROR_INVALID_PARAMETER;
    } catch (std::ios_base::failure&) {
      return ERROR_IO_DEVICE;
    } catch (std::bad_typeid&) {
      return ERROR_NOACCESS;
    } catch (std::bad_cast&) {
      return ERROR_NOACCESS;
    } catch (std::bad_weak_ptr&) {
      return ERROR_NOACCESS;
    } catch (std::bad_function_call&) {
      return ERROR_NOACCESS;
    } catch (std::bad_alloc&) {
      return ERROR_NOT_ENOUGH_MEMORY;
    } catch (...) {
      *mergefsError = MERGEFS_ERROR_GENERIC_FAILURE;
      return ERROR_SUCCESS;
    }
  }

  template<typename T>
  BOOL WrapException(const T& func) noexcept {
    static_assert(std::is_same_v<decltype(func()), DWORD>);

    DWORD mergefsError;
    const DWORD w32error = WrapExceptionImpl(func, &mergefsError);
    if (w32error != ERROR_SUCCESS) {
      gLastError = MERGEFS_ERROR_WINDOWS_ERROR;
      SetLastError(w32error);
    } else {
      gLastError = mergefsError;
    }
    return mergefsError == MERGEFS_ERROR_SUCCESS ? TRUE : FALSE;
  }

  template<typename T>
  BOOL WrapExceptionV(const T& func) noexcept {
    static_assert(std::is_same_v<decltype(func()), void>);

    return WrapException([&func]() -> DWORD {
      func();
      return MERGEFS_ERROR_SUCCESS;
    });
  }
}


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
  switch (fdwReason) {
    case DLL_PROCESS_DETACH:
      try {
        if (gMountStoreN) {
          gMountStoreN.value().UnmountAll();
          gMountStoreN.reset();
        }
      } catch (...) {}
      break;
  }
  return TRUE;
}


namespace Exports {
  DWORD WINAPI GetError(BOOL* win32error) MFNOEXCEPT {
    const DWORD lastError = gLastError;
    if (win32error) {
      const bool isWin32Error = lastError == MERGEFS_ERROR_WINDOWS_ERROR;
      *win32error = isWin32Error ? TRUE : FALSE;
      return isWin32Error ? ::GetLastError() : lastError;
    }
    return lastError;
  }


  BOOL WINAPI Init() MFNOEXCEPT {
    return WrapException([=]() -> DWORD {
      if (gMountStoreN) {
        return MERGEFS_ERROR_SUCCESS;
      }
      gMountStoreN.emplace();
      return MERGEFS_ERROR_SUCCESS;
    });
  }


  BOOL WINAPI Uninit() MFNOEXCEPT {
    return WrapException([=]() {
      if (!gMountStoreN) {
        return MERGEFS_ERROR_NOT_INITIALIZED;
      }
      {
        auto& mountStore = gMountStoreN.value();
        mountStore.UnmountAll();
      }
      gMountStoreN.reset();
      return MERGEFS_ERROR_SUCCESS;
    });
  }


  BOOL WINAPI AddSourcePlugin(LPCWSTR filename, BOOL front, PLUGIN_ID* outPluginId) MFNOEXCEPT {
    return WrapException([=]() -> DWORD {
      if (!gMountStoreN) {
        return MERGEFS_ERROR_NOT_INITIALIZED;
      }
      auto& mountStore = gMountStoreN.value();
      const auto pluginId = mountStore.AddSourcePlugin(filename, front);
      if (outPluginId) {
        *outPluginId = pluginId;
      }
      return MERGEFS_ERROR_SUCCESS;
    });
  }


  BOOL WINAPI RemoveSourcePlugin(PLUGIN_ID pluginId) noexcept {
    return WrapException([=]() -> DWORD {
      if (!gMountStoreN) {
        return MERGEFS_ERROR_NOT_INITIALIZED;
      }
      auto& mountStore = gMountStoreN.value();
      return mountStore.RemoveSourcePlugin(pluginId) ? MERGEFS_ERROR_SUCCESS : MERGEFS_ERROR_INVALID_PLUGIN_ID;
    });
  }


  BOOL WINAPI GetSourcePlugins(DWORD* outNumPluginIds, PLUGIN_ID* outPluginIds, DWORD maxPluginIds) MFNOEXCEPT {
    return WrapException([=]() -> DWORD {
      if (!gMountStoreN) {
        return MERGEFS_ERROR_NOT_INITIALIZED;
      }

      auto& mountStore = gMountStoreN.value();

      const std::size_t count = mountStore.CountSourcePlugins();

      if (outNumPluginIds) {
        *outNumPluginIds = static_cast<DWORD>(count);
      }

      if (outPluginIds) {
        const auto pluginIds = mountStore.ListSourcePlugins();

        const std::size_t maxEntries = std::min<std::size_t>(count, maxPluginIds);
        for (std::size_t i = 0; i < maxEntries; i++) {
          outPluginIds[i] = pluginIds[i];
        }

        if (maxPluginIds < count) {
          return MERGEFS_ERROR_MORE_DATA;
        }
      }

      return MERGEFS_ERROR_SUCCESS;
    });
  }


  BOOL WINAPI SetSourcePluginOrder(const PLUGIN_ID* pluginIds, DWORD numPluginIds) MFNOEXCEPT {
    return WrapException([=]() -> DWORD {
      if (!gMountStoreN) {
        return MERGEFS_ERROR_NOT_INITIALIZED;
      }

      auto& mountStore = gMountStoreN.value();

      if (numPluginIds != mountStore.CountSourcePlugins()) {
        return MERGEFS_ERROR_INVALID_PARAMETER;
      }

      for (DWORD i = 0; i < numPluginIds; i++) {
        const auto pluginId = pluginIds[i];
        if (!mountStore.HasSourcePlugin(pluginId)) {
          return MERGEFS_ERROR_INVALID_PLUGIN_ID;
        }
      }

      // TODO: handle error code by MountStore (SourcePluginStore)
      return mountStore.SetSourcePluginOrder(pluginIds, numPluginIds) ? MERGEFS_ERROR_SUCCESS : MERGEFS_ERROR_GENERIC_FAILURE;
    });
  }


  BOOL WINAPI GetSourcePluginInfo(PLUGIN_ID pluginId, PLUGIN_INFO_EX* pluginInfoEx) MFNOEXCEPT {
    return WrapException([=]() -> DWORD {
      if (!gMountStoreN) {
        return MERGEFS_ERROR_NOT_INITIALIZED;
      }

      auto& mountStore = gMountStoreN.value();

      if (!mountStore.HasSourcePlugin(pluginId)) {
        return MERGEFS_ERROR_INVALID_PLUGIN_ID;
      }

      const auto& sourcePlugin = mountStore.GetSourcePlugin(pluginId);

      if (pluginInfoEx) {
        pluginInfoEx->filename = sourcePlugin.pluginFilePath.c_str();
        pluginInfoEx->pluginInfo = sourcePlugin.pluginInfo;
      }

      return MERGEFS_ERROR_SUCCESS;
    });
  }


  BOOL WINAPI Mount(const MOUNT_INITIALIZE_INFO* mountInitializeInfo, PMountCallback callback, MOUNT_ID* outMountId) MFNOEXCEPT {
    return WrapException([=]() -> DWORD {
      if (!gMountStoreN) {
        return MERGEFS_ERROR_NOT_INITIALIZED;
      }

      auto& mountStore = gMountStoreN.value();

      if (!mountInitializeInfo) {
        return MERGEFS_ERROR_INVALID_PARAMETER;
      }

      std::vector<std::pair<PLUGIN_ID, PLUGIN_INITIALIZE_MOUNT_INFO>> sources(mountInitializeInfo->numSources);
      for (std::size_t i = 0; i < mountInitializeInfo->numSources; i++) {
        const auto& mountSourceInitializeInfo = mountInitializeInfo->sources[i];
        PLUGIN_ID pluginId = PLUGIN_ID_NULL;
        if (memcmp(&mountSourceInitializeInfo.sourcePluginGUID, &EmptyGUID, sizeof(GUID))) {
          pluginId = mountStore.GetSourcePluginIdFromGUID(mountSourceInitializeInfo.sourcePluginGUID).value_or(PLUGIN_ID_NULL);
          if (pluginId == PLUGIN_ID_NULL) {
            return MERGEFS_ERROR_INEXISTENT_PLUGIN;
          }
        } else if (mountSourceInitializeInfo.sourcePluginFilename && mountSourceInitializeInfo.sourcePluginFilename[0] != L'\0') {
          pluginId = mountStore.GetSourcePluginIdFromFilename(mountSourceInitializeInfo.sourcePluginFilename).value_or(PLUGIN_ID_NULL);
          if (pluginId == PLUGIN_ID_NULL) {
            return MERGEFS_ERROR_INEXISTENT_PLUGIN;
          }
        }
        sources[i] = std::make_pair(pluginId, PLUGIN_INITIALIZE_MOUNT_INFO{
          mountSourceInitializeInfo.mountSource,
          mountInitializeInfo->caseSensitive,
          mountSourceInitializeInfo.sourcePluginOptionsJSON,
        });
      }

      const auto mountId = mountStore.Mount(mountInitializeInfo->mountPoint, mountInitializeInfo->writable, mountInitializeInfo->metadataFileName, mountInitializeInfo->deferCopyEnabled, mountInitializeInfo->caseSensitive, sources, [callback](MOUNT_ID mountId, const MOUNT_INFO& mountInfo, int dokanMainResult) {
        callback(mountId, &mountInfo, dokanMainResult);
      });

      if (outMountId) {
        *outMountId = mountId;
      }

      return MERGEFS_ERROR_SUCCESS;
    });
  }


  BOOL WINAPI GetMounts(DWORD* outNumMountIds, MOUNT_ID* outMountIds, DWORD maxMountIds) MFNOEXCEPT {
    return WrapException([=]() -> DWORD {
      if (!gMountStoreN) {
        return MERGEFS_ERROR_NOT_INITIALIZED;
      }

      auto& mountStore = gMountStoreN.value();

      const std::size_t count = mountStore.CountMounts();

      if (outNumMountIds) {
        *outNumMountIds = static_cast<DWORD>(count);
      }

      if (outMountIds) {
        const auto mountIds = mountStore.ListMounts();

        const std::size_t maxEntries = std::min<std::size_t>(count, maxMountIds);
        for (std::size_t i = 0; i < maxEntries; i++) {
          outMountIds[i] = mountIds[i];
        }

        if (maxMountIds < count) {
          return MERGEFS_ERROR_MORE_DATA;
        }
      }

      return MERGEFS_ERROR_SUCCESS;
    });
  }


  BOOL WINAPI GetMountInfo(MOUNT_ID mountId, MOUNT_INFO* outMountInfo) MFNOEXCEPT {
    return WrapException([=]() {
      if (!gMountStoreN) {
        return MERGEFS_ERROR_NOT_INITIALIZED;
      }

      auto& mountStore = gMountStoreN.value();

      if (!mountStore.HasMount(mountId)) {
        return MERGEFS_ERROR_INVALID_MOUNT_ID;
      }

      if (outMountInfo) {
        *outMountInfo = mountStore.GetMountInfo(mountId);
      }

      return MERGEFS_ERROR_SUCCESS;
    });
  }


  BOOL WINAPI SafeUnmount(MOUNT_ID mountId) MFNOEXCEPT {
    return WrapException([=]() -> DWORD {
      if (!gMountStoreN) {
        return MERGEFS_ERROR_NOT_INITIALIZED;
      }

      auto& mountStore = gMountStoreN.value();

      if (!mountStore.HasMount(mountId)) {
        return MERGEFS_ERROR_INVALID_MOUNT_ID;
      }
      return mountStore.SafeUnmount(mountId) ? MERGEFS_ERROR_SUCCESS : MERGEFS_ERROR_GENERIC_FAILURE;
    });
  }


  BOOL WINAPI Unmount(MOUNT_ID mountId) MFNOEXCEPT {
    return WrapException([=]() -> DWORD {
      if (!gMountStoreN) {
        return MERGEFS_ERROR_NOT_INITIALIZED;
      }
      auto& mountStore = gMountStoreN.value();
      return mountStore.Unmount(mountId) ? MERGEFS_ERROR_SUCCESS : MERGEFS_ERROR_INVALID_MOUNT_ID;
    });
  }


  BOOL WINAPI SafeUnmountAll() MFNOEXCEPT {
    return WrapException([=]() {
      if (!gMountStoreN) {
        return MERGEFS_ERROR_NOT_INITIALIZED;
      }
      auto& mountStore = gMountStoreN.value();
      mountStore.SafeUnmountAll();
      return MERGEFS_ERROR_SUCCESS;
    });
  }


  BOOL WINAPI UnmountAll() MFNOEXCEPT {
    return WrapException([=]() {
      if (!gMountStoreN) {
        return MERGEFS_ERROR_NOT_INITIALIZED;
      }
      auto& mountStore = gMountStoreN.value();
      mountStore.UnmountAll();
      return MERGEFS_ERROR_SUCCESS;
    });
  }
}
