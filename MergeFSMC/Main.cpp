#include <yaml-cpp/yaml.h>
#include <nlohmann/json.hpp>

#include <array>
#include <cassert>
#include <cstddef>
#include <list>
#include <mutex>
#include <string>
#include <string_view>

#include <Windows.h>
#include <Windowsx.h>
#include <Shlwapi.h>

#include "resource.h"

#include "CommandLineArgs.hpp"
#include "EncodingConverter.hpp"
#include "MountManager.hpp"

using namespace std::literals;
using json = nlohmann::json;


namespace {
  struct MountError {
    std::wstring configFilepath;
    std::wstring mountPoint;
    std::wstring reason;
  };

  enum class CopyDataMessageId : ULONG_PTR {
    SecondInstanceLaunched = 1,
  };

  constexpr std::size_t PathBufferSize = 65600;
  constexpr std::size_t MenuPathLength = 32;
  constexpr auto ClassName = L"MergeFSMC.CLS";
  constexpr auto MutexName = L"MergeFSMC.MTX";
  constexpr auto WindowName = L"MergeFSMC.WND";
  constexpr auto ReadyWindowName = L"MergeFSMC.WNDRDY";
  constexpr UINT NotifyIconId = 6;
  constexpr UINT ProcessQueueMessageId = WM_APP + 0x1001;
  constexpr UINT NotifyIconCallbackMessageId = WM_APP + 0x1002;

  std::mutex gMutex;
  std::list<std::vector<std::wstring>> gSecondInstanceArgsQueue;
  std::list<MountError> gMountErrorQueue;
  auto& gMountManager = MountManager::GetInstance();
  HINSTANCE gHInstance;
  std::wstring gPluginsDirectory;


  // TODO: move
  std::wstring ToAbsoluteFilepath(const std::wstring& filepath) {
    const DWORD size = GetFullPathNameW(filepath.c_str(), 0, NULL, NULL);
    if (!size) {
      throw std::runtime_error("frist call of GetFullPathNameW failed");
    }
    auto buffer = std::make_unique<wchar_t[]>(size);
    DWORD size2 = GetFullPathNameW(filepath.c_str(), size, buffer.get(), NULL);
    if (!size2) {
      throw std::runtime_error("second call of GetFullPathNameW failed");
    }
    // trim trailing backslash
    if (buffer[size2 - 1] == L'\\') {
      buffer[--size2] = L'\0';
    }
    return std::wstring(buffer.get(), size2);
  }
}


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
    // IPC
    case WM_COPYDATA:
    {
      const auto ptrCopyDataStruct = reinterpret_cast<const COPYDATASTRUCT*>(lParam);

      switch (const CopyDataMessageId messageId = static_cast<CopyDataMessageId>(ptrCopyDataStruct->dwData); messageId) {
        case CopyDataMessageId::SecondInstanceLaunched:
          try {
            const auto argsJson = json::parse(std::string(reinterpret_cast<const char*>(ptrCopyDataStruct->lpData), ptrCopyDataStruct->cbData));

            std::vector<std::wstring> args(argsJson.size());
            for (std::size_t i = 0; i < argsJson.size(); i++) {
              args[i] = argsJson[i].get<std::wstring>();
            }

            gSecondInstanceArgsQueue.emplace_back(args);
            PostMessageW(hwnd, ProcessQueueMessageId, 0, 0);

            return TRUE;
          } catch (...) {}
          return FALSE;
      }
      return FALSE;
    }

    // internal
    case ProcessQueueMessageId:
    {
      for (const auto& args : gSecondInstanceArgsQueue) {
        if (args.empty()) {
          continue;
        }
        for (std::size_t i = 1; i < args.size(); i++) {
          // skip first argument because it is a filename of executable
          const auto& arg = args[i];

          try {
            gMountManager.AddMount(arg, [arg](MOUNT_ID mountId, int dokanMainResult, const MountManager::MountData& mountData, const MOUNT_INFO& mountInfo) -> void {
              if (dokanMainResult != 0) {
                std::lock_guard lock(gMutex);
                gMountErrorQueue.emplace_back(MountError{
                  arg,
                  mountInfo.mountPoint,
                  L"DokanMain returned code "s + std::to_wstring(dokanMainResult),
                });
              }
            });
          } catch (const MountManager::MountError& mountError) {
            std::lock_guard lock(gMutex);
            gMountErrorQueue.emplace_back(MountError{
              arg,
              mountError.mountPoint,
              mountError.errorMessage,
            });
          } catch (const std::exception& exception) {
            std::lock_guard lock(gMutex);
            gMountErrorQueue.emplace_back(MountError{
              arg,
              L""s,
              LocaleStringtoWString(exception.what()),
            });
          } catch (...) {
            std::lock_guard lock(gMutex);
            gMountErrorQueue.emplace_back(MountError{
              arg,
              L""s,
              L"an unknown error occurred"s,
            });
          }
        }
      }
      gSecondInstanceArgsQueue.clear();

      {
        std::unique_lock lock(gMutex);
        std::vector<MountError> mountErrorQueueCopy(gMountErrorQueue.cbegin(), gMountErrorQueue.cend());
        gMountErrorQueue.clear();
        lock.unlock();

        for (const auto& mountError : mountErrorQueueCopy) {
          std::wstring message = L"Failed to mount \""s + mountError.configFilepath + L"\""s;
          if (!mountError.mountPoint.empty()) {
            message += L" into \""s + mountError.mountPoint + L"\\"s;
          }
          message += L":\n  "s + mountError.reason;
          MessageBoxW(NULL, message.c_str(), L"MergeFSMC Mount Error", MB_ICONERROR | MB_OK);
        }
      }

      return TRUE;
    }

    // menu
    case WM_COMMAND:
    {
      if (lParam != 0 || HIWORD(wParam) != 0) {
        break;
      }

      const auto commandId = LOWORD(wParam);

      if (commandId >= IDMB_CTX_MOUNT_BEGIN && commandId < IDMB_CTX_MOUNT_END) {
        try {
          const auto idDiff = commandId - IDMB_CTX_MOUNT_BEGIN;
          const MOUNT_ID mountId = idDiff / IDMK_CTX_MOUNT_COEF;
          const auto mountCommandId = idDiff % IDMK_CTX_MOUNT_COEF;
          switch (mountCommandId) {
            case IDMB_CTX_MOUNT_OPEN:
            {
              const auto mountInfo = gMountManager.GetMountInfo(mountId);
              SHELLEXECUTEINFOW shellExecuteInfo{
                sizeof(shellExecuteInfo),
                SEE_MASK_DEFAULT,
                NULL,
                L"explore",
                mountInfo.mountPoint,
                NULL,
                NULL,
                SW_SHOWDEFAULT,
                NULL,
                NULL,
                NULL,
                NULL,
                0,
                NULL,
                NULL,
              };
              ShellExecuteExW(&shellExecuteInfo);
              // TODO: handle error
              return 0;
            }

            case IDMB_CTX_MOUNT_OPENCONFIG:
            {
              const auto& mountData = gMountManager.GetMountData(mountId);
              SHELLEXECUTEINFOW shellExecuteInfo{
                sizeof(shellExecuteInfo),
                SEE_MASK_DEFAULT,
                NULL,
                L"open",//L"edit",
                mountData.configFilepath.c_str(),
                NULL,
                NULL,
                SW_SHOWDEFAULT,
                NULL,
                NULL,
                NULL,
                NULL,
                0,
                NULL,
                NULL,
              };
              ShellExecuteExW(&shellExecuteInfo);
              // TODO: handle error
              return 0;
            }

            case IDMB_CTX_MOUNT_UNMOUNT:
              gMountManager.RemoveMount(mountId, true);
              return 0;
          }
        } catch (const MountManager::MergeFSError& mergefsError) {
          MessageBoxW(NULL, mergefsError.errorMessage.c_str(), L"MergeFSMC Error", MB_ICONERROR | MB_OK);
        } catch (const std::exception& exception) {
          MessageBoxW(NULL, LocaleStringtoWString(exception.what()).c_str(), L"MergeFSMC Error", MB_ICONERROR | MB_OK);
        } catch (...) {
          MessageBoxW(NULL, L"an unknown error occurred", L"MergeFSMC Error", MB_ICONERROR | MB_OK);
        }
        break;
      }

      if (commandId >= IDMB_CTX_PLUGIN_BEGIN && commandId < IDMB_CTX_PLUGIN_END) {
        try {
          const auto idDiff = commandId - IDMB_CTX_PLUGIN_BEGIN;
          const PLUGIN_ID pluginId = idDiff / IDMK_CTX_PLUGIN_COEF;
          const auto pluginCommandId = idDiff % IDMK_CTX_PLUGIN_COEF;
          switch (pluginCommandId) {
            case IDMB_CTX_PLUGIN_TOP:
            {
              const auto pluginInfo = gMountManager.GetPluginInfo(pluginId);

              std::wstring pluginBasename = pluginInfo.filename;
              const auto lastBackslashPos = pluginBasename.find_last_of(L'\\');
              if (lastBackslashPos != std::wstring::npos) {
                pluginBasename = pluginBasename.substr(lastBackslashPos + 1);
              }

              const std::wstring message =
                pluginBasename + L"\n"s +
                pluginInfo.pluginInfo.name + L" ver. " + pluginInfo.pluginInfo.versionString + L" ("s + std::to_wstring(pluginInfo.pluginInfo.version) + L")\n"s +
                pluginInfo.pluginInfo.description;
              MessageBoxW(NULL, message.c_str(), L"MergeFSMC Source Plugin", MB_ICONINFORMATION | MB_OK);

              return 0;
            }
          }
        } catch (const MountManager::MergeFSError& mergefsError) {
          MessageBoxW(NULL, mergefsError.errorMessage.c_str(), L"MergeFSMC Error", MB_ICONERROR | MB_OK);
        } catch (const std::exception& exception) {
          MessageBoxW(NULL, LocaleStringtoWString(exception.what()).c_str(), L"MergeFSMC Error", MB_ICONERROR | MB_OK);
        } catch (...) {
          MessageBoxW(NULL, L"an unknown error occurred", L"MergeFSMC Error", MB_ICONERROR | MB_OK);
        }
        break;
      }

      switch (commandId) {
        case IDM_CTX_EXIT:
          PostMessageW(hwnd, WM_CLOSE, 0, 0);
          return 0;

        case IDM_CTX_MOUNT:
        {
          static wchar_t sStrFile[PathBufferSize];
          sStrFile[0] = L'\0';
          OPENFILENAMEW openFilename{
            sizeof(openFilename),
            NULL,
            NULL,
            L"MergeFSMC Configuration File (*.mfcfg)\0*.mfcfg\0All Files (*.*)\0*.*\0\0",
            NULL,
            0,
            0,
            sStrFile,
            PathBufferSize,
            NULL,
            0,
            NULL,
            NULL,
            OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY,
            0,
            0,
            L"mfcfg",
            0,
            NULL,
            NULL,
            NULL,
            0,
            0,
          };
          if (GetOpenFileNameW(&openFilename)) {
            gSecondInstanceArgsQueue.emplace_back(std::vector<std::wstring>{L""s, sStrFile});
            PostMessageW(hwnd, ProcessQueueMessageId, 0, 0);
          }
          return 0;
        }

        case IDM_CTX_MOUNTS_UNMOUNTALL:
          gMountManager.UnmountAll(true);
          return 0;

        case IDM_CTX_PLUGINS_OPENDIR:
        {
          SHELLEXECUTEINFOW shellExecuteInfo{
            sizeof(shellExecuteInfo),
            SEE_MASK_DEFAULT,
            NULL,
            L"explore",
            gPluginsDirectory.c_str(),
            NULL,
            NULL,
            SW_SHOWDEFAULT,
            NULL,
            NULL,
            NULL,
            NULL,
            0,
            NULL,
            NULL,
          };
          ShellExecuteExW(&shellExecuteInfo);
          // TODO: handle error
          return 0;
        }
      }
      break;
    }

    // notify icon
    case NotifyIconCallbackMessageId:
    {
      if (HIWORD(lParam) != NotifyIconId) {
        break;
      }

      switch (LOWORD(lParam)) {
        case NIN_SELECT:
        case WM_CONTEXTMENU:
        {
          std::vector<HMENU> loadedMenuHandles;

          try {
            const HMENU hMenu = LoadMenuW(gHInstance, MAKEINTRESOURCEW(IDR_MENU1));
            if (hMenu == NULL) {
              throw std::runtime_error("failed to get hMenu of IDR_MENU1");
            }
            loadedMenuHandles.emplace_back(hMenu);

            const HMENU hSubMenu = GetSubMenu(hMenu, 0);
            if (hMenu == NULL) {
              throw std::runtime_error("failed to get hSubMenu for Context Menu");
            }

            const HMENU hMountsMenu = GetSubMenu(hSubMenu, 1);
            if (hMountsMenu != NULL) {
              const auto mounts = gMountManager.ListMounts();

              if (mounts.empty()) {
                // disable Unmount All
                const MENUITEMINFOW menuItemInfo{
                  sizeof(menuItemInfo),
                  MIIM_STATE,
                  MFT_STRING,
                  MFS_DISABLED,
                  IDM_CTX_MOUNTS_UNMOUNTALL,
                  NULL,
                  NULL,
                  NULL,
                  NULL,
                  NULL,
                  0,
                  NULL,
                };
                SetMenuItemInfoW(hMountsMenu, IDM_CTX_MOUNTS_UNMOUNTALL, FALSE, &menuItemInfo);
              }

              for (std::size_t i = 0; i < mounts.size(); i++) {
                const auto& [mountId, mountInfo] = mounts[i];

                const HMENU hMenuCopy = LoadMenuW(gHInstance, MAKEINTRESOURCEW(IDR_MENU1));
                if (hMenuCopy == NULL) {
                  continue;
                }

                loadedMenuHandles.emplace_back(hMenuCopy);

                const HMENU hMountSubMenu = GetSubMenu(hMenu, 1);
                if (hMountSubMenu == NULL) {
                  continue;
                }

                const UINT baseId = IDMB_CTX_MOUNT_BEGIN + IDMK_CTX_MOUNT_COEF * mountId;

                constexpr std::array<std::pair<UINT, UINT>, 3> IndexToIdMap{{
                  {0, IDMB_CTX_MOUNT_OPEN},
                  {1, IDMB_CTX_MOUNT_OPENCONFIG},
                  {3, IDMB_CTX_MOUNT_UNMOUNT},
                }};
                for (const auto& [index, id] : IndexToIdMap) {
                  const MENUITEMINFOW menuItemInfo{
                    sizeof(menuItemInfo),
                    MIIM_ID,
                    MFT_STRING,
                    MFS_ENABLED,
                    baseId + id,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    0,
                    NULL,
                  };
                  SetMenuItemInfoW(hMountSubMenu, static_cast<DWORD>(index), TRUE, &menuItemInfo);
                }

                wchar_t menuString[MenuPathLength];
                PathCompactPathExW(menuString, mountInfo.mountPoint, MenuPathLength, 0);

                const MENUITEMINFOW menuItemInfo{
                  sizeof(menuItemInfo),
                  MIIM_ID | MIIM_SUBMENU | MIIM_TYPE,
                  MFT_STRING,
                  MFS_ENABLED,
                  baseId + IDMB_CTX_MOUNT_TOP,
                  hMountSubMenu,
                  NULL,
                  NULL,
                  NULL,
                  menuString,
                  0,
                  NULL,
                };
                InsertMenuItemW(hMountsMenu, static_cast<DWORD>(i), TRUE, &menuItemInfo);
              }
            }

            const HMENU hPluginsMenu = GetSubMenu(hSubMenu, 2);
            if (hPluginsMenu != NULL) {
              const auto plugins = gMountManager.ListPlugins();
              for (std::size_t i = 0; i < plugins.size(); i++) {
                const auto& [pluginId, pluginInfo] = plugins[i];

                std::wstring menuString = pluginInfo.filename;
                const auto lastBackslashPos = menuString.find_last_of(L'\\');
                if (lastBackslashPos != std::wstring::npos) {
                  menuString = menuString.substr(lastBackslashPos + 1);
                }

                const MENUITEMINFOW menuItemInfo{
                  sizeof(menuItemInfo),
                  MIIM_ID | MIIM_TYPE,
                  MFT_STRING,
                  MFS_ENABLED,
                  IDMB_CTX_PLUGIN_BEGIN + IDMK_CTX_PLUGIN_COEF * pluginId + IDMB_CTX_PLUGIN_TOP,
                  NULL,
                  NULL,
                  NULL,
                  NULL,
                  const_cast<wchar_t*>(menuString.c_str()),
                  0,
                  NULL,
                };
                InsertMenuItemW(hPluginsMenu, static_cast<DWORD>(i), TRUE, &menuItemInfo);
              }
            }

            SetForegroundWindow(hwnd);

            const auto x = GET_X_LPARAM(wParam);
            const auto y = GET_Y_LPARAM(wParam);

            const UINT flags = TPM_LEFTALIGN;
            TrackPopupMenuEx(hSubMenu, flags, x, y, hwnd, NULL);
          } catch (...) {}

          for (const auto& hMenu : loadedMenuHandles) {
            DestroyMenu(hMenu);
          }

          return 0;
        }
      }
      break;
    }

    // on exit
    case WM_CLOSE:
      if (gMountManager.CountMounts() == 0 || MessageBoxW(NULL, L"Are you sure you want to exit MergeFSMC?\nAll mounts will be unmounted.", L"MergeFSMC", MB_ICONQUESTION | MB_OKCANCEL) == IDOK) {
        DestroyWindow(hwnd);
      }
      return 0;

    case WM_DESTROY:
      gMountManager.UnmountAll(true);
      PostQuitMessage(0);
      return 0;
  }

  return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd) {
  gHInstance = hInstance;


  const auto& args = GetCurrentCommandLineArgs();

  std::vector<std::wstring> sourceFilepaths(args);
  for (auto& sourceFilepath : sourceFilepaths) {
    sourceFilepath = ToAbsoluteFilepath(sourceFilepath);
  }


  {
    static wchar_t buffer[PathBufferSize];
    GetModuleFileNameW(hInstance, buffer, PathBufferSize);
    if (GetLastError() != ERROR_SUCCESS) {
      const std::wstring message = L"Initialization error: GetModuleFileNameW failed with code "s + std::to_wstring(GetLastError());
      MessageBoxW(NULL, message.c_str(), L"MergeFSMC Error", MB_ICONERROR | MB_OK);
      return 1;
    }

    gPluginsDirectory = std::wstring(buffer) + L"\\..\\..\\Plugins"s;

    gMountManager.AddPluginsByDirectory(gPluginsDirectory);
  }


  const auto hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_ICON1));
  if (hIcon == NULL) {
    const std::wstring message = L"Initialization error: LoadIconW failed with code "s + std::to_wstring(GetLastError());
    MessageBoxW(NULL, message.c_str(), L"MergeFSMC Error", MB_ICONERROR | MB_OK);
    return 1;
  }


  const WNDCLASSEXW wndClassExW{
    sizeof(wndClassExW),
    0,
    WindowProc,
    0,
    0,
    hInstance,
    hIcon,
    LoadCursorW(NULL, IDC_ARROW),
    reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)),
    NULL,
    ClassName,
    hIcon,
  };
  if (!RegisterClassExW(&wndClassExW)) {
    const std::wstring message = L"Initialization error: RegisterClassExW failed with code "s + std::to_wstring(GetLastError());
    MessageBoxW(NULL, message.c_str(), L"MergeFSMC Error", MB_ICONERROR | MB_OK);
    return 1;
  }


  HWND hWnd = CreateWindowExW(0, ClassName, WindowName, WS_OVERLAPPED, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, HWND_MESSAGE, NULL, hInstance, NULL);
  if (hWnd == NULL) {
    const std::wstring message = L"Initialization error: CreateWindowExW failed with code "s + std::to_wstring(GetLastError());
    MessageBoxW(NULL, message.c_str(), L"MergeFSMC Error", MB_ICONERROR | MB_OK);
    return 1;
  }

     
  SetLastError(ERROR_SUCCESS);
  const auto hMutex = CreateMutexW(NULL, TRUE, MutexName);
  if (GetLastError() != ERROR_SUCCESS) {
    // second instance

    if (GetLastError() != ERROR_ALREADY_EXISTS) {
      if (hMutex != NULL) {
        ReleaseMutex(hMutex);
      }
      const std::wstring message = L"Initialization error: CreateMutexW failed with code "s + std::to_wstring(GetLastError());
      MessageBoxW(NULL, message.c_str(), L"MergeFSMC Error", MB_ICONERROR | MB_OK);
      DestroyWindow(hWnd);
      return 1;
    }

    const auto hWndFirstInstance = FindWindowExW(HWND_MESSAGE, NULL, ClassName, ReadyWindowName);
    if (hWndFirstInstance == NULL) {
      if (hMutex != NULL) {
        ReleaseMutex(hMutex);
      }
      const std::wstring message = L"Initialization error: FindWindowExW failed with code "s + std::to_wstring(GetLastError());
      MessageBoxW(NULL, message.c_str(), L"MergeFSMC Error", MB_ICONERROR | MB_OK);
      DestroyWindow(hWnd);
      return 1;
    }

    const json argsJson = sourceFilepaths;
    const auto strArgsJson = argsJson.dump();
    COPYDATASTRUCT copyDataStruct{
      static_cast<ULONG_PTR>(CopyDataMessageId::SecondInstanceLaunched),
      static_cast<DWORD>(strArgsJson.size()),
      const_cast<PVOID>(reinterpret_cast<LPCVOID>(strArgsJson.c_str())),
    };
    SetLastError(ERROR_SUCCESS);
    SendMessageW(hWndFirstInstance, WM_COPYDATA, reinterpret_cast<WPARAM>(hWnd), reinterpret_cast<LPARAM>(&copyDataStruct));
    if (GetLastError() != ERROR_SUCCESS) {
      if (hMutex != NULL) {
        ReleaseMutex(hMutex);
      }
      const std::wstring message = L"Initialization error: SendMessageW failed with code "s + std::to_wstring(GetLastError());
      MessageBoxW(NULL, message.c_str(), L"MergeFSMC Error", MB_ICONERROR | MB_OK);
      DestroyWindow(hWnd);
      return 1;
    }

    if (hMutex != NULL) {
      ReleaseMutex(hMutex);
    }
    DestroyWindow(hWnd);

    return 0;
  }


  // process arguments
  gSecondInstanceArgsQueue.emplace_back(sourceFilepaths);
  if (!PostMessageW(hWnd, ProcessQueueMessageId, 0, 0)) {
    MessageBoxW(NULL, L"PostMessageW failed during initialization process; your mounts will not be activated for a while", L"MergeFSMC", MB_ICONWARNING | MB_OK);
  }


  // register notify icon
  const NOTIFYICONDATAW commonNotifyIconData{
    sizeof(commonNotifyIconData),
    hWnd,
    NotifyIconId,
    NIF_MESSAGE | NIF_ICON | NIF_TIP,
    NotifyIconCallbackMessageId,
    hIcon,
    L"MergeFSMC",
    0,
    0,
    {},
    {
      NOTIFYICON_VERSION_4,
    },
    {},
    NIIF_NONE,
    {},
    NULL,
  };

  {
    NOTIFYICONDATAW notifyIconData = commonNotifyIconData;
    if (!Shell_NotifyIconW(NIM_ADD, &notifyIconData)) {
      ReleaseMutex(hMutex);
      const std::wstring message = L"Initialization error: Shell_NotifyIconW [1] failed with code "s + std::to_wstring(GetLastError());
      MessageBoxW(NULL, message.c_str(), L"MergeFSMC Error", MB_ICONERROR | MB_OK);
      DestroyWindow(hWnd);
      return 1;
    }
  }

  {
    NOTIFYICONDATAW notifyIconData = commonNotifyIconData;
    if (!Shell_NotifyIconW(NIM_SETVERSION, &notifyIconData)) {
      ReleaseMutex(hMutex);
      const std::wstring message = L"Initialization error: Shell_NotifyIconW [2] failed with code "s + std::to_wstring(GetLastError());
      MessageBoxW(NULL, message.c_str(), L"MergeFSMC Error", MB_ICONERROR | MB_OK);
      DestroyWindow(hWnd);
      return 1;
    }
  }


  if (!SetWindowTextW(hWnd, ReadyWindowName)) {
    ReleaseMutex(hMutex);
    const std::wstring message = L"Initialization error: SetWindowTextW failed with code "s + std::to_wstring(GetLastError());
    MessageBoxW(NULL, message.c_str(), L"MergeFSMC Error", MB_ICONERROR | MB_OK);
    DestroyWindow(hWnd);
    return 1;
  }


  // message loop
  MSG msg;
  BOOL gmResult;
  while (true) {
    gmResult = GetMessageW(&msg, hWnd, 0, 0);
    if (gmResult == 0 || gmResult == -1) {
      break;
    }
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }


  // remove notify icon
  {
    NOTIFYICONDATAW notifyIconData = commonNotifyIconData;
    Shell_NotifyIconW(NIM_DELETE, &notifyIconData);
  }


  ReleaseMutex(hMutex);

  gMountManager.UnmountAll(true);


  if (gmResult == -1) {
    //return 1;
    return 0;
  }

  return msg.message == WM_QUIT ? static_cast<int>(msg.wParam) : 0;
}