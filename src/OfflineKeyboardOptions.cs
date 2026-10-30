using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;

namespace TwinKartLauncher
{
    public sealed class OfflineKeyboardOptions
    {
        public bool AddWasd { get; set; }
        static readonly string[] AxisEntries={
            "AxisMappings=(AxisName=\"MoveForward\",Scale=1.000000,Key=W)",
            "AxisMappings=(AxisName=\"MoveForward\",Scale=-2.000000,Key=S)",
            "AxisMappings=(AxisName=\"MoveLeft_NewCutting\",Scale=-1.000000,Key=A)",
            "AxisMappings=(AxisName=\"MoveRight\",Scale=-1.000000,Key=A)",
            "AxisMappings=(AxisName=\"MoveRight\",Scale=1.000000,Key=D)",
            "AxisMappings=(AxisName=\"MoveRight_NewCutting\",Scale=1.000000,Key=D)" };
        const string RightShiftDrift="ActionMappings=(ActionName=\"Drift\",Key=RightShift)";
        static readonly string[] PresetEntries={
            "(AxisName=\"MoveForward\",Scale=1.000000,Key=W)",
            "(AxisName=\"MoveForward\",Scale=-2.000000,Key=S)",
            "(AxisName=\"MoveLeft_NewCutting\",Scale=-1.000000,Key=A)",
            "(AxisName=\"MoveRight\",Scale=-1.000000,Key=A)",
            "(AxisName=\"MoveRight\",Scale=1.000000,Key=D)",
            "(AxisName=\"MoveRight_NewCutting\",Scale=1.000000,Key=D)" };
        const string RightShiftDriftPreset="(ActionName=\"Drift\",Key=RightShift)";
        static string Decode(byte[] original)
        { return new UTF8Encoding(false,true).GetString(original??new byte[0]).TrimStart('\ufeff').Replace("\r\n","\n"); }
        static bool Mapping(string line,string axis,string key)
        {
            return Regex.IsMatch(line,"AxisName=\\\""+Regex.Escape(axis)+"\\\"[^)]*Key="+Regex.Escape(key)+"(?:[,)]|$)",RegexOptions.CultureInvariant);
        }
        static bool ActionMapping(string line,string action,string key)
        {
            return Regex.IsMatch(line,"ActionName=\\\""+Regex.Escape(action)+"\\\"[^)]*Key="+Regex.Escape(key)+"(?:[,)]|$)",RegexOptions.CultureInvariant);
        }
        public byte[] RouteInputSettings(byte[] original)
        {
            if(!AddWasd)return original;
            var lines=new List<string>(Decode(original).Split('\n'));int section=-1,end=lines.Count;
            for(int i=0;i<lines.Count;++i) {
                string value=lines[i].Trim();
                if(value.StartsWith("[") && value.EndsWith("]")) {
                    if(String.Equals(value,"[/Script/Engine.InputSettings]",StringComparison.OrdinalIgnoreCase)) {if(section>=0)throw new InvalidDataException("Duplicate input settings section.");section=i;}
                    else if(section>=0 && end==lines.Count)end=i;
                }
            }
            if(section<0){if(lines.Count==1 && lines[0].Length==0)lines.Clear();lines.Add("[/Script/Engine.InputSettings]");section=lines.Count-1;end=lines.Count;}
            var additions=new List<string>();
            foreach(string entry in AxisEntries) {
                var match=Regex.Match(entry,"AxisName=\\\"([^\\\"]+)\\\".*Key=([^,)]+)");bool present=false;
                for(int i=section+1;i<end;++i)if(Mapping(lines[i],match.Groups[1].Value,match.Groups[2].Value)){present=true;break;}
                if(!present)additions.Add(entry);
            }
            bool rightShiftPresent=false;
            for(int i=section+1;i<end;++i)if(ActionMapping(lines[i],"Drift","RightShift")){rightShiftPresent=true;break;}
            if(!rightShiftPresent)additions.Add(RightShiftDrift);
            lines.InsertRange(end,additions);
            return new UTF8Encoding(false).GetBytes(String.Join("\r\n",lines));
        }
        public byte[] RouteKeyPreset(byte[] original)
        {
            if(!AddWasd)return original;
            var lines=new List<string>(Decode(original).Split('\n'));int active=-1,traditional=-1;
            for(int i=0;i<lines.Count;++i) {
                string value=lines[i].Trim();
                if(value.StartsWith("ActivePreset=",StringComparison.OrdinalIgnoreCase)){if(active>=0)throw new InvalidDataException("Duplicate active key preset.");active=i;}
                if(value.StartsWith("TraditionalPresetOverrides=",StringComparison.OrdinalIgnoreCase)){if(traditional>=0)throw new InvalidDataException("Duplicate traditional preset.");traditional=i;}
            }
            if(active<0 || traditional<0)throw new InvalidDataException("Original keyboard preset structure unavailable.");
            lines[active]="ActivePreset=Traditional";
            string row=lines[traditional],marker="),ActionOverrides=(";int split=row.IndexOf(marker,StringComparison.Ordinal);
            if(split<0 || row.IndexOf(marker,split+1,StringComparison.Ordinal)>=0)throw new InvalidDataException("Malformed traditional preset overrides.");
            var add=new List<string>();
            foreach(string entry in PresetEntries) {
                var match=Regex.Match(entry,"AxisName=\\\"([^\\\"]+)\\\".*Key=([^,)]+)");
                if(!Mapping(row,match.Groups[1].Value,match.Groups[2].Value))add.Add(entry);
            }
            string routed=row;
            if(add.Count>0)routed=row.Substring(0,split)+(row.Substring(0,split).EndsWith("(",StringComparison.Ordinal)?"":",")+String.Join(",",add)+row.Substring(split);
            if(!ActionMapping(routed,"Drift","RightShift")) {
                int action=routed.IndexOf(marker,StringComparison.Ordinal)+marker.Length;
                routed=routed.Insert(action,RightShiftDriftPreset+(routed[action]==')'?"":","));
            }
            lines[traditional]=routed;
            return new UTF8Encoding(false).GetBytes(String.Join("\r\n",lines));
        }
    }
}
