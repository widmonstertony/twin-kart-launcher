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
            Console.WriteLine("PASS native track inventory: build/count/completion checks, mirror and reverse separation, ambiguous names, failed/truncated captures and no invented driving acceptance."); return 0;
        } catch(Exception error) { Console.Error.WriteLine(error); return 1; }
    }
}
