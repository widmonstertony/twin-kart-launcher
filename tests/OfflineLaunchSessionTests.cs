using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using System.Web.Script.Serialization;
using TwinKartLauncher;

static class OfflineLaunchSessionTests
{
    static void Need(bool valid,string message) { if(!valid)throw new Exception(message); }
    static int Main()
    {
        try { return Run(); }
        catch(Exception error) { Console.Error.WriteLine("FAIL session cleanup regression: "+error.GetType().Name);return 1; }
    }
    static int Run()
    {
        string[] flags={"TWIN_KART_CHASER_COMPAT","TWIN_KART_CHASER_ARENA","TWIN_KART_HIDE_COMPAT","TWIN_KART_HIDE_REPAIRED_SCENE"};
        for(int mask=0;mask<16;++mask)for(int ready=0;ready<2;++ready) {
            var start=new ProcessStartInfo();var selected=new Dictionary<string,string>();
            for(int i=0;i<flags.Length;++i) {start.EnvironmentVariables[flags[i]]="1";selected[flags[i]]=(mask&(1<<i))!=0?"1":"0";}
            OfflineLaunchSession.ConfigurePrototypeBackendEnvironment(start,selected,ready==1);
            for(int i=0;i<flags.Length;++i) {
                int pair=i<2?3:12;bool enabled=ready==1 && (mask&pair)==pair;
                Need((start.EnvironmentVariables[flags[i]]=="1")==enabled,"Prototype backend session gate differs from native bridge");
            }
        }
        Console.WriteLine("PASS prototype backend flags: both required gates, ready session, all combinations and inherited-state cleanup");
        OfflineLaunchSession.StopOwned(null,"unused");
        using(var unstarted=new Process())OfflineLaunchSession.StopOwned(unstarted,"unused");
        using(var exited=Process.Start(new ProcessStartInfo(Environment.GetEnvironmentVariable("ComSpec"),"/c exit 0") {
            UseShellExecute=false,CreateNoWindow=true })) {
            exited.WaitForExit();OfflineLaunchSession.StopOwned(exited,"unused");
        }
        var cleanup=new Dictionary<string,object>();bool second=false;
        Need(!OfflineLaunchSession.CleanupStep(()=>{throw new IOException();},cleanup,"first"),"Cleanup error lost");
        Need(OfflineLaunchSession.CleanupStep(()=>second=true,cleanup,"second") && second && (string)cleanup["first"]=="IOException","Later cleanup skipped");

        // This executable lives in its own test output directory. Its invalid
        // backend deliberately fails before any game or user configuration use.
        string root=AppDomain.CurrentDomain.BaseDirectory;
        string backend=Path.Combine(root,"TwinKartGateway.exe");
        Need(!File.Exists(backend),"Test output must not contain a real backend");
        // Leave room for the session's own timestamp directory on legacy
        // .NET Framework hosts that still enforce MAX_PATH.
        string scratch=Path.Combine(root,"failure-"+Guid.NewGuid().ToString("N").Substring(0,8));
        Directory.CreateDirectory(scratch);
        File.WriteAllText(backend,"invalid test executable");
        try {
            string config=Path.Combine(scratch,"config"),runs=Path.Combine(scratch,"runs");Directory.CreateDirectory(config);
            byte[] original=new byte[]{239,187,191,91,88,93,13,10,65,61,49,13,10};
            File.WriteAllBytes(Path.Combine(config,"Game.ini"),original);
            string fakeGame=Path.Combine(scratch,"NeverLaunch.exe");
            File.WriteAllText(fakeGame,"never run");
            var listener=new TcpListener(IPAddress.Loopback,0);listener.Start();int port=((IPEndPoint)listener.LocalEndpoint).Port;listener.Stop();
            var manifest=new OfflineLaunchManifest { GamePath=fakeGame,GameSha256=ProfileStore.Fingerprint(fakeGame),
                GatewaySha256=ProfileStore.Fingerprint(backend),GatewayConfigPath=Path.Combine(scratch,"unused.json"),
                ConfigDirectory=config,SessionDirectory=runs,Port=port,MaximumSeconds=60,
                ServerSection=new Dictionary<string,string>{{"ServerAddrs","127.0.0.1"},{"PreAuthKey","test"}} };
            var json=new JavaScriptSerializer();string path=Path.Combine(scratch,"manifest.json");File.WriteAllText(path,json.Serialize(manifest));
            Exception originalFailure=null;string identity=null;
            try { OfflineLaunchSession.Run(path,message=>{if(identity==null)identity=message;},CancellationToken.None); }
            catch(System.ComponentModel.Win32Exception error) { originalFailure=error; }
            Need(originalFailure!=null,"Original Start Win32 error was masked");
            Need(identity!=null && identity.Contains(runs),"Session identity was not emitted");
            Need(!File.Exists(Path.Combine(runs,"pending-session.json")),"Unchanged configuration left pending");
            Need(File.ReadAllBytes(Path.Combine(config,"Game.ini")).SequenceEqual(original),"Configuration changed");
            string run=Directory.GetDirectories(runs).Single();
            var report=json.Deserialize<Dictionary<string,object>>(File.ReadAllText(Path.Combine(run,"report.json")));
            Need((bool)report["restored"] && (string)report["failure_stage"]=="backend_start","Failure report or recovery missing");
            Need((string)report["failure_type"]=="Win32Exception" && report.ContainsKey("failure_native_error_code") &&
                (int)report["failure_native_error_code"]==((System.ComponentModel.Win32Exception)originalFailure).NativeErrorCode,"Native Start error missing");
            Need(!report.ContainsKey("game_pid") && !report.ContainsKey("server_pid"),"Unexpected process launched");
            Console.WriteLine("PASS session failure keeps original native error, identifies run, restores config and clears pending without game start");
            return 0;
        } finally { File.Delete(backend); }
    }
}
