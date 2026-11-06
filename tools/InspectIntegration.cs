using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Web.Script.Serialization;
using TwinKartLauncher;

// Read-only setup inspection, apart from a temporary track-query fixture.
// It never starts a process, loads a game module, creates a device or connects
// to the backend. Passing this check does not establish gameplay/GPU results.
static class InspectIntegration
{
    static void Need(bool value,string message){if(!value)throw new InvalidDataException(message);}
    static bool Flag(OfflineLaunchManifest m,string key){string value;return m.Environment!=null && m.Environment.TryGetValue(key,out value) && value=="1";}
    static bool Within(string file,string root){return Path.GetFullPath(file).StartsWith(root+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase);}
    static int Main(string[] args)
    {
        if(args.Length!=1){Console.Error.WriteLine("Usage: TwinKartInspectIntegration.exe <normal private manifest>");return 2;}
        string query=null;
        try {
            string path=Path.GetFullPath(args[0]),root=Path.GetDirectoryName(path);
            var json=new JavaScriptSerializer {MaxJsonLength=16*1024*1024};
            var manifest=json.Deserialize<OfflineLaunchManifest>(File.ReadAllText(path,new UTF8Encoding(false,true)));
            Need(manifest!=null && manifest.MaximumSeconds==0 && manifest.EnableExperimentalRaceHost,"Expected a normal user-controlled room session.");
            Need(manifest.GameSha256=="5041d9f5d45adb4b1c932d8056a89f70db7fa98f93614ac30685cd1adeb9d9c2" &&
                ProfileStore.Fingerprint(manifest.GamePath)==manifest.GameSha256,"Original client build differs.");
            Need(ProfileStore.Fingerprint(Path.Combine(root,"TwinKartGateway.exe"))==manifest.GatewaySha256,"Candidate backend pin differs.");
            Need(manifest.Modules!=null && manifest.Modules.Length==4,"Expected the combined four-module build.");
            string[] names={"TwinKartDriftCamera.dll","TwinKartDriftRaceBridge.dll","TwinKartDriftRenderProbe.dll","TwinKartDriftAssetInventory.dll"};
            foreach(string name in names) {
                var matches=manifest.Modules.Where(m=>m!=null && String.Equals(Path.GetFileName(m.Path),name,StringComparison.OrdinalIgnoreCase)).ToArray();
                Need(matches.Length==1 && Within(matches[0].Path,root) && ProfileStore.Fingerprint(matches[0].Path)==matches[0].Sha256,"Combined native module pin differs.");
            }
            foreach(var row in manifest.ClientFiles??new LaunchModule[0])Need(row!=null && ProfileStore.Fingerprint(row.Path)==row.Sha256,"Pinned external dependency differs.");
            Need(manifest.Graphics!=null && manifest.Graphics.NeedsRenderer,"Combined graphics configuration missing.");
            manifest.Graphics.ValidateFiles(manifest.Environment);
            Need(manifest.Graphics.NeuralPasses==2,"Expected the retained two-pass option.");
            string[] flags={"TWIN_KART_EVENT_MODES","TWIN_KART_FACTORY_COMPAT","TWIN_KART_CHASER_COMPAT","TWIN_KART_CHASER_ARENA","TWIN_KART_HIDE_COMPAT","TWIN_KART_HIDE_REPAIRED_SCENE"};
            foreach(string flag in flags)Need(Flag(manifest,flag),"Normal prototype mode gate missing.");
            foreach(string key in new[]{"TWIN_KART_HIDE_DATA_CAPTURE","TWIN_KART_HIDE_BOT_CAPTURE","TWIN_KART_HIDE_INPUT_CAPTURE","TWIN_KART_HIDE_ARENA_CAPTURE","TWIN_KART_CAPTURE_HALL"})
                Need(!Flag(manifest,key),"Diagnostic capture selected in normal integration.");
            var backend=new ProcessStartInfo();OfflineLaunchSession.ConfigurePrototypeBackendEnvironment(backend,manifest.Environment,true);
            foreach(string key in flags.Skip(2))Need(backend.EnvironmentVariables[key]=="1","Session does not forward both prototype gate pairs.");
            Need(Within(manifest.GatewayConfigPath,root) && Within(manifest.SessionDirectory,root),"Candidate config/session isolation missing.");
            var gateway=json.Deserialize<Dictionary<string,object>>(File.ReadAllText(manifest.GatewayConfigPath));
            object raw;Need(gateway.TryGetValue("ProfilePath",out raw) && raw is string && Within((string)raw,root) && File.Exists((string)raw),"Candidate profile isolation missing.");
            query=Path.Combine(root,"preflight-tracks-"+Guid.NewGuid().ToString("N")+".tmp");
            NativeTrackSessionCatalog.PrepareQueries(manifest.TrackReportPath,manifest.GameSha256,query);
            int queries=File.ReadAllLines(query).Length;Need(queries>0,"Native catalog has no query identities.");
            Console.WriteLine(json.Serialize(new {setupChecksPassed=true,normalModes=new[]{100,108,109},nativeModules=names.Length,
                trackQueryIdentities=queries,superResolutionRequested=manifest.Graphics.SuperResolution,frameGenerationRequested=manifest.Graphics.FrameGeneration,
                neuralRenderingRequested=manifest.Graphics.NeuralRendering,neuralPasses=manifest.Graphics.NeuralPasses,
                gameStarted=false,modulesExecuted=false,gameplayAccepted=false,graphicsAccepted=false}));
            return 0;
        } catch(Exception error){Console.Error.WriteLine("FAIL combined setup: "+error.GetType().Name+": "+error.Message);return 1;}
        finally {if(query!=null && File.Exists(query))File.Delete(query);}
    }
}
