using System;
using System.IO;
using System.Threading;
using System.Web.Script.Serialization;

namespace TwinKartLauncher.Gateway
{
    static class GatewayHost
    {
        static int Main(string[] args)
        {
            bool offline = args.Length == 2 && args[0] == "--offline";
            bool localAccount = args.Length == 2 && (args[0] == "--local-account" || offline);
            if (args.Length != 1 && !localAccount)
            { Console.Error.WriteLine("Usage: TwinKartGateway.exe [--local-account|--offline] <private.local.json>"); return 2; }
            try
            {
                var file = new FileInfo(args[args.Length - 1]);
                if (!file.Exists || file.Length > 32768) throw new ArgumentException("Invalid config file");
                var settings = new JavaScriptSerializer().Deserialize<LocalAccountSettings>(File.ReadAllText(file.FullName));
                if (settings == null) throw new ArgumentException("Missing config");
                settings.Validate();
                if (localAccount) settings.ValidateLocalAccount();
                var catalog = localAccount && !string.IsNullOrEmpty(settings.DynamicDataPath) ? DynamicDataCatalog.Load(settings.DynamicDataPath) : null;
                using (var ping = offline ? new LocalPingServer() : null)
                {
                OfflineProfileStore profile = null;
                if (offline)
                {
                    var roomLaunch=NativeRoomLaunch.FromEnvironment();
                    string data = settings.ProfileDataDirectory;
                    if (!Directory.Exists(data) || String.IsNullOrEmpty(settings.ProfilePath)) throw new ArgumentException("Explicit external profile/data paths required");
                    profile = new OfflineProfileStore(settings.ProfilePath,
                        ProfileJson.Read(Path.Combine(data, "initial-profile.private.json")),
                        ProfileJson.Read(Path.Combine(data, "client-defaults.private.json")),
                        ProfileJson.Read(Path.Combine(data, "unlock-catalog.private.json")),
                        ProfileJson.Read(Path.Combine(data, "gear-rules.private.json")),
                        ProfileJson.Read(Path.Combine(data, "tracks.private.json")),
                        ProfileJson.Read(Path.Combine(data, "native-parameters.private.json")),
                        ProfileJson.Read(Path.Combine(data, "vehicle-parts.private.json")),
                        settings.LocalAccountId, ping.Port,
                        String.IsNullOrEmpty(settings.TuningPlanPath) ? null : ProfileJson.Read(settings.TuningPlanPath),
                        String.IsNullOrEmpty(settings.VehicleAvailabilityPath) ? null : ProfileJson.Read(settings.VehicleAvailabilityPath),
                        roomLaunch==null?(Func<RoomState,string>)null:roomLaunch.Start);
                }
                Func<Func<string, GatewayReply>> factory = () => {
                    Func<string, GatewayReply> handler = localAccount ?
                        (Func<string, GatewayReply>)new LocalAccountSession(settings, catalog, profile).Handle : new PreAuthSession(settings).Handle;
                    return wire => {
                        GatewayReply reply = handler(wire);
                        Console.WriteLine("{\"event\":\"" + reply.Event + "\",\"close_code\":" + reply.CloseCode + "}");
                        return reply;
                    };
                };
                using (var stop = new ManualResetEvent(false))
                using (var server = new LoopbackWebSocketServer(settings.Port, factory))
                {
                    ConsoleCancelEventHandler cancel = (sender, e) => { e.Cancel = true; stop.Set(); };
                    Console.CancelKeyPress += cancel;
                    try
                    {
                        Console.WriteLine("{\"event\":\"ready\",\"scope\":\"" + (offline ? "original-offline-profile" : localAccount ? "local-account-only" : "preauth-only") + "\",\"playable\":false,\"port\":" + server.Port + "}");
                        stop.WaitOne(settings.RunSeconds * 1000);
                    }
                    finally { Console.CancelKeyPress -= cancel; }
                }
                }
                Console.WriteLine("{\"event\":\"stopped\"}"); return 0;
            }
            catch (Exception error)
            {
                // Values, file paths and exception messages may contain secrets.
                Console.Error.WriteLine("Gateway failed: " + error.GetType().Name); return 1;
            }
        }
    }
}
