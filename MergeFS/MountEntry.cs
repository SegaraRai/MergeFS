using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MergeFS
{
  class MountEntry
  {
    private UInt32 Id;
    string Source { get; }
    string MountPoint { get; }

    public MountEntry(string source, string mountPoint)
    {
      Source = source;
      MountPoint = mountPoint;
    }
  }
}
