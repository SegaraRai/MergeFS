#include <Windows.h>

#include "NotifyIcon.hpp"



NotifyIcon::NotifyIcon(const NOTIFYICONDATAW& notifyIconData, bool setVersion) :
  mNotifyIconData(notifyIconData),
  mSetVersion(setVersion)
{
  // TODO: copy strings?
}


BOOL NotifyIcon::Register() {
  do {
    NOTIFYICONDATAW notifyIconData = mNotifyIconData;
    if (!Shell_NotifyIconW(NIM_ADD, &notifyIconData)) {
      if (GetLastError() == ERROR_TIMEOUT) {
        Sleep(1500);
        continue;
      }
      // TODO: return TRUE if notify icon is already registered
      return FALSE;
    }
  } while (false);

  if (mSetVersion) {
    NOTIFYICONDATAW notifyIconData = mNotifyIconData;
    if (!Shell_NotifyIconW(NIM_SETVERSION, &notifyIconData)) {
      return FALSE;
    }
  }

  return TRUE;
}


BOOL NotifyIcon::Unregister() {
  NOTIFYICONDATAW notifyIconData = mNotifyIconData;
  return Shell_NotifyIconW(NIM_DELETE, &notifyIconData);
}
