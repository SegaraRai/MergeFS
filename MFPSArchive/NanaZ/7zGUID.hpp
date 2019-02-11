#pragma once

/*
#include <7z/CPP/7zip/ICoder.h>
#include <7z/CPP/7zip/IDecl.h>
#include <7z/CPP/7zip/IPassword.h>
#include <7z/CPP/7zip/IProgress.h>
#include <7z/CPP/7zip/IStream.h>
#include <7z/CPP/7zip/Archive/IArchive.h>
#include <7z/CPP/7zip/UI/Agent/Agent.h>
#include <7z/CPP/7zip/UI/Agent/IFolderArchive.h>
#include <7z/CPP/7zip/UI/Common/IFileExtractCallback.h>
#include <7z/CPP/7zip/UI/FileManager/PluginInterface.h>
//*/


// 00 IProgress.h

struct __declspec(uuid("{23170F69-40C1-278A-0000-000000050000}")) IProgress;
//struct __declspec(uuid("{23170F69-40C1-278A-0000-000000050002}")) IProgress2;


// 01 IFolderArchive.h

//struct __declspec(uuid("{23170F69-40C1-278A-0000-000100050000}")) IArchiveFolder;     // old
//struct __declspec(uuid("{23170F69-40C1-278A-0000-000100060000}")) IInFolderArchive;   // old
struct __declspec(uuid("{23170F69-40C1-278A-0000-000100070000}")) IFolderArchiveExtractCallback;    // IFileExtractCallback.h
struct __declspec(uuid("{23170F69-40C1-278A-0000-000100080000}")) IFolderArchiveExtractCallback2;   // IFileExtractCallback.h
//struct __declspec(uuid("{23170F69-40C1-278A-0000-0001000A0000}")) IOutFolderArchive;
struct __declspec(uuid("{23170F69-40C1-278A-0000-0001000B0000}")) IFolderArchiveUpdateCallback;
struct __declspec(uuid("{23170F69-40C1-278A-0000-0001000C0000}")) IArchiveFolderInternal;           // Agent.h
struct __declspec(uuid("{23170F69-40C1-278A-0000-0001000D0000}")) IArchiveFolder;
struct __declspec(uuid("{23170F69-40C1-278A-0000-0001000E0000}")) IInFolderArchive;
struct __declspec(uuid("{23170F69-40C1-278A-0000-0001000F0000}")) IOutFolderArchive;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000100100000}")) IFolderArchiveUpdateCallback2;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000100110000}")) IFolderScanProgress;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000100200000}")) IGetProp;                         // IFileExtractCallback.h
struct __declspec(uuid("{23170F69-40C1-278A-0000-000100300000}")) IFolderExtractToStreamCallback;   // IFileExtractCallback.h


// 03 IStream.h

struct __declspec(uuid("{23170F69-40C1-278A-0000-000300010000}")) ISequentialInStream;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000300020000}")) ISequentialOutStream;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000300030000}")) IInStream;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000300040000}")) IOutStream;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000300060000}")) IStreamGetSize;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000300070000}")) IOutStreamFinish;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000300080000}")) IStreamGetProps;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000300090000}")) IStreamGetProps2;


// 04 ICoder.h

struct __declspec(uuid("{23170F69-40C1-278A-0000-000400040000}")) ICompressProgressInfo;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400050000}")) ICompressCoder;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400180000}")) ICompressCoder2;
struct __declspec(uuid("{23170F69-40C1-278A-0000-0004001F0000}")) ICompressSetCoderPropertiesOpt;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400200000}")) ICompressSetCoderProperties;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400210000}")) ICompressSetDecoderProperties;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400220000}")) ICompressSetDecoderProperties2;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400230000}")) ICompressWriteCoderProperties;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400240000}")) ICompressGetInStreamProcessedSize;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400250000}")) ICompressSetCoderMt;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400260000}")) ICompressSetFinishMode;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400270000}")) ICompressGetInStreamProcessedSize2;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400280000}")) ICompressSetMemLimit;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400300000}")) ICompressGetSubStreamSize;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400310000}")) ICompressSetInStream;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400320000}")) ICompressSetOutStream;
//struct __declspec(uuid("{23170F69-40C1-278A-0000-000400330000}")) ICompressSetInStreamSize;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400340000}")) ICompressSetOutStreamSize;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400350000}")) ICompressSetBufSize;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400360000}")) ICompressInitEncoder;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400370000}")) ICompressSetInStream2;
//struct __declspec(uuid("{23170F69-40C1-278A-0000-000400380000}")) ICompressSetOutStream2;
//struct __declspec(uuid("{23170F69-40C1-278A-0000-000400390000}")) SetInStreamSize2;
//struct __declspec(uuid("{23170F69-40C1-278A-0000-0004003A0000}")) SetOutStreamSize2;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400400000}")) ICompressFilter;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400600000}")) ICompressCodecsInfo;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400610000}")) ISetCompressCodecsInfo;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400800000}")) ICryptoProperties;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400880000}")) ICryptoResetSalt;
struct __declspec(uuid("{23170F69-40C1-278A-0000-0004008C0000}")) ICryptoResetInitVector;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400900000}")) ICryptoSetPassword;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400A00000}")) ICryptoSetCRC;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400C00000}")) IHasher;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000400C10000}")) IHashers;


// 05 IPassword.h

struct __declspec(uuid("{23170F69-40C1-278A-0000-000500100000}")) ICryptoGetTextPassword;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000500110000}")) ICryptoGetTextPassword2;


// 06 IArchive.h

struct __declspec(uuid("{23170F69-40C1-278A-0000-000600030000}")) ISetProperties;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000600040000}")) IArchiveKeepModeForNextOpen;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000600050000}")) IArchiveAllowTail;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000600100000}")) IArchiveOpenCallback;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000600200000}")) IArchiveExtractCallback;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000600210000}")) IArchiveExtractCallbackMessage;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000600300000}")) IArchiveOpenVolumeCallback;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000600400000}")) IInArchiveGetStream;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000600500000}")) IArchiveOpenSetSubArchiveName;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000600600000}")) IInArchive;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000600610000}")) IArchiveOpenSeq;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000600700000}")) IArchiveGetRawProps;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000600710000}")) IArchiveGetRootProps;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000600800000}")) IArchiveUpdateCallback;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000600820000}")) IArchiveUpdateCallback2;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000600830000}")) IArchiveUpdateCallbackFile;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000600A00000}")) IOutArchive;


// 08 IFolder.h

struct __declspec(uuid("{23170F69-40C1-278A-0000-000800000000}")) IFolderFolder;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000800010000}")) IEnumProperties;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000800020000}")) IFolderGetTypeID;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000800030000}")) IFolderGetPath;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000800040000}")) IFolderWasChanged;
//struct __declspec(uuid("{23170F69-40C1-278A-0000-000800050000}")) IFolderReload;
//struct __declspec(uuid("{23170F69-40C1-278A-0000-000800060000}")) IFolderOperations;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000800070000}")) IFolderGetSystemIconIndex;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000800080000}")) IFolderGetItemFullSize;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000800090000}")) IFolderClone;
struct __declspec(uuid("{23170F69-40C1-278A-0000-0008000A0000}")) IFolderSetFlatMode;
struct __declspec(uuid("{23170F69-40C1-278A-0000-0008000B0000}")) IFolderOperationsExtractCallback;
//struct __declspec(uuid("{23170F69-40C1-278A-0000-0008000C0000}")) ...;
//struct __declspec(uuid("{23170F69-40C1-278A-0000-0008000D0000}")) ...;
struct __declspec(uuid("{23170F69-40C1-278A-0000-0008000E0000}")) IFolderProperties;
//struct __declspec(uuid("{23170F69-40C1-278A-0000-0008000F0000}")) ...;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000800100000}")) IFolderArcProps;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000800110000}")) IGetFolderArcProps;
//struct __declspec(uuid("{23170F69-40C1-278A-0000-000800120000}")) IFolderOperations;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000800130000}")) IFolderOperations;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000800140000}")) IFolderCalcItemFullSize;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000800150000}")) IFolderCompare;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000800160000}")) IFolderGetItemName;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000800170000}")) IFolderAltStreams;

// 09 IFolder.h FOLDER_MANAGER_INTERFACE

//struct __declspec(uuid("{23170F69-40C1-278A-0000-000900000000}")) IFolderManager;   // old
//struct __declspec(uuid("{23170F69-40C1-278A-0000-000900010000}")) IFolderManager;   // old
//struct __declspec(uuid("{23170F69-40C1-278A-0000-000900020000}")) IFolderManager;   // old
//struct __declspec(uuid("{23170F69-40C1-278A-0000-000900030000}")) IFolderManager;   // old
//struct __declspec(uuid("{23170F69-40C1-278A-0000-000900040000}")) IFolderManager;   // old
struct __declspec(uuid("{23170F69-40C1-278A-0000-000900050000}")) IFolderManager;


// 0A PluginInterface.h

struct __declspec(uuid("{23170F69-40C1-278A-0000-000A00000000}")) IInitContextMenu;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000A00010000}")) IPluginOptionsCallback;
struct __declspec(uuid("{23170F69-40C1-278A-0000-000A00020000}")) IPluginOptions;
