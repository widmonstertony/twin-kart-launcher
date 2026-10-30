using System;
using System.Text;
using TwinKartLauncher;
static class OfflineKeyboardOptionsTests
{
    static void Need(bool value,string message){if(!value)throw new Exception(message);}
    static int Count(string value,string part){int count=0,offset=0;while((offset=value.IndexOf(part,offset,StringComparison.Ordinal))>=0){++count;offset+=part.Length;}return count;}
    public static int Main(){try{
        var option=new OfflineKeyboardOptions {AddWasd=true};
        string input="[/Script/Engine.InputSettings]\nAxisMappings=(AxisName=\"MoveForward\",Scale=1.000000,Key=Up)\nAxisMappings=(AxisName=\"MoveRight\",Scale=-1.000000,Key=Left)\nAxisMappings=(AxisName=\"MoveRight\",Scale=1.000000,Key=Right)\n[Other]\nKeep=True";
        string routed=Encoding.UTF8.GetString(option.RouteInputSettings(Encoding.UTF8.GetBytes(input)));
        foreach(string pair in new[]{"MoveForward\",Scale=1.000000,Key=W","MoveForward\",Scale=-2.000000,Key=S","MoveLeft_NewCutting\",Scale=-1.000000,Key=A","MoveRight\",Scale=-1.000000,Key=A","MoveRight\",Scale=1.000000,Key=D","MoveRight_NewCutting\",Scale=1.000000,Key=D"})Need(Count(routed,pair)==1,"Missing or duplicate WASD axis: "+pair);
        Need(routed.Contains("Key=Up") && routed.Contains("Key=Left") && routed.Contains("Key=Right") && routed.Contains("[Other]\r\nKeep=True"),"Arrow keys or unrelated input settings changed.");
        Need(Count(routed,"ActionMappings=(ActionName=\"Drift\",Key=RightShift)")==1,"Right Shift drift action is missing or duplicated.");
        Need(Encoding.UTF8.GetString(option.RouteInputSettings(Encoding.UTF8.GetBytes(routed)))==routed,"Input routing is not idempotent.");
        string preset="[/Script/KartDrift.KeyPresetConfig]\nActivePreset=Traditional\nTraditionalPresetOverrides=(AxisOverrides=((AxisName=\"MoveForward\",Key=Up),(AxisName=\"MoveRight\",Key=Right)),ActionOverrides=((ActionName=\"Drift\",Key=LeftShift)))\nWASDPresetOverrides=(AxisOverrides=(),ActionOverrides=())";
        string keys=Encoding.UTF8.GetString(option.RouteKeyPreset(Encoding.UTF8.GetBytes(preset)));
        Need(keys.Contains("ActivePreset=Traditional") && keys.Contains("Key=Up") && keys.Contains("Key=Right") && keys.Contains("ActionName=\"Drift\""),"Traditional arrows/actions were replaced.");
        Need(Count(keys,"(ActionName=\"Drift\",Key=LeftShift)")==1,"Traditional Left Shift drift was not preserved.");
        Need(Count(keys,"(ActionName=\"Drift\",Key=RightShift)")==1,"Traditional Right Shift drift is missing or duplicated.");
        foreach(string key in new[]{"Key=W","Key=S","Key=A","Key=D"})Need(keys.Contains(key),"Preset missing "+key);
        Need(Encoding.UTF8.GetString(option.RouteKeyPreset(Encoding.UTF8.GetBytes(keys)))==keys,"Preset routing is not idempotent.");
        option.AddWasd=false;Need(Object.ReferenceEquals(option.RouteInputSettings(null),null) && Object.ReferenceEquals(option.RouteKeyPreset(null),null),"Disabled WASD policy rewrote input.");
        Console.WriteLine("PASS keyboard routing: W/S/A/D add forward/reverse/left/right while original arrow mappings and actions remain intact. No game started.");return 0;
    }catch(Exception e){Console.Error.WriteLine(e);return 1;}}
}
