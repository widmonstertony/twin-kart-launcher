using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;

namespace TwinKartLauncher.Gateway
{
    public sealed class TrackResourceEvidence
    {
        public string TrackId,BaseAlias,WorldMatch,AiPathMatch,WorldObjectPath,AiPathObjectPath;
        public bool MirrorRequested,ReverseRequested,DrivingVerified;
        public string NativeDefinitionStatus,NativeLevelName;
    }
    public sealed class TrackSupportReport
    {
        public string ClientSha256;
        public int NativeAssets,NativeWorldAssets,NativeAiPathAssets,CatalogVariants,CatalogBaseAliases;
        public int ExactMainWorldBaseAliases,ReverseWorldCandidates,UnmatchedMainWorldBaseAliases;
        public int ExactAiPathBaseAliases,ReverseAiPathCandidates,UnmatchedAiPathBaseAliases;
        public string[] UnmatchedMainWorldAliases,NativeTrackRootsWithoutExactCatalogAlias;
        public bool AllTracksDriven;
        public bool NativeDefinitionsComplete;
        public int NativeDefinitionsAvailable,NativeDefinitionsEmpty;
        public TrackResourceEvidence[] Tracks;
    }
    // Class/path inventory establishes resource records, not successful loading,
    // correct mirror/reverse transforms, AI navigation or completed races.
    public static class TrackSupportAudit
    {
        public static void AttachDefinitions(TrackSupportReport report,string lookupJsonLines)
        {
            if(report==null || lookupJsonLines==null || lookupJsonLines.Length>1024*1024 || report.NativeDefinitionsComplete) throw new ArgumentException("Invalid native definition input.");
            var json=new JavaScriptSerializer { MaxJsonLength=16384 };
            var definitions=new Dictionary<string,Dictionary<string,object>>(StringComparer.Ordinal);
            bool ready=false,complete=false; int thread=0,found=0,empty=0;
            using(var reader=new StringReader(lookupJsonLines)) {
                string line;
                while((line=reader.ReadLine())!=null) {
                    if(String.IsNullOrWhiteSpace(line)) continue;
                    if(complete) throw new FormatException("Rows after native definition completion.");
                    var row=json.Deserialize<Dictionary<string,object>>(line); string kind=Text(row,"event");
                    if(kind=="native_race_bridge_ready") {
                        if(ready || !String.Equals(Text(row,"client_sha256"),report.ClientSha256,StringComparison.OrdinalIgnoreCase)) throw new FormatException("Native definition client mismatch.");
                        thread=Number(row,"game_thread"); if(thread==0) throw new FormatException("Missing game thread."); ready=true;
                    } else if(kind=="native_track_lookup_waiting") {
                        if(!ready || Number(row,"game_thread")!=thread) throw new FormatException("Native lookup thread mismatch.");
                    } else if(kind=="native_track_definition") {
                        if(!ready || Number(row,"game_thread")!=thread) throw new FormatException("Native lookup thread mismatch.");
                        string id=Text(row,"track_id"),level=Text(row,"native_level_name"); int status=Number(row,"status");
                        if(definitions.Count>=10000 || definitions.ContainsKey(id) || (status!=1 && status!=2) ||
                            (status==1 && !TrackName.IsMatch(level)) || (status==2 && level.Length!=0)) throw new FormatException("Invalid native track definition.");
                        definitions.Add(id,row); if(status==1) ++found; else ++empty;
                    } else if(kind=="native_track_queries_complete") {
                        if(!ready || Number(row,"found")!=found || Number(row,"empty")!=empty || definitions.Count!=report.Tracks.Length) throw new FormatException("Incomplete native definition counts.");
                        complete=true;
                    } else throw new FormatException("Failed or unknown native definition event.");
                }
            }
            if(!complete || report.Tracks.Any(track=>!definitions.ContainsKey(track.TrackId))) throw new FormatException("Native definition capture did not cover the catalog.");
            // Commit only after the entire capture has passed validation.
            foreach(var track in report.Tracks) {
                var row=definitions[track.TrackId]; track.NativeLevelName=Text(row,"native_level_name");
                track.NativeDefinitionStatus=Number(row,"status")==1?"native-definition-present":
                    track.MirrorRequested || track.ReverseRequested?"variant-definition-unresolved":"no-native-definition-in-client";
            }
            report.NativeDefinitionsComplete=true; report.NativeDefinitionsAvailable=found; report.NativeDefinitionsEmpty=empty;
        }
        static readonly Regex TrackName=new Regex("^[A-Za-z]+_[IR][0-9]+(?:_rvs)?$",RegexOptions.CultureInvariant);
        static string Text(Dictionary<string,object> row,string key)
        { object value; if(!row.TryGetValue(key,out value) || !(value is string)) throw new FormatException("Missing inventory text field: "+key); return (string)value; }
        static int Number(Dictionary<string,object> row,string key)
        { object value; if(!row.TryGetValue(key,out value) || !(value is int) || (int)value<0) throw new FormatException("Invalid inventory count."); return (int)value; }
        static bool Unique(Dictionary<string,string> entries,string key,out string path)
        { return entries.TryGetValue(key,out path) && path!=null; }
        static string AssetName(string path)
        {
            if(path.Length>1024 || !path.StartsWith("/",StringComparison.Ordinal) || path.Contains("..") || path.Any(ch=>Char.IsControl(ch))) throw new FormatException("Invalid native object path.");
            int dot=path.LastIndexOf('.'),slash=path.LastIndexOf('/');
            if(dot<=slash || dot==path.Length-1) throw new FormatException("Expected native package.object path.");
            string name=path.Substring(dot+1);
            if(!String.Equals(path.Substring(slash+1,dot-slash-1),name,StringComparison.OrdinalIgnoreCase)) throw new FormatException("Unexpected native top-level object name.");
            return name;
        }
        public static TrackSupportReport Read(string inventoryJsonLines,string trackCatalogJson,string expectedClientSha256)
        {
            if(!Regex.IsMatch(expectedClientSha256??"","^[0-9a-fA-F]{64}$") || inventoryJsonLines==null || inventoryJsonLines.Length>16*1024*1024) throw new ArgumentException("Invalid inventory/build input.");
            var json=new JavaScriptSerializer { MaxJsonLength=16384 };
            var worlds=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            var paths=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            bool started=false,complete=false; int worldRows=0,aiRows=0; Dictionary<string,object> footer=null;
            using(var reader=new StringReader(inventoryJsonLines)) {
                string line; int rows=0;
                while((line=reader.ReadLine())!=null) {
                    if(String.IsNullOrWhiteSpace(line)) continue;
                    if(++rows>100000 || complete) throw new FormatException("Unexpected rows after completion or oversized inventory.");
                    var row=json.Deserialize<Dictionary<string,object>>(line);
                    if(row==null) throw new FormatException("Expected inventory object.");
                    string type=Text(row,"event");
                    if(type=="asset_registry_begin") {
                        if(started || !String.Equals(Text(row,"client_sha256"),expectedClientSha256,StringComparison.OrdinalIgnoreCase)) throw new FormatException("Inventory client mismatch.");
                        started=true;
                    } else if(type=="native_asset") {
                        if(!started) throw new FormatException("Inventory missing build header.");
                        string assetClass=Text(row,"class"),path=Text(row,"object_path"),name=AssetName(path);
                        if(assetClass=="World") { if(worlds.ContainsKey(name)) worlds[name]=null; else worlds.Add(name,path); ++worldRows; }
                        else if(assetClass.Contains("AI") && assetClass.Contains("Path")) { if(paths.ContainsKey(name)) paths[name]=null; else paths.Add(name,path); ++aiRows; }
                        else throw new FormatException("Unexpected asset class in inventory.");
                    } else if(type=="asset_registry_complete") {
                        if(!started || Number(row,"worlds")!=worldRows || Number(row,"ai_path_assets")!=aiRows || Number(row,"assets")<worldRows+aiRows || worldRows==0) throw new FormatException("Incomplete inventory counts.");
                        footer=row; complete=true;
                    } else throw new FormatException("Inventory failed or has an unknown event.");
                }
            }
            if(!complete) throw new FormatException("Native inventory did not complete.");
            string[] tracks=new TrackCatalog(trackCatalogJson).All;
            var aliases=new HashSet<string>(tracks.Select(id=>id.EndsWith("_Mirror",StringComparison.Ordinal)?id.Substring(0,id.Length-7):id),StringComparer.OrdinalIgnoreCase);
            var result=new TrackSupportReport { ClientSha256=expectedClientSha256.ToLowerInvariant(),NativeAssets=Number(footer,"assets"),NativeWorldAssets=worldRows,NativeAiPathAssets=aiRows,CatalogVariants=tracks.Length,CatalogBaseAliases=aliases.Count };
            var missing=new List<string>();
            foreach(string alias in aliases) {
                string ignored;
                if(Unique(worlds,alias,out ignored)) ++result.ExactMainWorldBaseAliases;
                else if(alias.EndsWith("_rvs",StringComparison.OrdinalIgnoreCase) && Unique(worlds,alias.Substring(0,alias.Length-4),out ignored)) ++result.ReverseWorldCandidates;
                else { ++result.UnmatchedMainWorldBaseAliases; missing.Add(alias); }
                if(Unique(paths,alias,out ignored)) ++result.ExactAiPathBaseAliases;
                else if(alias.EndsWith("_rvs",StringComparison.OrdinalIgnoreCase) && Unique(paths,alias.Substring(0,alias.Length-4),out ignored)) ++result.ReverseAiPathCandidates;
                else ++result.UnmatchedAiPathBaseAliases;
            }
            var evidence=new List<TrackResourceEvidence>();
            foreach(string track in tracks) {
                bool mirror=track.EndsWith("_Mirror",StringComparison.Ordinal); string alias=mirror?track.Substring(0,track.Length-7):track;
                var row=new TrackResourceEvidence { TrackId=track,BaseAlias=alias,MirrorRequested=mirror,ReverseRequested=alias.EndsWith("_rvs",StringComparison.OrdinalIgnoreCase),WorldMatch="unmatched",AiPathMatch="unmatched" };
                string path;
                if(Unique(worlds,alias,out path)) { row.WorldObjectPath=path; row.WorldMatch="exact-base-world-record"; }
                else if(row.ReverseRequested && Unique(worlds,alias.Substring(0,alias.Length-4),out path)) { row.WorldObjectPath=path; row.WorldMatch="reverse-base-candidate"; }
                else if(worlds.ContainsKey(alias)) row.WorldMatch="ambiguous-leaf-name";
                if(Unique(paths,alias,out path)) { row.AiPathObjectPath=path; row.AiPathMatch="exact-base-ai-record"; }
                else if(row.ReverseRequested && Unique(paths,alias.Substring(0,alias.Length-4),out path)) { row.AiPathObjectPath=path; row.AiPathMatch="reverse-base-candidate"; }
                else if(paths.ContainsKey(alias)) row.AiPathMatch="ambiguous-leaf-name";
                evidence.Add(row);
            }
            result.Tracks=evidence.ToArray(); result.UnmatchedMainWorldAliases=missing.OrderBy(x=>x,StringComparer.Ordinal).ToArray();
            result.NativeTrackRootsWithoutExactCatalogAlias=worlds.Keys.Where(name=>TrackName.IsMatch(name) && !aliases.Contains(name)).OrderBy(x=>x,StringComparer.Ordinal).ToArray();
            return result;
        }
    }
}
