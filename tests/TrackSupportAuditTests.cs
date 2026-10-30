using System;
using TwinKartLauncher.Gateway;
static class TrackSupportAuditTests
{
    const string Hash="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    const string Begin="{\"event\":\"asset_registry_begin\",\"client_sha256\":\""+Hash+"\"}\n";
    const string World="{\"event\":\"native_asset\",\"class\":\"World\",\"object_path\":\"/Game/Map/Test_R01.Test_R01\"}\n";
    const string Ai="{\"event\":\"native_asset\",\"class\":\"AIPathDataAsset\",\"object_path\":\"/Game/AI/Test_R01.Test_R01\"}\n";
    const string End="{\"event\":\"asset_registry_complete\",\"assets\":100,\"worlds\":1,\"ai_path_assets\":1,\"all_tracks_driven\":true}\n";
    const string Catalog="{\"tracks\":[\"Test_R01\",\"Test_R01_Mirror\",\"Test_R01_rvs\",\"Test_R02\"]}";
    static string Definition(string id,int status,string name) { return "{\"event\":\"native_track_definition\",\"track_id\":\""+id+"\",\"status\":"+status+",\"native_level_name\":\""+name+"\",\"game_thread\":42}\n"; }
    static void Need(bool value,string message) { if(!value) throw new Exception(message); }
    static void Reject(string log,string hash) { bool rejected=false; try { TrackSupportAudit.Read(log,Catalog,hash); } catch(FormatException) { rejected=true; } Need(rejected,"Invalid native inventory accepted"); }
    static int Main() {
        try {
            var report=TrackSupportAudit.Read(Begin+World+Ai+End,Catalog,Hash);
            Need(report.CatalogVariants==4 && report.CatalogBaseAliases==3 && report.ExactMainWorldBaseAliases==1 && report.ReverseWorldCandidates==1 && report.UnmatchedMainWorldBaseAliases==1,"Variant, base and reverse evidence conflated");
            Need(report.Tracks[1].MirrorRequested && report.Tracks[2].WorldMatch=="reverse-base-candidate" && !report.AllTracksDriven && !report.Tracks[0].DrivingVerified,"Resource records falsely established driving or reverse support");
            Reject(Begin+World+Ai,Hash); Reject(Begin+World+Ai+End,new string('b',64));
            Reject(Begin+World+Ai+End+World,Hash); Reject(World+Ai+End,Hash);
            Reject(Begin+World+Ai+End.Replace("\"worlds\":1","\"worlds\":2"),Hash);
            Reject(Begin+World+Ai+"{\"event\":\"asset_registry_failed\"}\n",Hash);
            string duplicate=World.Replace("/Game/Map/","/Game/Other/");
            var ambiguous=TrackSupportAudit.Read(Begin+World+duplicate+Ai+End.Replace("\"worlds\":1","\"worlds\":2"),Catalog,Hash);
            Need(ambiguous.Tracks[0].WorldMatch=="ambiguous-leaf-name" && ambiguous.ExactMainWorldBaseAliases==0,"Ambiguous package leaf silently selected");
            string definitions="{\"event\":\"native_race_bridge_ready\",\"client_sha256\":\""+Hash+"\",\"game_thread\":42}\n"+
                Definition("Test_R01",1,"Test_R01")+Definition("Test_R01_Mirror",2,"")+Definition("Test_R01_rvs",2,"")+Definition("Test_R02",2,"")+
                "{\"event\":\"native_track_queries_complete\",\"found\":1,\"empty\":3}\n";
            TrackSupportAudit.AttachDefinitions(report,definitions);
            Need(report.NativeDefinitionsComplete && report.NativeDefinitionsAvailable==1 && report.Tracks[0].NativeLevelName=="Test_R01" &&
                report.Tracks[1].NativeDefinitionStatus=="variant-definition-unresolved" && report.Tracks[3].NativeDefinitionStatus=="no-native-definition-in-client" && !report.AllTracksDriven,"Native lookup evidence mislabeled transforms or driving");
            Need(!report.Tracks[0].NativeModeMask.HasValue && !report.Tracks[0].NativeLaps.HasValue,"Historical capture fabricated native modes");
            string modes=definitions.Replace("\"game_thread\":42}","\"game_thread\":42,\"native_mode_mask\":0,\"native_laps\":0}")
                .Replace("\"native_level_name\":\"Test_R01\",\"game_thread\":42,\"native_mode_mask\":0,\"native_laps\":0","\"native_level_name\":\"Test_R01\",\"game_thread\":42,\"native_mode_mask\":6,\"native_laps\":3");
            var modeReport=TrackSupportAudit.Read(Begin+World+Ai+End,Catalog,Hash);TrackSupportAudit.AttachDefinitions(modeReport,modes);
            Need(modeReport.Tracks[0].NativeModeMask==6 && modeReport.Tracks[0].NativeLaps==3 && modeReport.Tracks[1].NativeModeMask==0,"Native mode/lap metadata lost");
            foreach(string bad in new[]{modes.Replace("\"native_mode_mask\":6","\"native_mode_mask\":32"),modes.Replace("\"native_laps\":3","\"native_laps\":100"),
                modes.Replace(",\"native_laps\":3",""),modes.Replace("\"native_mode_mask\":0","\"native_mode_mask\":2")}) {
                var clean=TrackSupportAudit.Read(Begin+World+Ai+End,Catalog,Hash);bool rejected=false;
                try{TrackSupportAudit.AttachDefinitions(clean,bad);}catch(FormatException){rejected=true;}
                Need(rejected && !clean.NativeDefinitionsComplete && !clean.Tracks[0].NativeModeMask.HasValue,"Malformed native mode capture partially committed");
            }
            foreach(string bad in new[]{definitions.Replace("\"game_thread\":42}","\"game_thread\":43}").Replace("\"game_thread\":43}\n{\"event\":\"native_track_definition\"","\"game_thread\":42}\n{\"event\":\"native_track_definition\""),
                definitions.Replace("\"empty\":3","\"empty\":2"),definitions.Replace(Definition("Test_R02",2,""),Definition("Test_R01",2,"")),definitions.Replace("\"status\":2,\"native_level_name\":\"\"","\"status\":2,\"native_level_name\":\"Other\""),definitions.Replace(Hash,new string('b',64))}) {
                var clean=TrackSupportAudit.Read(Begin+World+Ai+End,Catalog,Hash); bool invalid=false;
                try { TrackSupportAudit.AttachDefinitions(clean,bad); } catch(FormatException) { invalid=true; }
                Need(invalid && !clean.NativeDefinitionsComplete && clean.Tracks[0].NativeDefinitionStatus==null,"Invalid definition capture partially committed");
            }
            Console.WriteLine("PASS native track inventory: build/count/completion checks, mirror and reverse separation, ambiguous names, failed/truncated captures and no invented driving acceptance."); return 0;
        } catch(Exception error) { Console.Error.WriteLine(error); return 1; }
    }
}
