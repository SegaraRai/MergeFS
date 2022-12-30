#pragma once

#include "../dokan/dokan/dokan.h"


namespace DokanConfig {
  constexpr USHORT Version = DOKAN_VERSION;
  constexpr BOOLEAN SingleThread = FALSE;
  constexpr ULONG Options = DOKAN_OPTION_ALT_STREAM;
  constexpr ULONG Timeout = 0;
  constexpr ULONG AllocationUnitSize = 0;
  constexpr ULONG SectorSize = 0;
}
