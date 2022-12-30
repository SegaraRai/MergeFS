#pragma comment(lib, "dokan2.lib")
#pragma comment(lib, "dokannp2.lib")

#define NOMINMAX

#define FROMLIBMERGEFS

#include "../SDK/LibMergeFS.h"

#include "MountStore.hpp"

#include <Windows.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <ios>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
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
  std::shared_mutex gMutex;
  MERGEFS_ERROR_INFO gLastErrorInfo{
    MERGEFS_ERROR_SUCCESS,
  };


  namespace SetError {
    void SetSuccessError() {
      gLastErrorInfo.errorCode = MERGEFS_ERROR_SUCCESS;
    }

    void SetGenericFailureError() {
      gLastErrorInfo.errorCode = MERGEFS_ERROR_GENERIC_FAILURE;
    }

    void SetMergeFSError(DWORD errorCode) {
      assert(errorCode != MERGEFS_ERROR_WINDOWS_ERROR);
      assert(errorCode != MERGEFS_ERROR_DOKAN_MAIN_ERROR);
      gLastErrorInfo.errorCode = errorCode;
    }

    void SetWindowsError(DWORD windowsErrorCode = GetLastError()) {
      assert(windowsErrorCode != ERROR_SUCCESS);
      gLastErrorInfo.errorCode = MERGEFS_ERROR_WINDOWS_ERROR;
      gLastErrorInfo.vendorError.windowsErrorCode = windowsErrorCode;
    }

    void SetDokanMainError(int dokanMainResult) {
      assert(dokanMainResult != DOKAN_SUCCESS);
      gLastErrorInfo.errorCode = MERGEFS_ERROR_DOKAN_MAIN_ERROR;
      gLastErrorInfo.vendorError.dokanMainResult = dokanMainResult;
    }
  }


  template<typename T>
  BOOL WrapException(const T& func) noexcept {
    using namespace SetError;

    static_assert(std::is_same_v<decltype(func()), DWORD>);

    try {
      const auto error = func();
      if (error == MERGEFS_ERROR_SUCCESS) {
        SetSuccessError();
        return TRUE;
      }
      SetMergeFSError(error);
    } catch (Mount::DokanMainError& dokanMainError) {
      SetDokanMainError(dokanMainError.GetError());
    } catch (std::invalid_argument&) {
      SetWindowsError(ERROR_INVALID_PARAMETER);
    } catch (std::domain_error&) {
      SetWindowsError(ERROR_INVALID_PARAMETER);
    } catch (std::length_error&) {
      SetWindowsError(ERROR_INVALID_PARAMETER);
    } catch (std::out_of_range&) {
      SetWindowsError(ERROR_NOACCESS);
    } catch (std::bad_optional_access&) {
      SetWindowsError(ERROR_NOACCESS);
    } catch (std::range_error&) {
      SetWindowsError(ERROR_INVALID_PARAMETER);
    } catch (std::ios_base::failure&) {
      SetWindowsError(ERROR_IO_DEVICE);
    } catch (std::bad_typeid&) {
      SetWindowsError(ERROR_NOACCESS);
    } catch (std::bad_cast&) {
      SetWindowsError(ERROR_NOACCESS);
    } catch (std::bad_weak_ptr&) {
      SetWindowsError(ERROR_NOACCESS);
    } catch (std::bad_function_call&) {
      SetWindowsError(ERROR_NOACCESS);
    } catch (std::bad_alloc&) {
      SetWindowsError(ERROR_NOT_ENOUGH_MEMORY);
    } catch (...) {
      SetGenericFailureError();
    }

    return FALSE;
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
        std::lock_guard lock(gMutex);
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
  DWORD WINAPI LMF_GetLastError(BOOL* win32error) MFNOEXCEPT {
    try {
      std::shared_lock lock(gMutex);
      const DWORD lastError = gLastErrorInfo.errorCode;
      if (win32error) {
        const bool isWin32Error = lastError == MERGEFS_ERROR_WINDOWS_ERROR;
        *win32error = isWin32Error ? TRUE : FALSE;
        return isWin32Error ? GetLastError() : lastError;
      }
      return lastError;
    } catch (...) {}
    return MERGEFS_ERROR_GENERIC_FAILURE;
  }


  BOOL WINAPI LMF_GetLastErrorInfo(MERGEFS_ERROR_INFO* ptrErrorInfo) MFNOEXCEPT {
    return WrapExceptionV([=]() {
      std::shared_lock lock(gMutex);
      if (ptrErrorInfo) {
        *ptrErrorInfo = gLastErrorInfo;
      }
    });
  }


  BOOL WINAPI LMF_Init() MFNOEXCEPT {
    return WrapException([=]() -> DWORD {
      std::lock_guard lock(gMutex);
      if (gMountStoreN) {
        return MERGEFS_ERROR_SUCCESS;
      }
      gMountStoreN.emplace();
      return MERGEFS_ERROR_SUCCESS;
    });
  }


  BOOL WINAPI LMF_Uninit() MFNOEXCEPT {
    return WrapException([=]() {
      std::lock_guard lock(gMutex);
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


  BOOL WINAPI LMF_AddSourcePlugin(LPCWSTR filename, BOOL front, PLUGIN_ID* outPluginId) MFNOEXCEPT {
    return WrapException([=]() -> DWORD {
      std::lock_guard lock(gMutex);
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


  BOOL WINAPI LMF_RemoveSourcePlugin(PLUGIN_ID pluginId) noexcept {
    return WrapException([=]() -> DWORD {
      std::lock_guard lock(gMutex);
      if (!gMountStoreN) {
        return MERGEFS_ERROR_NOT_INITIALIZED;
      }
      auto& mountStore = gMountStoreN.value();
      return mountStore.RemoveSourcePlugin(pluginId) ? MERGEFS_ERROR_SUCCESS : MERGEFS_ERROR_INVALID_PLUGIN_ID;
    });
  }


  BOOL WINAPI LMF_GetSourcePlugins(DWORD* outNumPluginIds, PLUGIN_ID* outPluginIds, DWORD maxPluginIds) MFNOEXCEPT {
    return WrapException([=]() -> DWORD {
      std::shared_lock lock(gMutex);

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


  BOOL WINAPI LMF_SetSourcePluginOrder(const PLUGIN_ID* pluginIds, DWORD numPluginIds) MFNOEXCEPT {
    return WrapException([=]() -> DWORD {
      std::lock_guard lock(gMutex);

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


  BOOL WINAPI LMF_GetSourcePluginInfo(PLUGIN_ID pluginId, PLUGIN_INFO_EX* pluginInfoEx) MFNOEXCEPT {
    return WrapException([=]() -> DWORD {
      std::shared_lock lock(gMutex);

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


  BOOL WINAPI LMF_Mount(const MOUNT_INITIALIZE_INFO* mountInitializeInfo, PMountCallback callback, MOUNT_ID* outMountId) MFNOEXCEPT {
    return WrapException([=]() -> DWORD {
      std::lock_guard lock(gMutex);

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

      VolumeInfoOverride volumeInfoOverride{};
      if (mountInitializeInfo->volumeInfoOverride.overrideFlags & MERGEFS_VIOF_VOLUMENAME) {
        volumeInfoOverride.VolumeName.emplace(mountInitializeInfo->volumeInfoOverride.VolumeName);
      }
      if (mountInitializeInfo->volumeInfoOverride.overrideFlags & MERGEFS_VIOF_VOLUMESERIALNUMBER) {
        volumeInfoOverride.VolumeSerialNumber.emplace(mountInitializeInfo->volumeInfoOverride.VolumeSerialNumber);
      }
      if (mountInitializeInfo->volumeInfoOverride.overrideFlags & MERGEFS_VIOF_MAXIMUMCOMPONENTLENGTH) {
        volumeInfoOverride.MaximumComponentLength.emplace(mountInitializeInfo->volumeInfoOverride.MaximumComponentLength);
      }
      if (mountInitializeInfo->volumeInfoOverride.overrideFlags & MERGEFS_VIOF_FILESYSTEMFLAGS) {
        volumeInfoOverride.FileSystemFlags.emplace(mountInitializeInfo->volumeInfoOverride.FileSystemFlags);
      }
      if (mountInitializeInfo->volumeInfoOverride.overrideFlags & MERGEFS_VIOF_FILESYSTEMNAME) {
        volumeInfoOverride.FileSystemName.emplace(mountInitializeInfo->volumeInfoOverride.FileSystemName);
      }
      if (mountInitializeInfo->volumeInfoOverride.overrideFlags & MERGEFS_VIOF_FREEBYTESAVAILABLE) {
        volumeInfoOverride.FreeBytesAvailable.emplace(mountInitializeInfo->volumeInfoOverride.FreeBytesAvailable);
      }
      if (mountInitializeInfo->volumeInfoOverride.overrideFlags & MERGEFS_VIOF_TOTALNUMBEROFBYTES) {
        volumeInfoOverride.TotalNumberOfBytes.emplace(mountInitializeInfo->volumeInfoOverride.TotalNumberOfBytes);
      }
      if (mountInitializeInfo->volumeInfoOverride.overrideFlags & MERGEFS_VIOF_TOTALNUMBEROFFREEBYTES) {
        volumeInfoOverride.TotalNumberOfFreeBytes.emplace(mountInitializeInfo->volumeInfoOverride.TotalNumberOfFreeBytes);
      }

      const auto mountId = mountStore.Mount(mountInitializeInfo->mountPoint, mountInitializeInfo->writable, mountInitializeInfo->metadataFileName, mountInitializeInfo->deferCopyEnabled, mountInitializeInfo->caseSensitive, volumeInfoOverride, sources, [callback](MOUNT_ID mountId, const MOUNT_INFO* ptrMountInfo, int dokanMainResult) {
        callback(mountId, ptrMountInfo, dokanMainResult);
      });

      if (outMountId) {
        *outMountId = mountId;
      }

      return MERGEFS_ERROR_SUCCESS;
    });
  }


  BOOL WINAPI LMF_GetMounts(DWORD* outNumMountIds, MOUNT_ID* outMountIds, DWORD maxMountIds) MFNOEXCEPT {
    return WrapException([=]() -> DWORD {
      std::shared_lock lock(gMutex);

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


  BOOL WINAPI LMF_GetMountInfo(MOUNT_ID mountId, MOUNT_INFO* outMountInfo) MFNOEXCEPT {
    return WrapException([=]() {
      std::shared_lock lock(gMutex);

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


  BOOL WINAPI LMF_SafeUnmount(MOUNT_ID mountId) MFNOEXCEPT {
    return WrapException([=]() -> DWORD {
      std::lock_guard lock(gMutex);
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


  BOOL WINAPI LMF_Unmount(MOUNT_ID mountId) MFNOEXCEPT {
    return WrapException([=]() -> DWORD {
      std::lock_guard lock(gMutex);
      if (!gMountStoreN) {
        return MERGEFS_ERROR_NOT_INITIALIZED;
      }
      auto& mountStore = gMountStoreN.value();
      return mountStore.Unmount(mountId) ? MERGEFS_ERROR_SUCCESS : MERGEFS_ERROR_INVALID_MOUNT_ID;
    });
  }


  BOOL WINAPI LMF_SafeUnmountAll() MFNOEXCEPT {
    return WrapException([=]() {
      std::lock_guard lock(gMutex);
      if (!gMountStoreN) {
        return MERGEFS_ERROR_NOT_INITIALIZED;
      }
      auto& mountStore = gMountStoreN.value();
      mountStore.SafeUnmountAll();
      return MERGEFS_ERROR_SUCCESS;
    });
  }


  BOOL WINAPI LMF_UnmountAll() MFNOEXCEPT {
    return WrapException([=]() {
      std::lock_guard lock(gMutex);
      if (!gMountStoreN) {
        return MERGEFS_ERROR_NOT_INITIALIZED;
      }
      auto& mountStore = gMountStoreN.value();
      mountStore.UnmountAll();
      return MERGEFS_ERROR_SUCCESS;
    });
  }
}
