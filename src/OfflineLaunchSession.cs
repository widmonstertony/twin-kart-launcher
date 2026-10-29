using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Runtime.InteropServices;
using System.Web.Script.Serialization;

namespace TwinKartLauncher
{
    public sealed class LaunchModule
    { public string Path { get; set; } public string Sha256 { get; set; } }
    public sealed class OfflineLaunchManifest
    {
        public string GamePath { get; set; }
        public string GameSha256 { get; set; }
        public string GameArguments { get; set; }
        public string GatewayConfigPath { get; set; }
        public string GatewaySha256 { get; set; }
        public string ConfigDirectory { get; set; }
        public string SessionDirectory { get; set; }
        public int Port { get; set; }
        public int MaximumSeconds { get; set; }
        public Dictionary<string,string> ServerSection { get; set; }
        public Dictionary<string,string> Environment { get; set; }
        public LaunchModule[] Modules { get; set; }
        public LaunchModule[] ClientFiles { get; set; }
        public string ExternalSdkProvenance { get; set; }
        public bool EnableExperimentalRaceHost { get; set; }
        public string TrackReportPath { get; set; }
        public OfflineGraphicsOptions Graphics { get; set; }
    }
    public static class OfflineLaunchSession
    {
        [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool QueryFullProcessImageName(IntPtr process,uint flags,StringBuilder path,ref uint length);
        static readonly JavaScriptSerializer Json=new JavaScriptSerializer { MaxJsonLength=1024*1024 };
        static readonly object cameraGate=new object();
        static string activeCameraControl;
        static string activeRaceControl;
        public static bool TryStartRace(LocalRaceSettings settings)
        {
            if(settings==null || !settings.IsValid) throw new ArgumentException("Invalid race settings.");
            lock(cameraGate) { if(activeRaceControl==null) return false; settings.WriteControl(activeRaceControl); return true; }
        }
        public static bool TryApplyCockpit(CockpitSettings settings)
        {
            if(settings==null || !settings.IsValid) throw new ArgumentException("Invalid cockpit settings.");
            lock(cameraGate) { if(activeCameraControl==null) return false; settings.WriteNativeControl(activeCameraControl); return true; }
        }
        static string Quote(string path)
        { if(path.IndexOf('"')>=0 || path.EndsWith("\\",StringComparison.Ordinal)) throw new ArgumentException("Invalid process path."); return "\""+path+"\""; }
        static void Save(string path, object value)
        { string tmp=path+".new"; File.WriteAllText(tmp,Json.Serialize(value),new UTF8Encoding(false)); if(File.Exists(path)) File.Replace(tmp,path,null); else File.Move(tmp,path); }
        internal static byte[] Route(byte[] original, IDictionary<string,string> values)
        {
            string text=new UTF8Encoding(false,true).GetString(original??new byte[0]).TrimStart('\ufeff');
            string section="/Script/KartDrift.KdServerConfig";
            var output=new List<string>(); bool inside=false,found=false;
            foreach(string line in text.Replace("\r\n","\n").Split('\n'))
            {
                string value=line.Trim();
                if(value.StartsWith("[",StringComparison.Ordinal) && value.EndsWith("]",StringComparison.Ordinal))
                {
                    if(inside) foreach(var entry in values) output.Add(entry.Key+"="+entry.Value);
                    inside=String.Equals(value,"["+section+"]",StringComparison.OrdinalIgnoreCase);
                    if(inside) { if(found) throw new InvalidDataException("Duplicate server configuration section."); found=true; }
                }
                if(inside && value.Contains("="))
                {
                    string key=value.Substring(0,value.IndexOf('=')).Trim().TrimStart('+','-','!','.');
                    if(values.Keys.Any(x=>String.Equals(x,key,StringComparison.OrdinalIgnoreCase))) continue;
                }
                output.Add(line);
            }
            if(!found) output.Add("["+section+"]");
            if(inside || !found) foreach(var entry in values) output.Add(entry.Key+"="+entry.Value);
            return new UTF8Encoding(true).GetPreamble().Concat(Encoding.UTF8.GetBytes(String.Join("\r\n",output))).ToArray();
        }
        static void StopOwned(Process process, string path)
        {
            if(process==null || process.HasExited) return;
            // This also works before primary-thread loader initialization,
            // when a suspended process may have no PEB module enumeration yet.
            var image=new StringBuilder(32768);uint length=(uint)image.Capacity;
            if(!QueryFullProcessImageName(process.Handle,0,image,ref length) || !String.Equals(image.ToString(),path,StringComparison.OrdinalIgnoreCase)) throw new InvalidOperationException("Refusing to stop a different process.");
            process.Kill(); if(!process.WaitForExit(10000)) throw new TimeoutException("Owned process did not stop.");
        }
        public static string Run(string manifestPath, Action<string> progress, CancellationToken cancellation, CockpitSettings cockpit=null)
        {
            var manifest=Json.Deserialize<OfflineLaunchManifest>(File.ReadAllText(manifestPath));
            string backend=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"TwinKartGateway.exe");
            string game=Path.GetFullPath(manifest.GamePath), config=Path.GetFullPath(manifest.ConfigDirectory);
            if(manifest.MaximumSeconds<60 || manifest.MaximumSeconds>3600 || manifest.Port<1024 || manifest.Port>65535) throw new ArgumentException("Invalid run limits.");
            if(ProfileStore.Fingerprint(game)!=manifest.GameSha256 || ProfileStore.Fingerprint(backend)!=manifest.GatewaySha256) throw new InvalidDataException("Game/backend build differs from manifest.");
            if(manifest.ClientFiles!=null) foreach(var companion in manifest.ClientFiles)
                if(ProfileStore.Fingerprint(companion.Path)!=companion.Sha256) throw new InvalidDataException("Client dependency differs from manifest.");
            if(manifest.Modules!=null)foreach(var module in manifest.Modules)
                if(module==null || ProfileStore.Fingerprint(module.Path)!=module.Sha256)throw new InvalidDataException("Original module differs from manifest.");
            if(manifest.Graphics!=null)manifest.Graphics.ValidateFiles(manifest.Environment);
            if(manifest.Graphics!=null && manifest.Graphics.NeedsRenderer && (manifest.Modules==null || !manifest.Modules.Any(m=>String.Equals(Path.GetFileName(m.Path),"TwinKartDriftRenderProbe.dll",StringComparison.OrdinalIgnoreCase))))
                throw new InvalidDataException("Requested graphics integration requires the original render adapter module.");
            if(manifest.Graphics!=null && manifest.Graphics.FrameGeneration && (manifest.Modules==null || !manifest.Modules.Any(m=>String.Equals(Path.GetFileName(m.Path),"TwinKartDriftRaceBridge.dll",StringComparison.OrdinalIgnoreCase))))
                throw new InvalidDataException("Frame generation requires the original race-state bridge for menu/loading/pause gating.");
            if(!Directory.Exists(config)) throw new DirectoryNotFoundException("Game config directory unavailable.");
            if(manifest.ServerSection==null || !manifest.ServerSection.ContainsKey("ServerAddrs") || !manifest.ServerSection.ContainsKey("PreAuthKey")) throw new ArgumentException("External server route configuration required.");
            foreach(var pair in manifest.ServerSection)
                if(pair.Key.IndexOfAny(new[]{'\r','\n','=','[',']'})>=0 || pair.Value.IndexOfAny(new[]{'\r','\n','\0'})>=0) throw new ArgumentException("Invalid INI override.");
            foreach(var other in Process.GetProcessesByName(Path.GetFileNameWithoutExtension(game)))
                using(other) { if(!other.HasExited) throw new InvalidOperationException("Exit the running game before starting an owned session."); }
            using(var occupied=new TcpClient())
            {
                try { occupied.Connect("127.0.0.1",manifest.Port); }
                catch(SocketException) { }
                if(occupied.Connected) throw new InvalidOperationException("Backend port already occupied.");
            }
            string root=Path.GetFullPath(manifest.SessionDirectory);
            Directory.CreateDirectory(root);
            using(var exclusive=new FileStream(Path.Combine(root,"session.lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None))
            {
                string pending=Path.Combine(root,"pending-session.json");
                if(File.Exists(pending)) throw new InvalidOperationException("A previous session needs configuration recovery.");
                string run=Path.Combine(root,DateTime.UtcNow.ToString("yyyyMMdd-HHmmss")+"-"+Guid.NewGuid().ToString("N").Substring(0,6));
                string before=Path.Combine(run,"config-before"),after=Path.Combine(run,"config-after");
                Directory.CreateDirectory(before); Directory.CreateDirectory(after);
                string cameraControl=Path.Combine(run,"cockpit.control");
                string raceControl=Path.Combine(run,"race.control");
                if(cockpit==null) { string flag; cockpit=new CockpitSettings { Enabled=manifest.Environment!=null && manifest.Environment.TryGetValue("TWIN_KART_FIRST_PERSON",out flag) && flag=="1" }; }
                cockpit.Copy().WriteNativeControl(cameraControl);
                var hashes=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
                foreach(string file in Directory.GetFiles(config,"*.ini")) { string name=Path.GetFileName(file); File.Copy(file,Path.Combine(before,name)); hashes.Add(name,ProfileStore.Fingerprint(file)); }
                var report=new Dictionary<string,object> { {"original_backend",true},{"original_launcher",true},{"external_sdk_provenance",manifest.ExternalSdkProvenance??"unspecified"},{"restored",false},{"started_utc",DateTime.UtcNow.ToString("o")},{"game_sha256",manifest.GameSha256},{"backend_sha256",manifest.GatewaySha256} };
                Save(pending,new { ConfigDirectory=config,RunDirectory=run,Before=hashes });
                Process server=null,gameProcess=null;
                try
                {
                    var ready=new ManualResetEvent(false);
                    using(var serverLog=new StreamWriter(Path.Combine(run,"backend.jsonl"),false,new UTF8Encoding(false)))
                    {
                        object logGate=new object();
                        var start=new ProcessStartInfo(backend,"--offline "+Quote(Path.GetFullPath(manifest.GatewayConfigPath))) { UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true,WorkingDirectory=AppDomain.CurrentDomain.BaseDirectory };
                        server=new Process { StartInfo=start };
                        DataReceivedEventHandler capture=(sender,args)=>{ if(args.Data==null) return; try { lock(logGate){serverLog.WriteLine(args.Data);serverLog.Flush();} } catch(ObjectDisposedException) { } if(args.Data.Contains("\"event\":\"ready\"")) ready.Set(); };
                        server.OutputDataReceived+=capture; server.ErrorDataReceived+=capture;
                        if(!server.Start()) throw new InvalidOperationException("Backend process did not start.");
                        server.BeginOutputReadLine(); server.BeginErrorReadLine(); report["server_pid"]=server.Id;
                        if(!ready.WaitOne(15000) || server.HasExited) throw new InvalidOperationException("Original backend did not become ready; inspect session log.");
                        cancellation.ThrowIfCancellationRequested();
                        string gameIni=Path.Combine(config,"Game.ini");
                        File.WriteAllBytes(gameIni,Route(File.Exists(gameIni)?File.ReadAllBytes(gameIni):null,manifest.ServerSection));
                        if(manifest.Graphics!=null && (manifest.Graphics.SuperResolution || manifest.Graphics.FrameGeneration)) {
                            string userIni=Path.Combine(config,"GameUserSettings.ini");
                            File.WriteAllBytes(userIni,manifest.Graphics.RouteUserSettings(File.Exists(userIni)?File.ReadAllBytes(userIni):null));
                        }
                        var launch=new ProcessStartInfo(game,manifest.GameArguments??"") { UseShellExecute=false,WorkingDirectory=Path.GetDirectoryName(game) };
                        if(manifest.Environment!=null) foreach(var variable in manifest.Environment) launch.EnvironmentVariables[variable.Key]=variable.Value;
                        if(manifest.Graphics!=null) {
                            var graphicsEnvironment=new Dictionary<string,string>();
                            foreach(string key in launch.EnvironmentVariables.Keys)graphicsEnvironment[key]=launch.EnvironmentVariables[key];
                            manifest.Graphics.ApplyEnvironment(graphicsEnvironment,run);
                            foreach(var variable in graphicsEnvironment)launch.EnvironmentVariables[variable.Key]=variable.Value;
                        }
                        launch.EnvironmentVariables["TWIN_KART_ALLOWED_GAME"]=game;
                        launch.EnvironmentVariables["TWIN_KART_CAMERA_LOG"]=Path.Combine(run,"camera.jsonl");
                        launch.EnvironmentVariables["TWIN_KART_CAMERA_CONTROL"]=cameraControl;
                        launch.EnvironmentVariables["TWIN_KART_ASSET_LOG"]=Path.Combine(run,"assets.jsonl");
                        launch.EnvironmentVariables["TWIN_KART_RACE_BRIDGE_LOG"]=Path.Combine(run,"race-bridge.jsonl");
                        if(manifest.EnableExperimentalRaceHost) launch.EnvironmentVariables["TWIN_KART_RACE_CONTROL"]=raceControl;
                        launch.EnvironmentVariables["TWIN_KART_COCKPIT_HEAD"]="1";
                        launch.EnvironmentVariables["TWIN_KART_RENDER_LOG"]=Path.Combine(run,"render.jsonl");
                        launch.EnvironmentVariables["TWIN_KART_RENDER_PASSES"]="1";
                        launch.EnvironmentVariables["TWIN_KART_DLSS_CACHE"]=Path.Combine(run,"ngx-cache"); Directory.CreateDirectory(Path.Combine(run,"ngx-cache"));
                        if(manifest.Graphics!=null && manifest.Graphics.NeedsRenderer) {
                            string readyName="Local\\TwinKartRenderReady-"+Guid.NewGuid().ToString("N");
                            using(var graphicsReady=new EventWaitHandle(false,EventResetMode.ManualReset,readyName)) {
                                launch.EnvironmentVariables["TWIN_KART_RENDER_READY_EVENT"]=readyName;
                                using(var startup=OwnedGameStartup.Create(launch)) {
                                    gameProcess=startup.Process;report["game_pid"]=gameProcess.Id;Save(Path.Combine(run,"report.json"),report);
                                    if(manifest.Modules!=null)foreach(var module in manifest.Modules)startup.Load(module.Path,module.Sha256);
                                    if(!graphicsReady.WaitOne(15000))throw new TimeoutException("Graphics hooks were not ready before client startup; the owned process was not resumed.");
                                    cancellation.ThrowIfCancellationRequested();startup.Resume();
                                    report["graphics_hooks_before_primary_thread"]=true;
                                }
                            }
                        } else {
                            gameProcess=Process.Start(launch); report["game_pid"]=gameProcess.Id;
                            Save(Path.Combine(run,"report.json"),report);
                            if(manifest.Modules!=null) foreach(var module in manifest.Modules) OwnedGameModule.Load(gameProcess,game,module.Path,module.Sha256);
                        }
                        if(manifest.Modules!=null && manifest.Modules.Any(module=>String.Equals(Path.GetFileName(module.Path),"TwinKartDriftCamera.dll",StringComparison.OrdinalIgnoreCase)))
                            lock(cameraGate) activeCameraControl=cameraControl;
                        if(manifest.EnableExperimentalRaceHost && manifest.Modules!=null && manifest.Modules.Any(module=>String.Equals(Path.GetFileName(module.Path),"TwinKartDriftRaceBridge.dll",StringComparison.OrdinalIgnoreCase)))
                            lock(cameraGate) activeRaceControl=raceControl;
                        progress("原创启动器已启动原创后端和原厂游戏；会话："+run);
                        var timer=Stopwatch.StartNew();
                        while(!gameProcess.WaitForExit(250))
                        { cancellation.ThrowIfCancellationRequested(); if(server.HasExited) throw new InvalidOperationException("Original backend exited during game session."); if(timer.Elapsed.TotalSeconds>=manifest.MaximumSeconds) break; }
                        report["elapsed_seconds"]=timer.Elapsed.TotalSeconds;
                        StopOwned(gameProcess,game); StopOwned(server,backend); server.WaitForExit();
                    }
                }
                catch(Exception problem) { report["failure_type"]=problem.GetType().Name; throw; }
                finally
                {
                    lock(cameraGate) { if(activeCameraControl==cameraControl) activeCameraControl=null; }
                    lock(cameraGate) { if(activeRaceControl==raceControl) activeRaceControl=null; }
                    StopOwned(gameProcess,game); StopOwned(server,backend);
                    foreach(string file in Directory.GetFiles(config,"*.ini"))
                    { string name=Path.GetFileName(file); File.Copy(file,Path.Combine(after,name),true); if(!hashes.ContainsKey(name)) File.Delete(file); }
                    foreach(string name in hashes.Keys) File.Copy(Path.Combine(before,name),Path.Combine(config,name),true);
                    bool restored=hashes.All(pair=>ProfileStore.Fingerprint(Path.Combine(config,pair.Key))==pair.Value);
                    report["restored"]=restored; report["ended_utc"]=DateTime.UtcNow.ToString("o");
                    Save(Path.Combine(run,"report.json"),report);
                    if(restored) File.Delete(pending);
                    if(gameProcess!=null) gameProcess.Dispose(); if(server!=null) server.Dispose();
                    progress(restored?"本轮测试结束，配置已完整恢复。":"配置恢复失败，请保留会话日志。");
                }
                return run;
            }
        }
    }
}
