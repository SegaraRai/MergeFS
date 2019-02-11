using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;

namespace MergeFS
{
  /// <summary>
  /// App.xaml の相互作用ロジック
  /// </summary>
  public partial class App : Application
  {
    static string MutexName = "MergeFS_SIM";

    [STAThread]
    public static void Main()
    {
      bool mutexCreated;
      var mutex = new Mutex(true, MutexName, out mutexCreated);

      if (!mutexCreated)
      {
        mutex.Close();
        return;
      }

      try
      {
        if (!LibMergeFS.LibMergeFS.Init())
        {
          MessageBox.Show("Cannot initialize mergefs");
          throw new Exception();
        }

        App app = new App();
        app.InitializeComponent();
        app.Run();
      }
      finally
      {
        mutex.ReleaseMutex();
        mutex.Close();

        LibMergeFS.LibMergeFS.UnmountAll();
        LibMergeFS.LibMergeFS.Uninit();
      }
    }
  }
}
