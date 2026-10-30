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
            {"TWIN_KART_NEURAL_MODULE_SHA256",new String('b',64)},
            {"TWIN_KART_DLSS_MODULE","external-original-sr.dll"},{"TWIN_KART_DLSS_PROVIDER","external"},{"TWIN_KART_DLSS_MODULE_SHA256",new String('a',64)}};
        p.Validate(environment);
        p.ApplyEnvironment(environment,Path.GetTempPath());
        Need(environment["TWIN_KART_NR_PASSES"]=="2" && environment["TWIN_KART_NR_STYLE"]=="1","Natural/two-pass request not exported.");
        p.NeuralPasses=2;p.NeuralStyle=2;p.ApplyEnvironment(environment,Path.GetTempPath());
        Need(environment["TWIN_KART_NR_PASSES"]=="2" && environment["TWIN_KART_NR_STYLE"]=="2","Explicit Movie/two-pass comparison not preserved.");
        p.NeuralStyle=0;p.ApplyEnvironment(environment,Path.GetTempPath());Need(environment["TWIN_KART_NR_STYLE"]=="0","Default DLSS 5 style was not exported.");p.NeuralStyle=2;
        p.NeuralPasses=3;Reject(()=>p.Validate(environment));p.NeuralPasses=2;
        p.NeuralStyle=-1;Reject(()=>p.Validate(environment));p.NeuralStyle=2;
        p.Brightness=2;Reject(()=>p.Validate(environment));p.Brightness=1.1;
        Need(environment["TWIN_KART_SUPER_RESOLUTION"]=="1" && environment["TWIN_KART_RENDER_PERCENTAGE"]=="50","Native 120-FPS performance scale request was not exported");
        string ini="[Elsewhere]\r\nScreenPercentage=100\r\n[/Script/KartDrift.KartGameUserSettings]\r\nGraphicSetting=(ScreenPercentage=100.000000,Gamma=1.250000,TextureQuality=3)\r\nLastGraphicSetting=(ScreenPercentage=50,Gamma=1.250000)\r\nbUseDynamicResolution=True\r\n[Other]\r\nGraphicSetting=(ScreenPercentage=90)";
        byte[] source=Encoding.UTF8.GetBytes(ini);string output=Encoding.UTF8.GetString(p.RouteUserSettings(source));
        Need(Encoding.UTF8.GetString(source)==ini,"Source configuration buffer changed.");
        Need(output.Contains("GraphicSetting=(Brightness=1.1,ScreenPercentage=50,Gamma=1.250000,TextureQuality=3)") && output.Contains("[Other]\r\nGraphicSetting=(ScreenPercentage=90)"),"Native brightness or unrelated graphics settings changed.");
        Need(output.Contains("bUseDynamicResolution=False"),"Dynamic resolution could override the requested input.");
        string engine="[Other]\nr.ScreenPercentage=90\n[SystemSettings]\nr.ScreenPercentage=250\nr.ShadowQuality=3\n[Unchanged]\nKey=Value";
        byte[] engineSource=Encoding.UTF8.GetBytes(engine);
        string engineOutput=Encoding.UTF8.GetString(p.RouteEngineSettings(engineSource));
        Need(Encoding.UTF8.GetString(engineSource)==engine && engineOutput.Contains("[Other]\r\nr.ScreenPercentage=90") &&
            engineOutput.Contains("[SystemSettings]\r\nr.ScreenPercentage=50\r\nr.ShadowQuality=3") &&
            engineOutput.Contains("[Unchanged]\r\nKey=Value"),"Native engine cvar request changed unrelated settings");
        Need(Encoding.UTF8.GetString(p.RouteEngineSettings(Encoding.UTF8.GetBytes(engineOutput)))==engineOutput,"Engine cvar request not idempotent");
        Reject(()=>p.RouteEngineSettings(Encoding.UTF8.GetBytes("[SystemSettings]\nr.ScreenPercentage=50\nR.SCREENPERCENTAGE=60")));
        Reject(()=>p.RouteEngineSettings(Encoding.UTF8.GetBytes("[SystemSettings]\n[SystemSettings]")));
        Need(Encoding.UTF8.GetString(p.RouteUserSettings(Encoding.UTF8.GetBytes(output)))==output,"Graphics routing is not idempotent.");
        Reject(()=>p.RouteUserSettings(Encoding.UTF8.GetBytes("[/Script/KartDrift.KartGameUserSettings]\nGraphicSetting=(ScreenPercentage=50,ScreenPercentage=60)")));
        Reject(()=>p.RouteUserSettings(Encoding.UTF8.GetBytes("[/Script/KartDrift.KartGameUserSettings]\n[/Script/KartDrift.KartGameUserSettings]")));
        p.RenderPercentage=100;Reject(()=>p.Validate(environment));p.RenderPercentage=50;
        p.FrameGeneration=true;Reject(()=>p.Validate(environment));
        p.NeuralRendering=false;Reject(()=>p.Validate(environment));
        p.SuperResolution=false;p.NeuralRendering=true;
        string fgOnly=Encoding.UTF8.GetString(p.RouteUserSettings(Encoding.UTF8.GetBytes("[/Script/KartDrift.KartGameUserSettings]\nGraphicSetting=(ScreenPercentage=80,ReflexMode=1,Gamma=1.2)")));
        Need(fgOnly.Contains("GraphicSetting=(Brightness=1.1,ScreenPercentage=80,ReflexMode=0,Gamma=1.2)"),"FG-only policy changed resolution, brightness or retained competing native Reflex.");
        p.FrameGeneration=false;p.Brightness=1;Need(Object.ReferenceEquals(source,p.RouteUserSettings(source)),"Disabled policy rewrote user bytes.");
        Need(Object.ReferenceEquals(source,p.RouteEngineSettings(source)),"Disabled SR policy rewrote engine bytes");
        environment["TWIN_KART_FG_PROVIDER"]="external";p.SuperResolution=true;p.FrameGeneration=true;p.NeuralRendering=false;p.NeuralStyle=0;
        p.Validate(environment);p.ApplyEnvironment(environment,Path.GetTempPath());
        Need(environment["TWIN_KART_SUPER_RESOLUTION"]=="1" && environment["TWIN_KART_FRAME_GENERATION"]=="1" && environment["TWIN_KART_NEURAL_MODULE"]=="","NR Off did not preserve the independent SR and FG paths.");
        p.SuperResolution=false;p.FrameGeneration=false;p.ApplyEnvironment(environment,Path.GetTempPath());
        Need(environment["TWIN_KART_SUPER_RESOLUTION"]=="0" && environment["TWIN_KART_FRAME_GENERATION"]=="0","Independent SR/FG switch did not disable both paths.");
        string scratch=Path.Combine(Path.GetTempPath(),"TwinKartGraphicsPreflight-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(scratch);
        try {
            string module=Path.Combine(scratch,"original-fixture.bin");File.WriteAllText(module,"original fixture, never loaded");
            string fingerprint;
            using(var file=File.OpenRead(module))using(var hash=System.Security.Cryptography.SHA256.Create())fingerprint=BitConverter.ToString(hash.ComputeHash(file)).Replace("-","").ToLowerInvariant();
            var files=new Dictionary<string,string>{{"TWIN_KART_DLSS_MODULE",module},{"TWIN_KART_DLSS_MODULE_SHA256",fingerprint},{"TWIN_KART_DLSS_PROVIDER",scratch},
                {"TWIN_KART_NEURAL_MODULE",module},{"TWIN_KART_NEURAL_MODULE_SHA256",fingerprint},{"TWIN_KART_NEURAL_PROVIDER",scratch}};
            var all=new OfflineGraphicsOptions { SuperResolution=true,NeuralRendering=true };
            all.ValidateFiles(files);Need(all.NeedsRenderer,"SR/NR request missed early renderer startup");
            File.AppendAllText(module,"changed");Reject(()=>all.ValidateFiles(files));
            var nrOnly=new OfflineGraphicsOptions {NeuralRendering=true};
            Reject(()=>nrOnly.ValidateFiles(files));
            files["TWIN_KART_NEURAL_MODULE_SHA256"]="";Reject(()=>nrOnly.Validate(files));
            files["TWIN_KART_NEURAL_MODULE_SHA256"]=new String('x',64);Reject(()=>nrOnly.Validate(files));
            using(var file=File.OpenRead(module))using(var hash=System.Security.Cryptography.SHA256.Create())files["TWIN_KART_NEURAL_MODULE_SHA256"]=BitConverter.ToString(hash.ComputeHash(file)).Replace("-","").ToLowerInvariant();
            nrOnly.ValidateFiles(files);
        } finally {Directory.Delete(scratch,true);}
        Console.WriteLine("PASS offline graphics routing: NR Default/Natural/Movie/Off is independent from paired SR/FG, with double NR pass, brightness and dependency rejection. No game started.");return 0;
    }catch(Exception e){Console.Error.WriteLine(e.Message);return 1;}}
}
