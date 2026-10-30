using System;
using System.IO;
using System.Linq;
using System.Web.Script.Serialization;
using TwinKartLauncher;
using TwinKartLauncher.Gateway;
static class NativeTrackSessionCatalogTests
{
    const string Hash="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    const string Catalog="{\"tracks\":[\"Test_R01\",\"Test_R01_Mirror\",\"Test_R01_rvs\",\"Test_R02\"]}";
    const string Ready="{\"event\":\"native_race_bridge_ready\",\"client_sha256\":\""+Hash+"\",\"game_thread\":42}\n";
    static string Entry(string id,int mask,int laps) {return "{\"event\":\"native_track_catalog_entry\",\"track_id\":\""+id+"\",\"native_mode_mask\":"+mask+",\"native_laps\":"+laps+",\"game_thread\":42}\n";}
    static string Definition(string id,int status,string level,int mask,int laps) {return "{\"event\":\"native_track_definition\",\"track_id\":\""+id+"\",\"status\":"+status+",\"native_level_name\":\""+level+"\",\"native_mode_mask\":"+mask+",\"native_laps\":"+laps+",\"game_thread\":42}\n";}
    static void Need(bool value,string message){if(!value)throw new Exception(message);}
    static int Main() {
        string scratch=Path.GetFullPath(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"test-scratch"));
        string folder=Path.Combine(scratch,"native-tracks-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(folder);
        try {
            string world="{\"event\":\"native_asset\",\"class\":\"World\",\"object_path\":\"/Game/Map/Test_R01.Test_R01\"}\n";
            string ai="{\"event\":\"native_asset\",\"class\":\"AIPathDataAsset\",\"object_path\":\"/Game/AI/Test_R01.Test_R01\"}\n";
            string inventory="{\"event\":\"asset_registry_begin\",\"client_sha256\":\""+Hash+"\"}\n"+world+world.Replace("Test_R01","New_R01")+ai+ai.Replace("Test_R01","New_R01")+
                "{\"event\":\"asset_registry_complete\",\"assets\":100,\"worlds\":2,\"ai_path_assets\":2}\n";
            var report=TrackSupportAudit.Read(inventory,Catalog,Hash);report.NativeDefinitionsComplete=true;
            string basePath=Path.Combine(folder,"base.json"),queries=Path.Combine(folder,"queries.txt"),capture=Path.Combine(folder,"capture.jsonl"),output=Path.Combine(folder,"refreshed.json");
            var json=new JavaScriptSerializer();string original=json.Serialize(report);File.WriteAllText(basePath,original);
            NativeTrackSessionCatalog.PrepareQueries(basePath,Hash,queries);
            Need(File.ReadAllLines(queries).SequenceEqual(new[]{"Test_R01","Test_R01_Mirror","Test_R01_rvs","Test_R02"}),"Seed queries changed track identities");
            string begin="{\"event\":\"native_track_catalog_begin\",\"rows\":3,\"count\":2,\"non_racing\":1,\"game_thread\":42}\n";
            string end="{\"event\":\"native_track_catalog_complete\",\"count\":2,\"game_thread\":42}\n";
            string definitions=Definition("Test_R01",1,"Test_R01",6,3)+Definition("Test_R01_Mirror",2,"",0,0)+Definition("Test_R01_rvs",2,"",0,0)+Definition("Test_R02",2,"",0,0)+Definition("New_R01",1,"New_R01",4,2);
            string complete="{\"event\":\"native_track_queries_complete\",\"found\":2,\"empty\":3}\n";
            string table=begin+Entry("Test_R01",6,3)+Entry("New_R01",4,2)+end;
            string log="{\"event\":\"native_local_ai_hooks_installed\"}\n"+Ready+table+definitions;
            TrackSupportReport refreshed;
            Need(!NativeTrackSessionCatalog.TryRefresh(basePath,Hash,capture,output,out refreshed),"Missing capture fabricated a report");
            File.WriteAllText(capture,log+complete.TrimEnd('\n'));
            Need(!NativeTrackSessionCatalog.TryRefresh(basePath,Hash,capture,output,out refreshed) && !File.Exists(output),"Partial final capture published");
            foreach(string bad in new[]{
                log.Replace(Entry("New_R01",4,2),Entry("New_R01",6,2))+complete,
                log.Replace(end,end.Replace("\"count\":2","\"count\":1"))+complete,
                log.Replace(Entry("New_R01",4,2),Entry("test_R01",4,2))+complete,
                log.Replace(Entry("New_R01",4,2),"")+complete,
                log.Replace(Definition("New_R01",1,"New_R01",4,2),Definition("Other_R01",1,"Other_R01",4,2))+complete,
                Ready+definitions+complete,
                Ready+"{\"event\":\"native_track_capture_failed\"}\n",
                Ready+"{\"event\":\"native_race_bridge_failed\"}\n"
            }) {
                File.WriteAllText(capture,bad);bool rejected=false;
                try{NativeTrackSessionCatalog.TryRefresh(basePath,Hash,capture,output,out refreshed);}catch(FormatException){rejected=true;}catch(InvalidDataException){rejected=true;}
                Need(rejected && !File.Exists(output) && File.ReadAllText(basePath)==original,"Invalid native discovery changed/published report");
            }
            File.WriteAllText(capture,log+complete+"{\"event\":\"native_local_ai_driver_setup_state\"}\n");
            Need(NativeTrackSessionCatalog.TryRefresh(basePath,Hash,capture,output,out refreshed),"Complete native discovery did not publish");
            var extra=refreshed.Tracks.Single(t=>t.TrackId=="New_R01");
            Need(refreshed.NativeCatalogComplete && refreshed.NativeCatalogRows==3 && refreshed.NativeCatalogDefinitions==2 && refreshed.NativeCatalogNonRacing==1 &&
                refreshed.CatalogVariants==4 && refreshed.Tracks.Length==5 && refreshed.NativeDefinitionsOutsideExternalCatalog==1 && refreshed.NativeDefinitionsAvailable==2,
                "Original table coverage conflated with external catalog coverage");
            Need(extra.NativeCatalogDiscovered && !extra.ExternalCatalogMember && extra.NativeModeMask==4 && extra.NativeLaps==2 &&
                extra.WorldObjectPath=="/Game/Map/New_R01.New_R01" && extra.AiPathObjectPath=="/Game/AI/New_R01.New_R01" && !extra.DrivingVerified && !refreshed.AllTracksDriven,
                "Discovered native identity/resources/modes lost or fabricated driving");
            Need(File.ReadAllText(basePath)==original && File.Exists(output),"Session refresh overwrote original report");
            string observed999=(log+complete).Replace(Entry("New_R01",4,2),Entry("New_R01",4,999))
                .Replace(Definition("New_R01",1,"New_R01",4,2),Definition("New_R01",1,"New_R01",4,999));
            File.WriteAllText(capture,observed999);
            Need(NativeTrackSessionCatalog.TryRefresh(basePath,Hash,capture,Path.Combine(folder,"observed999.json"),out refreshed) && refreshed.Tracks.Single(t=>t.TrackId=="New_R01").NativeLaps==999,
                "Observed native999 metadata was rejected or rewritten");
            bool mismatch=false;try{NativeTrackSessionCatalog.PrepareQueries(basePath,new string('b',64),Path.Combine(folder,"bad-queries"));}catch(InvalidDataException){mismatch=true;}
            foreach(int eventMask in new[]{8,16}) {
                string eventLog=(log+complete).Replace(Entry("New_R01",4,2),Entry("New_R01",eventMask,2))
                    .Replace(Definition("New_R01",1,"New_R01",4,2),Definition("New_R01",1,"New_R01",eventMask,2));
                File.WriteAllText(capture,eventLog);
                Need(NativeTrackSessionCatalog.TryRefresh(basePath,Hash,capture,Path.Combine(folder,"event-"+eventMask+".json"),out refreshed) &&
                    refreshed.Tracks.Single(t=>t.TrackId=="New_R01").NativeModeMask==eventMask && !refreshed.AllTracksDriven,
                    "Native event-map discovery rejected or changed its mode metadata");
            }
            Need(mismatch,"Different client's report seeded native lookup");
            Console.WriteLine("PASS native track session integration: seed queries, original table/exact lookup agreement, extra native world discovery, game-thread/build/count gates, partial/failed capture isolation, atomic private report and no fabricated loading/driving. No game started.");return 0;
        } catch(Exception error){Console.Error.WriteLine(error);return 1;}
        finally {if(Path.GetFullPath(folder).StartsWith(scratch+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase) && Directory.Exists(folder))Directory.Delete(folder,true);}
    }
}
