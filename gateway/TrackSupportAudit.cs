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
        public int? NativeModeMask,NativeLaps;
        public bool NativeCatalogDiscovered;
        public bool ExternalCatalogMember=true;
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
        public bool NativeCatalogComplete;
        public int NativeCatalogRows,NativeCatalogDefinitions,NativeCatalogNonRacing,NativeDefinitionsOutsideExternalCatalog;
        public TrackResourceEvidence[] NativeWorldCandidates;
    }
    // Class/path inventory establishes resource records, not successful loading,
    // correct mirror/reverse transforms, AI navigation or completed races.
    public static class TrackSupportAudit
    {
        public static void AttachDefinitions(TrackSupportReport report,string lookupJsonLines)
        {
            if(report==null || report.Tracks==null || lookupJsonLines==null || lookupJsonLines.Length>4*1024*1024 || report.NativeDefinitionsComplete) throw new ArgumentException("Invalid native definition input.");
            var json=new JavaScriptSerializer { MaxJsonLength=16384 };
            var definitions=new Dictionary<string,Dictionary<string,object>>(StringComparer.Ordinal);
            var catalog=new Dictionary<string,Dictionary<string,object>>(StringComparer.Ordinal);
            var known=new HashSet<string>(report.Tracks.Select(t=>t.TrackId),StringComparer.Ordinal);
            if(known.Count!=report.Tracks.Length || known.Count>10000)throw new FormatException("Invalid base track catalog.");
            var spellings=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            bool catalogStarted=false,catalogComplete=false;int catalogRows=0,catalogCount=0,catalogNonRacing=0;
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
                    } else if(kind=="native_track_catalog_begin") {
                        if(!ready || catalogStarted || definitions.Count!=0 || Number(row,"game_thread")!=thread)throw new FormatException("Invalid native catalog boundary.");
                        catalogRows=Number(row,"rows");catalogCount=Number(row,"count");catalogNonRacing=Number(row,"non_racing");
                        if(catalogRows<1 || catalogRows>10000 || catalogCount>10000 || catalogNonRacing>10000 || catalogCount+catalogNonRacing!=catalogRows)throw new FormatException("Invalid native catalog counts.");
                        catalogStarted=true;
                    } else if(kind=="native_track_catalog_entry") {
                        if(!catalogStarted || catalogComplete || Number(row,"game_thread")!=thread || catalog.Count>=catalogCount)throw new FormatException("Invalid native catalog entry boundary.");
                        string id=Text(row,"track_id");int mask=Number(row,"native_mode_mask"),laps=Number(row,"native_laps");
                        if(!NativeAlias.IsMatch(id) || mask>31 || (mask&1)!=0 || (mask&30)==0 || (laps>99 && laps!=999) || !spellings.Add(id))throw new FormatException("Invalid native racing definition.");
                        catalog.Add(id,row);
                    } else if(kind=="native_track_catalog_complete") {
                        if(!catalogStarted || catalogComplete || Number(row,"game_thread")!=thread || Number(row,"count")!=catalogCount || catalog.Count!=catalogCount)throw new FormatException("Native catalog did not complete.");
                        catalogComplete=true;
                    } else if(kind=="native_track_definition") {
                        if(!ready || Number(row,"game_thread")!=thread) throw new FormatException("Native lookup thread mismatch.");
                        string id=Text(row,"track_id"),level=Text(row,"native_level_name"); int status=Number(row,"status");
                        if(definitions.Count>=10000 || definitions.ContainsKey(id) || (status!=1 && status!=2) ||
                            (catalogStarted && !catalogComplete) || (!known.Contains(id) && !catalog.ContainsKey(id)) ||
                            (status==1 && !(TrackName.IsMatch(level) || (catalog.ContainsKey(id) && level==id))) || (status==2 && level.Length!=0)) throw new FormatException("Invalid native track definition.");
                        // Older exact-build captures did not record mode/lap
                        // metadata. Preserve unknown rather than deriving it
                        // from I/R suffixes. New captures must carry both.
                        bool modes=row.ContainsKey("native_mode_mask"),laps=row.ContainsKey("native_laps");
                        if(modes!=laps || (modes && (Number(row,"native_mode_mask")>31 || (Number(row,"native_laps")>99 && Number(row,"native_laps")!=999) ||
                            (status==2 && (Number(row,"native_mode_mask")!=0 || Number(row,"native_laps")!=0)))))
                            throw new FormatException("Invalid native track mode/lap evidence.");
                        definitions.Add(id,row); if(status==1) ++found; else ++empty;
                    } else if(kind=="native_track_queries_complete") {
                        if(!ready || (catalogStarted && !catalogComplete) || Number(row,"found")!=found || Number(row,"empty")!=empty ||
                            definitions.Count!=known.Count+catalog.Keys.Count(id=>!known.Contains(id))) throw new FormatException("Incomplete native definition counts.");
                        complete=true;
                    } else throw new FormatException("Failed or unknown native definition event.");
                }
            }
            if(!complete || report.Tracks.Any(track=>!definitions.ContainsKey(track.TrackId))) throw new FormatException("Native definition capture did not cover the catalog.");
            foreach(var item in catalog) {
                Dictionary<string,object> definition;
                if(!definitions.TryGetValue(item.Key,out definition) || Number(definition,"status")!=1 || Text(definition,"native_level_name")!=item.Key ||
                    !definition.ContainsKey("native_mode_mask") || Number(definition,"native_mode_mask")!=Number(item.Value,"native_mode_mask") ||
                    Number(definition,"native_laps")!=Number(item.Value,"native_laps"))throw new FormatException("Native table and exact lookup disagree.");
            }
            // Commit only after the entire capture has passed validation.
            var combined=report.Tracks.ToList();
            foreach(string id in catalog.Keys.Where(id=>!known.Contains(id)).OrderBy(id=>id,StringComparer.Ordinal)) {
                var resources=report.NativeWorldCandidates==null?null:report.NativeWorldCandidates.FirstOrDefault(t=>t.TrackId==id);
                var extra=resources??new TrackResourceEvidence {TrackId=id,BaseAlias=id,WorldMatch="not-audited",AiPathMatch="not-audited"};
                extra.ExternalCatalogMember=false;combined.Add(extra);
            }
            foreach(var track in combined) {
                var row=definitions[track.TrackId]; track.NativeLevelName=Text(row,"native_level_name");
                track.NativeModeMask=row.ContainsKey("native_mode_mask")?(int?)Number(row,"native_mode_mask"):null;
                track.NativeLaps=row.ContainsKey("native_laps")?(int?)Number(row,"native_laps"):null;
                track.NativeCatalogDiscovered=catalog.ContainsKey(track.TrackId);
                track.NativeDefinitionStatus=Number(row,"status")==1?"native-definition-present":
                    track.MirrorRequested || track.ReverseRequested?"variant-definition-unresolved":"no-native-definition-in-client";
            }
            report.Tracks=combined.ToArray();report.NativeCatalogComplete=catalogComplete;report.NativeCatalogRows=catalogRows;
            report.NativeCatalogDefinitions=catalog.Count;report.NativeCatalogNonRacing=catalogNonRacing;
            report.NativeDefinitionsOutsideExternalCatalog=combined.Count(t=>!t.ExternalCatalogMember);
            report.NativeDefinitionsComplete=true; report.NativeDefinitionsAvailable=found; report.NativeDefinitionsEmpty=empty;
        }
        static readonly Regex NativeAlias=new Regex("^[A-Za-z][A-Za-z0-9_]{0,95}$",RegexOptions.CultureInvariant);
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
            result.NativeWorldCandidates=worlds.Keys.Where(id=>NativeAlias.IsMatch(id) && !aliases.Contains(id)).OrderBy(id=>id,StringComparer.Ordinal).Select(id=>{
                string worldPath,aiPath;bool worldFound=Unique(worlds,id,out worldPath),aiFound=Unique(paths,id,out aiPath);
                return new TrackResourceEvidence {TrackId=id,BaseAlias=id,ExternalCatalogMember=false,WorldMatch=worldFound?"exact-base-world-record":"ambiguous-leaf-name",
                    WorldObjectPath=worldPath,AiPathMatch=aiFound?"exact-base-ai-record":"unmatched",AiPathObjectPath=aiPath};
            }).ToArray();
            return result;
        }
    }
}
