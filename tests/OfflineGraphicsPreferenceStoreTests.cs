using System;
using System.Collections.Generic;
using System.IO;
using System.Web.Script.Serialization;
using TwinKartLauncher;
static class OfflineGraphicsPreferenceStoreTests
{
    static void Need(bool value,string message){if(!value)throw new Exception(message);}
    static void Reject(Action work){try{work();}catch(ArgumentException){return;}catch(InvalidDataException){return;}throw new Exception("Invalid preference accepted.");}
    public static int Main(){string folder=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"TwinKartGraphicsPreference-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(folder);try{
        string path=Path.Combine(folder,"launch.private.local.json");
        File.WriteAllText(path,"{\"GamePath\":\"original.exe\",\"Unknown\":{\"keep\":[1,2,3]},\"Environment\":{\"TWIN_KART_NR_STYLE\":\"1\",\"TWIN_KART_NR_PASSES\":\"2\",\"PIN\":\"keep\"},\"Graphics\":{\"SuperResolution\":true,\"FrameGeneration\":true,\"NeuralRendering\":true,\"NeuralPasses\":2,\"NeuralStyle\":1,\"RenderPercentage\":66.666667}}\n");
        var before=OfflineGraphicsPreferenceStore.Load(path);Need(before.NeuralStyle==1 && before.NeuralPasses==2 && before.Brightness==1,"Natural private graphics preference changed.");
        OfflineGraphicsPreferenceStore.Save(path,0,1.05,true);var defaults=OfflineGraphicsPreferenceStore.Load(path);
        Need(defaults.NeuralStyle==0 && defaults.SuperResolutionAndFrameGeneration && Math.Abs(defaults.Brightness-1.05)<0.000001,"Default DLSS 5 style did not persist.");
        OfflineGraphicsPreferenceStore.Save(path,2,1.1,true);var after=OfflineGraphicsPreferenceStore.Load(path);
        Need(after.NeuralStyle==2 && after.NeuralPasses==2 && Math.Abs(after.Brightness-1.1)<0.000001,"Movie/brightness preference did not persist.");
        var root=(Dictionary<string,object>)new JavaScriptSerializer().DeserializeObject(File.ReadAllText(path));
        var environment=(Dictionary<string,object>)root["Environment"];
        var graphics=(Dictionary<string,object>)root["Graphics"];
        var unknown=(Dictionary<string,object>)root["Unknown"];
        Need((string)environment["TWIN_KART_NR_STYLE"]=="2" && (string)environment["TWIN_KART_NR_PASSES"]=="2" && (string)environment["PIN"]=="keep" && ((object[])unknown["keep"]).Length==3,"Preference save changed pins or unrelated manifest data.");
        OfflineGraphicsPreferenceStore.Save(path,-1,1.2,true);var off=OfflineGraphicsPreferenceStore.Load(path);
        root=(Dictionary<string,object>)new JavaScriptSerializer().DeserializeObject(File.ReadAllText(path));graphics=(Dictionary<string,object>)root["Graphics"];
        Need(off.NeuralStyle==-1 && off.Brightness==1 && off.SuperResolutionAndFrameGeneration && (bool)graphics["SuperResolution"] && (bool)graphics["FrameGeneration"] && !(bool)graphics["NeuralRendering"],"Style Off did not leave SR/FG enabled while disabling only NR.");
        OfflineGraphicsPreferenceStore.Save(path,1,1,false);var nrOnly=OfflineGraphicsPreferenceStore.Load(path);
        root=(Dictionary<string,object>)new JavaScriptSerializer().DeserializeObject(File.ReadAllText(path));graphics=(Dictionary<string,object>)root["Graphics"];
        Need(nrOnly.NeuralStyle==1 && !nrOnly.SuperResolutionAndFrameGeneration && !(bool)graphics["SuperResolution"] && !(bool)graphics["FrameGeneration"] && (bool)graphics["NeuralRendering"],"Independent SR/FG switch altered the selected NR style.");
        OfflineGraphicsPreferenceStore.Save(path,1,1,true);var restored=OfflineGraphicsPreferenceStore.Load(path);
        Need(restored.NeuralStyle==1 && restored.SuperResolutionAndFrameGeneration,"Independent SR/FG switch did not re-enable both paths.");
        Reject(()=>OfflineGraphicsPreferenceStore.Save(path,-2,1,true));Reject(()=>OfflineGraphicsPreferenceStore.Save(path,3,1,true));Reject(()=>OfflineGraphicsPreferenceStore.Save(path,2,2,true));
        Need(Directory.GetFiles(folder,"*.tmp").Length==0,"Atomic preference update left a temporary file.");
        Console.WriteLine("PASS private graphics preference: NR style Off is independent from the paired SR/FG switch; Default/Natural/Movie, brightness, double pass and pins persist. No game started.");return 0;
    }catch(Exception e){Console.Error.WriteLine(e);return 1;}finally{Directory.Delete(folder,true);}}
}
