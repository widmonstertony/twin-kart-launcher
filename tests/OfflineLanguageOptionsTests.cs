using System;
using System.IO;
using System.Text;
using TwinKartLauncher;

static class OfflineLanguageOptionsTests
{
    static void Need(bool value,string message){if(!value)throw new Exception(message);}
    static int Main(){try{
        var language=new OfflineLanguageOptions();
        string arguments=language.ApplyArguments("-nohmd -culture=ko-kr -language=ko-kr");
        Need(arguments.Contains("-nohmd") && arguments.Contains("-culture=zh-Hans") && arguments.Contains("-language=zh-Hans") && arguments.Contains("-locale=zh-Hans") && !arguments.Contains("ko-kr"),"Command-line culture was not replaced.");
        string input="[/Script/KartDrift.KartGameUserSettings]\r\nCultureIndex=8\r\nPlatformCultureIndex=8\r\nLastCultureIndex=8\r\nKeep=1\r\n[Other]\r\nCultureIndex=99\r\n";
        string routed=Encoding.UTF8.GetString(language.RouteUserSettings(Encoding.UTF8.GetBytes(input)));
        Need(routed.Contains("CultureIndex=3") && routed.Contains("PlatformCultureIndex=3") && routed.Contains("LastCultureIndex=3") && routed.Contains("Keep=1") && routed.Contains("[Other]\r\nCultureIndex=99"),"Native culture settings were not scoped and routed.");
        bool duplicate=false;try{language.RouteUserSettings(Encoding.UTF8.GetBytes(input.Replace("Keep=1","CultureIndex=2")));}catch(InvalidDataException){duplicate=true;}
        Need(duplicate,"Ambiguous culture settings were accepted.");
        Console.WriteLine("PASS Simplified Chinese command-line and session-owned native culture routing. No game started.");return 0;
    }catch(Exception error){Console.Error.WriteLine(error);return 1;}}
}
