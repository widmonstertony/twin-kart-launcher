using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;

namespace TwinKartLauncher
{
    internal static class AiRosterCatalog
    {
        static readonly string[] CharacterKeys={"Character","Costume","MotionFinish","MotionRetire","MotionVictory","MotionDefeat"};
        static readonly string[] KartKeys={"Kart","PartFront","PartSide","PartBack","PartWheel","PartHandle","PartPlate","KartBooster","Balloon","SkidMark"};
        static Dictionary<string,object> Map(object value) { var m=value as Dictionary<string,object>;if(m==null)throw new InvalidDataException("Invalid local AI catalog object.");return m; }
        static Dictionary<string,object> Read(string path) {
            var file=new FileInfo(path);if(!file.Exists || file.Length>32*1024*1024)throw new InvalidDataException("AI catalog input unavailable.");
            return Map(new JavaScriptSerializer {MaxJsonLength=32*1024*1024}.DeserializeObject(File.ReadAllText(path)));
        }
        static string Field(string key,Dictionary<string,object> outfit,HashSet<string> known) {
            object value;if(!outfit.TryGetValue(key,out value))return "-";
            string text=value as string;
            if(text==null || !known.Contains(text) || !Regex.IsMatch(text,@"\A[A-Za-z_]+:[0-9]{1,18}\z"))throw new InvalidDataException("Unknown AI outfit identifier.");
            return text;
        }
        internal static string Build(string gatewayConfig,string outputDirectory) {
            var config=Read(gatewayConfig);string directory=(string)config["ProfileDataDirectory"];
            var parts=Read(Path.Combine(directory,"vehicle-parts.private.json"));
            var inventory=Read(Path.Combine(directory,"unlock-catalog.private.json"));
            var known=new HashSet<string>(((object[])inventory["items"]).Select(x=>(string)Map(x)["id"]),StringComparer.Ordinal);
            var bodies=new HashSet<string>(((object[])inventory["kartBodies"]).Cast<string>(),StringComparer.Ordinal);
            var output=new StringBuilder("TKAI1\n");int characters=0,karts=0;
            foreach(var pair in Map(parts["characters"]).OrderBy(x=>x.Key,StringComparer.Ordinal)) {
                if(!known.Contains(pair.Key))continue;
                var outfit=new Dictionary<string,object>(Map(pair.Value));outfit["Character"]=pair.Key;
                if(!outfit.ContainsKey("Costume"))continue;
                output.Append("C ").Append(String.Join(" ",CharacterKeys.Select(key=>Field(key,outfit,known)))).Append('\n');++characters;
            }
            foreach(var pair in Map(parts["vehicles"]).OrderBy(x=>x.Key,StringComparer.Ordinal)) {
                if(!known.Contains(pair.Key) || !bodies.Contains(pair.Key))continue;
                var outfit=new Dictionary<string,object>(Map(parts["common"]));
                foreach(var part in Map(pair.Value))outfit[part.Key]=part.Value;
                outfit["Kart"]=pair.Key;
                output.Append("K ").Append(String.Join(" ",KartKeys.Select(key=>Field(key,outfit,known)))).Append('\n');++karts;
            }
            if(characters<8 || karts<8)throw new InvalidDataException("Insufficient compatible AI characters or kart bodies.");
            string path=Path.Combine(outputDirectory,"ai-catalog.private.txt");
            File.WriteAllText(path,output.ToString(),new UTF8Encoding(false));return path;
        }
    }
}
