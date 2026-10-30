using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace TwinKartLauncher
{
    // Session-owned localization. The original files are restored when the
    // owned session exits; this never installs or redistributes language data.
    public sealed class OfflineLanguageOptions
    {
        public string Culture { get; set; }
        public OfflineLanguageOptions() { Culture="zh-Hans"; }

        public void Validate()
        {
            if(!String.Equals(Culture,"zh-Hans",StringComparison.OrdinalIgnoreCase))
                throw new ArgumentException("This build currently supports the verified Simplified Chinese route only.");
        }

        public string ApplyArguments(string arguments)
        {
            Validate();
            string value=arguments??"";
            value=Regex.Replace(value,"(^|\\s+)-(culture|language|locale)=(\\\"[^\\\"]*\\\"|\\S+)"," ",RegexOptions.IgnoreCase|RegexOptions.CultureInvariant);
            value=Regex.Replace(value,"\\s+"," ",RegexOptions.CultureInvariant).Trim();
            return value+(value.Length==0?"":" ")+"-culture=zh-Hans -language=zh-Hans -locale=zh-Hans";
        }

        public byte[] RouteUserSettings(byte[] original)
        {
            Validate();
            string text=new UTF8Encoding(false,true).GetString(original??new byte[0]).TrimStart('\ufeff');
            const string section="[/Script/KartDrift.KartGameUserSettings]";
            var keys=new HashSet<string>(new[]{"CultureIndex","PlatformCultureIndex","LastCultureIndex"},StringComparer.OrdinalIgnoreCase);
            var seen=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            var output=new List<string>();bool inside=false,found=false;
            Action append=()=>{foreach(string key in keys.OrderBy(x=>x,StringComparer.Ordinal))if(!seen.Contains(key))output.Add(key+"=3");};
            foreach(string line in text.Replace("\r\n","\n").Split('\n')) {
                string trimmed=line.Trim();
                if(trimmed.StartsWith("[",StringComparison.Ordinal) && trimmed.EndsWith("]",StringComparison.Ordinal)) {
                    if(inside)append();inside=String.Equals(trimmed,section,StringComparison.OrdinalIgnoreCase);
                    if(inside){if(found)throw new InvalidDataException("Duplicate native user settings section.");found=true;}
                }
                if(inside) {
                    int equal=trimmed.IndexOf('=');string key=equal<0?trimmed:trimmed.Substring(0,equal).Trim();
                    if(keys.Contains(key)) {
                        if(!seen.Add(key))throw new InvalidDataException("Duplicate native culture setting: "+key);
                        output.Add(key+"=3");continue;
                    }
                }
                output.Add(line);
            }
            if(!found){output.Add(section);inside=true;}
            if(inside)append();
            return new UTF8Encoding(false).GetBytes(String.Join("\r\n",output));
        }
    }
}
