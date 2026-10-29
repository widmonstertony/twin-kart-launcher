using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using TwinKartLauncher;
static class OfflineGraphicsOptionsTests
{
    static void Need(bool value,string message){if(!value)throw new Exception(message);}
    static void Reject(Action work){try{work();}catch(ArgumentException){return;}catch(InvalidDataException){return;}throw new Exception("Invalid graphics policy accepted.");}
    public static int Main(){try{
        var p=new OfflineGraphicsOptions {SuperResolution=true,NeuralRendering=true};
        var environment=new Dictionary<string,string>{{"TWIN_KART_NEURAL_MODULE","external-original.dll"},{"TWIN_KART_NEURAL_PROVIDER","external"},
            {"TWIN_KART_DLSS_MODULE","external-original-sr.dll"},{"TWIN_KART_DLSS_PROVIDER","external"},{"TWIN_KART_DLSS_MODULE_SHA256",new String('a',64)}};
        p.Validate(environment);
        string ini="[Elsewhere]\r\nScreenPercentage=100\r\n[/Script/KartDrift.KartGameUserSettings]\r\nGraphicSetting=(ScreenPercentage=100.000000,Gamma=1.250000,TextureQuality=3)\r\nLastGraphicSetting=(ScreenPercentage=50,Gamma=1.250000)\r\nbUseDynamicResolution=True\r\n[Other]\r\nGraphicSetting=(ScreenPercentage=90)";
        byte[] source=Encoding.UTF8.GetBytes(ini);string output=Encoding.UTF8.GetString(p.RouteUserSettings(source));
        Need(Encoding.UTF8.GetString(source)==ini,"Source configuration buffer changed.");
        Need(output.Contains("ScreenPercentage=66.666667,Gamma=1.250000,TextureQuality=3") && output.Contains("[Other]\r\nGraphicSetting=(ScreenPercentage=90)"),"Unrelated native graphics setting changed.");
        Need(output.Contains("bUseDynamicResolution=False"),"Dynamic resolution could override the requested input.");
        string engine="[Other]\nr.ScreenPercentage=90\n[SystemSettings]\nr.ScreenPercentage=250\nr.ShadowQuality=3\n[Unchanged]\nKey=Value";
        byte[] engineSource=Encoding.UTF8.GetBytes(engine);
        string engineOutput=Encoding.UTF8.GetString(p.RouteEngineSettings(engineSource));
        Need(Encoding.UTF8.GetString(engineSource)==engine && engineOutput.Contains("[Other]\r\nr.ScreenPercentage=90") &&
            engineOutput.Contains("[SystemSettings]\r\nr.ScreenPercentage=66.666667\r\nr.ShadowQuality=3") &&
            engineOutput.Contains("[Unchanged]\r\nKey=Value"),"Native engine cvar request changed unrelated settings");
        Need(Encoding.UTF8.GetString(p.RouteEngineSettings(Encoding.UTF8.GetBytes(engineOutput)))==engineOutput,"Engine cvar request not idempotent");
        Reject(()=>p.RouteEngineSettings(Encoding.UTF8.GetBytes("[SystemSettings]\nr.ScreenPercentage=50\nR.SCREENPERCENTAGE=60")));
        Reject(()=>p.RouteEngineSettings(Encoding.UTF8.GetBytes("[SystemSettings]\n[SystemSettings]")));
        Need(Encoding.UTF8.GetString(p.RouteUserSettings(Encoding.UTF8.GetBytes(output)))==output,"Graphics routing is not idempotent.");
        Reject(()=>p.RouteUserSettings(Encoding.UTF8.GetBytes("[/Script/KartDrift.KartGameUserSettings]\nGraphicSetting=(ScreenPercentage=50,ScreenPercentage=60)")));
        Reject(()=>p.RouteUserSettings(Encoding.UTF8.GetBytes("[/Script/KartDrift.KartGameUserSettings]\n[/Script/KartDrift.KartGameUserSettings]")));
        p.RenderPercentage=100;Reject(()=>p.Validate(environment));p.RenderPercentage=66;
        p.FrameGeneration=true;Reject(()=>p.Validate(environment));
        p.NeuralRendering=false;Reject(()=>p.Validate(environment));
        p.SuperResolution=false;p.NeuralRendering=true;
        string fgOnly=Encoding.UTF8.GetString(p.RouteUserSettings(Encoding.UTF8.GetBytes("[/Script/KartDrift.KartGameUserSettings]\nGraphicSetting=(ScreenPercentage=80,ReflexMode=1,Gamma=1.2)")));
        Need(fgOnly.Contains("ScreenPercentage=80,ReflexMode=0,Gamma=1.2"),"FG-only policy changed resolution or retained competing native Reflex.");
        p.FrameGeneration=false;Need(Object.ReferenceEquals(source,p.RouteUserSettings(source)),"Disabled policy rewrote user bytes.");
        Need(Object.ReferenceEquals(source,p.RouteEngineSettings(source)),"Disabled SR policy rewrote engine bytes");
        string scratch=Path.Combine(Path.GetTempPath(),"TwinKartGraphicsPreflight-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(scratch);
        try {
            string module=Path.Combine(scratch,"original-fixture.bin");File.WriteAllText(module,"original fixture, never loaded");
            string fingerprint;
            using(var file=File.OpenRead(module))using(var hash=System.Security.Cryptography.SHA256.Create())fingerprint=BitConverter.ToString(hash.ComputeHash(file)).Replace("-","").ToLowerInvariant();
            var files=new Dictionary<string,string>{{"TWIN_KART_DLSS_MODULE",module},{"TWIN_KART_DLSS_MODULE_SHA256",fingerprint},{"TWIN_KART_DLSS_PROVIDER",scratch},
                {"TWIN_KART_NEURAL_MODULE",module},{"TWIN_KART_NEURAL_PROVIDER",scratch}};
            var all=new OfflineGraphicsOptions { SuperResolution=true,NeuralRendering=true };
            all.ValidateFiles(files);Need(all.NeedsRenderer,"SR/NR request missed early renderer startup");
            File.AppendAllText(module,"changed");Reject(()=>all.ValidateFiles(files));
        } finally {Directory.Delete(scratch,true);}
        Console.WriteLine("PASS offline graphics routing: lower native resolution, unrelated settings preserved, idempotence, ambiguous-input/dependency rejection and no file/game writes.");return 0;
    }catch(Exception e){Console.Error.WriteLine(e.Message);return 1;}}
}
