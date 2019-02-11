using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace LibMergeFS
{
  using PluginType = UInt32;

  using PluginId = UInt32;
  using MountId = UInt32;
  using Error = UInt32;

  [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
  public struct PluginInfo
  {
    public uint interfaceVersion;
    public PluginType pluginType;
    public Guid guid;
    [MarshalAs(UnmanagedType.LPWStr)]
    public string name;
    [MarshalAs(UnmanagedType.LPWStr)]
    public string description;
    public uint version;
    [MarshalAs(UnmanagedType.LPWStr)]
    public string versionString;
  }

  [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
  public struct PluginInfoEx
  {
    [MarshalAs(UnmanagedType.LPWStr)]
    public string filename;
    public PluginInfo pluginInfo;
  }

  [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
  public struct MountInfo
  {
    // TODO
  }

  [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
  public struct MountSourceInitializeInfo
  {
    [MarshalAs(UnmanagedType.LPWStr)]
    public string mountSource;
    [MarshalAs(UnmanagedType.LPWStr)]
    public Guid sourcePluginGuid;
    [MarshalAs(UnmanagedType.LPWStr)]
    public string sourcePluginFilename;
  }

  [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
  public struct MountInitializeInfo
  {
    [MarshalAs(UnmanagedType.LPWStr)]
    public string mountPoint;
    public bool writable;
    [MarshalAs(UnmanagedType.LPWStr)]
    public string metadataFilename;
    public bool deferCopyEnabled;
    public uint numSources;
    [MarshalAs(UnmanagedType.LPArray)]
    public MountSourceInitializeInfo[] sources;
  }

  public class LibMergeFS
  {
    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    public delegate void MountCallback();


    [DllImport("LibMergeFS.dll")]
    public static extern Error GetLastError([Out] bool win32error);

    [DllImport("LibMergeFS.dll")]
    public static extern bool Init();

    [DllImport("LibMergeFS.dll")]
    public static extern bool Uninit();

    [DllImport("LibMergeFS.dll", CharSet=CharSet.Unicode)]
    public static extern bool AddSourcePlugin(string filename, bool front, [Out] PluginId pluginId);

    [DllImport("LibMergeFS.dll")]
    public static extern bool RemoveSourcePlugin(PluginId pluginId);

    [DllImport("LibMergeFS.dll")]
    public static extern bool GetSourcePlugins([In] MountInitializeInfo mountInitializeInfo, MountCallback mountCallback, [Out] MountId mountId);

    [DllImport("LibMergeFS.dll")]
    public static extern bool GetSourcePluginInfo(PluginId pluginId, [Out] PluginInfoEx pluginInfoEx, [Out] IntPtr pluginPath);

    [DllImport("LibMergeFS.dll")]
    public static extern bool GetMounts([Out] uint numMounts, [Out] MountId[] mountIds, uint maxMountIds);

    [DllImport("LibMergeFS.dll")]
    public static extern bool GetMountInfo(MountId mountId, [Out] MountInfo mountInfo);

    [DllImport("LibMergeFS.dll")]
    public static extern bool Unmount(MountId mountId);

    [DllImport("LibMergeFS.dll")]
    public static extern bool UnmountAll();
  }
}
