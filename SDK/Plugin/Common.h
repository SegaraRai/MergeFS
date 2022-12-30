#pragma once

#if !defined(FROMLIBMERGEFS) && !defined(FROMPLUGIN) && !defined(FROMCLIENT)
# define FROMPLUGIN
#endif

#include "../LibMergeFS.h"

#ifndef DOKAN_VERSION
# error include dokan.h before including this
#endif


#define MERGEFS_PLUGIN_INITCODE_SUCCESS                 ((DWORD) 0)
#define MERGEFS_PLUGIN_INITCODE_OTHER                   ((DWORD) 1)
#define MERGEFS_PLUGIN_INITCODE_W32                     ((DWORD) 2)
#define MERGEFS_PLUGIN_INITCODE_ALLOCATIONFAILED        ((DWORD) 3)
#define MERGEFS_PLUGIN_INITCODE_ALREADYEXISTS           ((DWORD) 4)
#define MERGEFS_PLUGIN_INITCODE_INCOMPATIBLE            ((DWORD) 5)
#define MERGEFS_PLUGIN_INITCODE_WRONGTYPE               ((DWORD) 6)


typedef DWORD tagPLUGIN_INITCODE;

#ifdef __cplusplus
enum class PLUGIN_INITCODE : tagPLUGIN_INITCODE {
  Success = MERGEFS_PLUGIN_INITCODE_SUCCESS,
  Other = MERGEFS_PLUGIN_INITCODE_OTHER,
  W32 = MERGEFS_PLUGIN_INITCODE_W32,
  AllocationFailed = MERGEFS_PLUGIN_INITCODE_ALLOCATIONFAILED,
  AlreadyExists = MERGEFS_PLUGIN_INITCODE_ALREADYEXISTS,
  WrongType = MERGEFS_PLUGIN_INITCODE_WRONGTYPE,
  Incompatible = MERGEFS_PLUGIN_INITCODE_INCOMPATIBLE,
};
#else
typedef tagPLUGIN_INITCODE PLUGIN_INITCODE;
#endif


typedef void(WINAPI *PDokanMapKernelToUserCreateFileFlags)(ACCESS_MASK DesiredAccess, ULONG FileAttributes, ULONG CreateOptions, ULONG CreateDisposition, ACCESS_MASK* outDesiredAccess, DWORD* outFileAttributesAndFlags, DWORD* outCreationDisposition) MFNOEXCEPT;
typedef NTSTATUS(WINAPI *PDokanNtStatusFromWin32)(DWORD Error) MFNOEXCEPT;
typedef BOOL(WINAPI *PDokanIsNameInExpression)(LPCWSTR Expression, LPCWSTR Name, BOOL IgnoreCase) MFNOEXCEPT;
typedef BOOL(WINAPI *PDokanResetTimeout)(ULONG Timeout, PDOKAN_FILE_INFO DokanFileInfo) MFNOEXCEPT;
typedef HANDLE(WINAPI *PDokanOpenRequestorToken)(PDOKAN_FILE_INFO DokanFileInfo) MFNOEXCEPT;


#pragma pack(push, 1)


typedef struct {
  PDokanMapKernelToUserCreateFileFlags DokanMapKernelToUserCreateFileFlags;
  PDokanNtStatusFromWin32 DokanNtStatusFromWin32;
  PDokanIsNameInExpression DokanIsNameInExpression;
  PDokanResetTimeout DokanResetTimeout;
  PDokanOpenRequestorToken DokanOpenRequestorToken;
} PLUGIN_DOKANFUNCS;


typedef struct {
  DWORD mergefsVersion;
  ULONG dokanVersion;
  ULONG dokanMinCompatVersion;
  ULONG dokanDriverVersion;
  PLUGIN_DOKANFUNCS dokanFuncs;
} PLUGIN_INITIALIZE_INFO;


typedef struct {
  LPCWSTR FileName;
  BOOL CaseSensitive;
  LPCSTR OptionsJSON;
} PLUGIN_INITIALIZE_MOUNT_INFO;


#ifdef FROMLIBMERGEFS
static_assert(sizeof(PLUGIN_DOKANFUNCS) == 5 * sizeof(void*));
static_assert(sizeof(PLUGIN_INITIALIZE_INFO) == 4 * 4 + sizeof(PLUGIN_DOKANFUNCS));
static_assert(sizeof(PLUGIN_INITIALIZE_MOUNT_INFO) == 1 * 4 + 2 * sizeof(void*));
#endif


#pragma pack(pop)


#ifdef FROMLIBMERGEFS
namespace External::Plugin {
#endif

MFEXTERNC MFPEXPORT const PLUGIN_INFO* WINAPI SGetPluginInfo() MFNOEXCEPT;
MFEXTERNC MFPEXPORT PLUGIN_INITCODE WINAPI SInitialize(const PLUGIN_INITIALIZE_INFO* InitializeInfo) MFNOEXCEPT;

#ifdef FROMLIBMERGEFS
}
#endif
