using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Web.Script.Serialization;
using TwinKartLauncher;
static class CockpitProfileTests
{
    static void Need(bool value,string message) { if(!value) throw new Exception(message); }
    static int Main(string[] args)
    {
        string folder=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"test-scratch","cockpit-"+Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(folder);
        try {
            var profile=new LauncherProfile { Edition=GameEdition.Drift,ClientPath="synthetic-client.exe" };
            var json=new JavaScriptSerializer();
            var legacy=json.Deserialize<Dictionary<string,object>>(json.Serialize(profile)); legacy.Remove("Cockpit");
            File.WriteAllText(Path.Combine(folder,"drift.json"),json.Serialize(legacy));
            var store=new ProfileStore(folder); var loaded=store.Load(GameEdition.Drift);
            Need(loaded.ClientPath=="synthetic-client.exe" && loaded.Cockpit.Enabled && loaded.Cockpit.FollowHead,"Legacy launcher profile migration lost values");
            loaded.Cockpit.FieldOfView=105; store.Save(loaded);
            Need(store.Load(GameEdition.Drift).Cockpit.FieldOfView==105,"Cockpit settings not persisted");
            var settings=new CockpitSettings(); string control=Path.Combine(folder,"camera.control"); settings.WriteNativeControl(control);
            string before=File.ReadAllText(control); settings.FieldOfView=121;
            bool rejected=false; try { settings.WriteNativeControl(control); } catch(ArgumentOutOfRangeException) { rejected=true; }
            Need(rejected && File.ReadAllText(control)==before,"Invalid camera settings changed the control file");
            if(args.Length!=1 || !String.Equals(Path.GetFileName(args[0]),"CockpitControlTests.exe",StringComparison.OrdinalIgnoreCase)) throw new ArgumentException("Native cockpit contract test required.");
            var launch=new ProcessStartInfo(Path.GetFullPath(args[0]),"\""+control+"\"") { UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true };
            using(var process=Process.Start(launch)) { Console.Write(process.StandardOutput.ReadToEnd()); process.WaitForExit(); Need(process.ExitCode==0,"Managed/native control interoperability failed"); }
            Console.WriteLine("PASS cockpit profile migration, persistence, invalid-write isolation and actual managed-to-native control-file interoperability."); return 0;
        } catch(Exception error) { Console.Error.WriteLine(error); return 1; }
        finally { Directory.Delete(folder,true); }
    }
}
