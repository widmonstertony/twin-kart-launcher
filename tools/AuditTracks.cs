using System;
using System.IO;
using System.Text;
using System.Web.Script.Serialization;
using TwinKartLauncher.Gateway;
static class AuditTracks
{
    static int Main(string[] args) {
        try {
            if(args.Length!=4 && args.Length!=5) throw new ArgumentException("Usage: TwinKartTrackAudit.exe <native-inventory.jsonl> <external-tracks.json> <expected-client-sha256> <new-report.json> [native-definition-lookup.jsonl]");
            foreach(string input in new[]{args[0],args[1]}) if(new FileInfo(input).Length>16*1024*1024) throw new ArgumentException("Input too large.");
            var report=TrackSupportAudit.Read(File.ReadAllText(args[0],new UTF8Encoding(false,true)),File.ReadAllText(args[1],new UTF8Encoding(false,true)),args[2]);
            if(args.Length==5) {
                if(new FileInfo(args[4]).Length>1024*1024) throw new ArgumentException("Definition capture too large.");
                TrackSupportAudit.AttachDefinitions(report,File.ReadAllText(args[4],new UTF8Encoding(false,true)));
            }
            string text=new JavaScriptSerializer { MaxJsonLength=16*1024*1024 }.Serialize(report);
            using(var output=new FileStream(args[3],FileMode.CreateNew,FileAccess.Write,FileShare.Read)) {
                byte[] bytes=Encoding.UTF8.GetBytes(text); output.Write(bytes,0,bytes.Length); output.Flush(true);
            }
            Console.WriteLine("Native world records: {0}; AI path records: {1}; catalog base aliases: {2}; exact main-world matches: {3}; reverse candidates: {4}; unmatched aliases: {5}. Driving remains unverified.",report.NativeWorldAssets,report.NativeAiPathAssets,report.CatalogBaseAliases,report.ExactMainWorldBaseAliases,report.ReverseWorldCandidates,report.UnmatchedMainWorldBaseAliases);
            return 0;
        } catch(Exception error) { Console.Error.WriteLine(error.Message); return 1; }
    }
}
