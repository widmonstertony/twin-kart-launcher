using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
using TwinKartLauncher;
static class OwnedGameStartupTests
{
    static void Need(bool value,string message){if(!value)throw new Exception(message);}
    public static int Main(string[] args){string directory=Path.Combine(Path.GetTempPath(),"TwinKartStartupFixture-"+Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(directory);
        try{
            Need(args.Length==2,"Expected original fixture EXE and DLL.");
            string marker=Path.Combine(directory,"primary.txt"),readyName="Local\\TwinKartStartupFixture-"+Guid.NewGuid().ToString("N");
            using(var ready=new EventWaitHandle(false,EventResetMode.ManualReset,readyName)) {
                var info=new ProcessStartInfo(Path.GetFullPath(args[0])) {UseShellExecute=false,CreateNoWindow=true,WorkingDirectory=directory};
                info.EnvironmentVariables["TWIN_KART_TEST_READY_EVENT"]=readyName;info.EnvironmentVariables["TWIN_KART_TEST_MAIN_MARKER"]=marker;
                using(var startup=OwnedGameStartup.Create(info)) {
                    Need(!File.Exists(marker),"Primary thread ran before bootstrap.");
                    startup.Load(args[1],ProfileStore.Fingerprint(args[1]));
                    Need(ready.WaitOne(5000),"Original fixture module did not signal readiness.");
                    Need(!File.Exists(marker),"Primary thread ran while initialization was pending.");
                    startup.Resume();Need(startup.Process.WaitForExit(10000) && startup.ExitCode==0,"Original fixture primary did not complete.");
                    Need(File.ReadAllText(marker)=="primary-resumed","Wrong primary-thread marker.");
                    startup.Process.Dispose();
                }
                File.Delete(marker);
                using(var startup=OwnedGameStartup.Create(info)) {
                    bool rejected=false;try{startup.Load(args[1],new string('0',64));}catch(InvalidDataException){rejected=true;}
                    Need(rejected,"Mismatched module fingerprint accepted.");
                }
                Need(!File.Exists(marker),"Failed bootstrap resumed its primary thread.");
            }
            Console.WriteLine("PASS owned startup: actual original fixture DLL readiness before primary entry, exact-hash rejection, resume and abort. No game or vendor runtime started.");return 0;
        }catch(Exception e){Console.Error.WriteLine(e.ToString());return 1;}
        finally{if(File.Exists(Path.Combine(directory,"primary.txt")))File.Delete(Path.Combine(directory,"primary.txt"));Directory.Delete(directory);}
    }
}
