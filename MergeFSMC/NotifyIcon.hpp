#pragma once

#include <Windows.h>


class NotifyIcon {
  NOTIFYICONDATAW mNotifyIconData;
  bool mSetVersion;

public:
  NotifyIcon(const NOTIFYICONDATAW& notifyIconData, bool setVersion);

  BOOL Register();
  BOOL Unregister();
};
