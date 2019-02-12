#pragma once

#if !defined(FROMLIBMERGEFS) && !defined(FROMPLUGIN) && !defined(FROMCLIENT)
# define FROMCLIENT
#endif

#ifdef FROMCLIENT
# ifndef DWORD
#  include <Windows.h>
# endif
#else
# ifndef DOKAN_VERSION
#  ifdef FROMLIBMERGEFS
#   include "../dokan/dokan/dokan.h"
#  else
#   error include dokan.h before including this
#  endif
# endif
#endif


#define MERGEFS_VERSION                         ((DWORD) 0x00000001)
#define MERGEFS_PLUGIN_INTERFACE_VERSION        ((DWORD) 0x00000002)

#define MERGEFS_PLUGIN_TYPE_SOURCE        ((DWORD) 1)

#define MERGEFS_ERROR_SUCCESS                           ((DWORD) 0)
#define MERGEFS_ERROR_WINDOWS_ERROR                     ((DWORD) 0x00000001)
#define MERGEFS_ERROR_GENERIC_FAILURE                   ((DWORD) 0x00010000)
#define MERGEFS_ERROR_MORE_DATA                         ((DWORD) 0x00010001)
#define MERGEFS_ERROR_INCOMPATIBLE_PLATFORM             ((DWORD) 0x00020001)
#define MERGEFS_ERROR_INCOMPATIBLE_OS_VERSION           ((DWORD) 0x00020002)
#define MERGEFS_ERROR_INCOMPATIBLE_DOKAN_VERSION        ((DWORD) 0x00020003)
#define MERGEFS_ERROR_INCOMPATIBLE_PLUGIN_TYPE          ((DWORD) 0x00020004)
#define MERGEFS_ERROR_INCOMPATIBLE_PLUGIN_VERSION       ((DWORD) 0x00020005)
#define MERGEFS_ERROR_INVALID_PARAMETER                 ((DWORD) 0x00030000)
#define MERGEFS_ERROR_INVALID_PLUGIN_ID                 ((DWORD) 0x00030001)
#define MERGEFS_ERROR_INVALID_SOURCECONTEXT_ID          ((DWORD) 0x00030002)
#define MERGEFS_ERROR_INVALID_MOUNT_ID                  ((DWORD) 0x00030003)
#define MERGEFS_ERROR_INVALID_FILECONTEXT_ID            ((DWORD) 0x00030004)
#define MERGEFS_ERROR_INVALID_MOUNTPOINT                ((DWORD) 0x00030005)
#define MERGEFS_ERROR_INEXISTENT_FILE                   ((DWORD) 0x00040001)
#define MERGEFS_ERROR_INEXISTENT_PLUGIN                 ((DWORD) 0x00040002)
#define MERGEFS_ERROR_INEXISTENT_SOURCE                 ((DWORD) 0x00040003)
#define MERGEFS_ERROR_INEXISTENT_MOUNT                  ((DWORD) 0x00040004)
#define MERGEFS_ERROR_ALREADY_EXISTING_FILE             ((DWORD) 0x00050001)
#define MERGEFS_ERROR_ALREADY_EXISTING_PLUGIN           ((DWORD) 0x00050002)
#define MERGEFS_ERROR_ALREADY_EXISTING_SOURCE           ((DWORD) 0x00050003)
#define MERGEFS_ERROR_ALREADY_EXISTING_MOUNT            ((DWORD) 0x00050004)
#define MERGEFS_ERROR_ALREADY_EXISTING_MOUNTPOINT       ((DWORD) 0x00050005)


# ifdef __cplusplus
#  define MFEXTERNC extern "C"
#  define MFNOEXCEPT noexcept
# else
#  define MFEXTERNC
#  define MFNOEXCEPT
# endif


#ifdef FROMLIBMERGEFS
# define MFPEXPORT
# define MFCIMPORT
#else
# define MFPEXPORT __declspec(dllexport)
# define MFCIMPORT __declspec(dllimport)
#endif


typedef DWORD MOUNT_ID;
typedef DWORD PLUGIN_ID;

#ifdef __cplusplus
constexpr MOUNT_ID MOUNT_ID_NULL = 0;
constexpr PLUGIN_ID PLUGIN_ID_NULL = 0;
#else
# define MOUNT_ID_NULL ((MOUNT_ID)0)
# define PLUGIN_ID_NULL ((PLUGIN_ID)0)
#endif


typedef DWORD tagPLUGIN_TYPE;

#ifdef __cplusplus
enum class PLUGIN_TYPE : tagPLUGIN_TYPE {
  Source = MERGEFS_PLUGIN_TYPE_SOURCE,
};
#else
typedef tagPLUGIN_TYPE PLUGIN_TYPE;
#endif


typedef void(WINAPI *PMountCallback)(int dokanMainResult) MFNOEXCEPT;


#pragma pack(push, 1)


typedef struct {
  DWORD interfaceVersion;
  PLUGIN_TYPE pluginType;
  GUID guid;
  LPCWSTR name;
  LPCWSTR description;
  DWORD version;
  LPCWSTR versionString;
} PLUGIN_INFO;


typedef struct {
  LPCWSTR filename;
  PLUGIN_INFO pluginInfo;
} PLUGIN_INFO_EX;


// TODO
typedef struct {
  
} MOUNT_INFO;


typedef struct {
  LPCWSTR mountSource;
  GUID sourcePluginGUID;          // set {00000000-0000-0000-0000-000000000000} if unused
  LPCWSTR sourcePluginFilename;   // set nullptr or "" if unused
} MOUNT_SOURCE_INITIALIZE_INFO;


typedef struct {
  LPCWSTR mountPoint;
  BOOL writable;
  LPCWSTR metadataFileName;
  BOOL deferCopyEnabled;
  BOOL caseSensitive;
  DWORD numSources;
  MOUNT_SOURCE_INITIALIZE_INFO* sources;
} MOUNT_INITIALIZE_INFO;


#ifdef FROMLIBMERGEFS
static_assert(sizeof(PLUGIN_INFO) == 3 * 4 + 1 * 16 + 3 * sizeof(void*));
static_assert(sizeof(PLUGIN_INFO_EX) == sizeof(PLUGIN_INFO) + 1 * sizeof(void*));
// TODO: static_assert(sizeof(MOUNT_INFO) == ...);
static_assert(sizeof(MOUNT_SOURCE_INITIALIZE_INFO) == 1 * 16 + 2 * sizeof(void*));
static_assert(sizeof(MOUNT_INITIALIZE_INFO) == 4 * 4 + 3 * sizeof(void*));
#endif


#pragma pack(pop)


#ifndef FROMPLUGIN

#ifdef FROMLIBMERGEFS
namespace Exports {
#endif

MFEXTERNC MFCIMPORT DWORD WINAPI GetError(BOOL* win32error) MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI Init() MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI Uninit() MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI AddSourcePlugin(LPCWSTR filename, BOOL front, PLUGIN_ID* outPluginId) MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI RemoveSourcePlugin(PLUGIN_ID pluginId) MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI GetSourcePlugins(DWORD* outNumPluginIds, PLUGIN_ID* outPluginIds, DWORD maxPluginIds) MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI SetSourcePluginOrder(const PLUGIN_ID* pluginIds, DWORD numPluginIds) MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI GetSourcePluginInfo(PLUGIN_ID pluginId, PLUGIN_INFO_EX* pluginInfoEx) MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI Mount(const MOUNT_INITIALIZE_INFO* mountInitializeInfo, PMountCallback callback, MOUNT_ID* outMountId) MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI GetMounts(DWORD* outNum, PLUGIN_ID* outMountIds, DWORD maxMountIds) MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI GetMountInfo(MOUNT_ID id, MOUNT_INFO* outMountInfo) MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI SafeUnmount(MOUNT_ID mountId) MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI Unmount(MOUNT_ID mountId) MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI SafeUnmountAll() MFNOEXCEPT;
MFEXTERNC MFCIMPORT BOOL WINAPI UnmountAll() MFNOEXCEPT;

#ifdef FROMLIBMERGEFS
}
#endif

#endif
