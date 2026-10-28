using System;
using System.Diagnostics;
using System.IO;
using System.Web.Script.Serialization;
using System.Collections.Generic;
using TwinKartLauncher;
static class LocalRaceProfileTests
{
    static void Need(bool value) { if(!value) throw new Exception("Local race request contract failed."); }
    static int Main(string[] args) {
        string folder=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"test-scratch","race-"+Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(folder);
        try {
            if(args.Length!=1) throw new ArgumentException("Native control parser required.");
            var profile=new LauncherProfile { Edition=GameEdition.Drift }; var json=new JavaScriptSerializer();
            var legacy=json.Deserialize<Dictionary<string,object>>(json.Serialize(profile)); legacy.Remove("Race");
            File.WriteAllText(Path.Combine(folder,"drift.json"),json.Serialize(legacy));
            var store=new ProfileStore(folder); var restored=store.Load(GameEdition.Drift); Need(restored.Race.IsValid && restored.Race.AiCount==7);
            restored.Race.Mode=4; store.Save(restored); Need(store.Load(GameEdition.Drift).Race.Mode==4);
            string control=Path.Combine(folder,"race.control"); restored.Race.WriteControl(control);
            string before=File.ReadAllText(control); restored.Race.WriteControl(control); Need(before!=File.ReadAllText(control));
            before=File.ReadAllText(control); restored.Race.Track="Pirate_R01?listen";
            bool rejected=false; try { restored.Race.WriteControl(control); } catch(ArgumentException) { rejected=true; }
            Need(rejected && File.ReadAllText(control)==before);
            using(var process=Process.Start(new ProcessStartInfo(Path.GetFullPath(args[0]),"\""+control+"\"") { UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true })) {
                Console.Write(process.StandardOutput.ReadToEnd()); process.WaitForExit(); Need(process.ExitCode==0);
            }
            Console.WriteLine("PASS race settings migration/persistence, fresh request identities, invalid-write rollback and C# to native interoperability. No game started."); return 0;
        } catch(Exception error) { Console.Error.WriteLine(error); return 1; }
        finally { Directory.Delete(folder,true); }
    }
}
