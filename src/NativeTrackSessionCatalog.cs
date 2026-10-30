using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;
using TwinKartLauncher.Gateway;

namespace TwinKartLauncher
{
    // Session-local metadata only. Catalog and exact alias lookup must agree
    // before replacing the launcher's report; no row implies loading/driving.
    public static class NativeTrackSessionCatalog
    {
        static readonly Regex Alias=new Regex("^[A-Za-z][A-Za-z0-9_]{0,95}$",RegexOptions.CultureInvariant);
        static TrackSupportReport ReadBase(string path,string client)
        {
            if(new FileInfo(path).Length>16*1024*1024)throw new InvalidDataException("Track report too large.");
            var report=new JavaScriptSerializer {MaxJsonLength=16*1024*1024}.Deserialize<TrackSupportReport>(File.ReadAllText(path,new UTF8Encoding(false,true)));
            if(report==null || !String.Equals(report.ClientSha256,client,StringComparison.OrdinalIgnoreCase) || report.Tracks==null || report.Tracks.Length<1 || report.Tracks.Length>10000 ||
                report.Tracks.Any(t=>t==null || t.TrackId==null || !Alias.IsMatch(t.TrackId)) || report.Tracks.Select(t=>t.TrackId).Distinct(StringComparer.Ordinal).Count()!=report.Tracks.Length)
                throw new InvalidDataException("Track report does not match this client.");
            return report;
        }
        public static void PrepareQueries(string reportPath,string client,string queryPath)
        {
            var report=ReadBase(reportPath,client);byte[] bytes=new UTF8Encoding(false).GetBytes(String.Join("\n",report.Tracks.Select(t=>t.TrackId))+"\n");
            using(var file=new FileStream(queryPath,FileMode.CreateNew,FileAccess.Write,FileShare.Read)){file.Write(bytes,0,bytes.Length);file.Flush(true);}
        }
        public static bool TryRefresh(string baseReportPath,string client,string capturePath,string outputPath,out TrackSupportReport refreshed)
        {
            refreshed=null;if(!File.Exists(capturePath))return false;
            string capture;
            using(var file=new FileStream(capturePath,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete)) {
                if(file.Length>4*1024*1024)throw new InvalidDataException("Native track capture too large.");
                using(var reader=new StreamReader(file,new UTF8Encoding(false,true)))capture=reader.ReadToEnd();
            }
            if(capture.Length>4*1024*1024)throw new InvalidDataException("Native track capture too large.");
            int newline=capture.LastIndexOf('\n');if(newline<0)return false;
            var json=new JavaScriptSerializer {MaxJsonLength=16384};var filtered=new StringBuilder();bool complete=false;
            using(var reader=new StringReader(capture.Substring(0,newline+1))) {
                string line;
                while((line=reader.ReadLine())!=null) {
                    if(String.IsNullOrWhiteSpace(line))continue;
                    var row=json.Deserialize<Dictionary<string,object>>(line);object raw;
                    string kind=row!=null && row.TryGetValue("event",out raw)?raw as string:null;
                    if(kind=="native_race_bridge_failed" || kind=="native_track_capture_failed")throw new InvalidDataException("Native track capture failed.");
                    if(kind=="native_race_bridge_ready" || kind=="native_track_lookup_waiting" || kind=="native_track_definition" ||
                        kind=="native_track_catalog_begin" || kind=="native_track_catalog_entry" || kind=="native_track_catalog_complete" || kind=="native_track_queries_complete")filtered.AppendLine(line);
                    if(kind=="native_track_queries_complete"){complete=true;break;}
                }
            }
            if(!complete)return false;
            var report=ReadBase(baseReportPath,client);report.NativeDefinitionsComplete=false;report.AllTracksDriven=false;
            foreach(var track in report.Tracks)track.DrivingVerified=false;
            TrackSupportAudit.AttachDefinitions(report,filtered.ToString());
            if(!report.NativeCatalogComplete)throw new InvalidDataException("Session did not capture the original native table.");
            var serializer=new JavaScriptSerializer {MaxJsonLength=16*1024*1024};byte[] output=new UTF8Encoding(false).GetBytes(serializer.Serialize(report));
            string temporary=outputPath+".new-"+Guid.NewGuid().ToString("N");
            using(var file=new FileStream(temporary,FileMode.CreateNew,FileAccess.Write,FileShare.Read)){file.Write(output,0,output.Length);file.Flush(true);}
            File.Move(temporary,outputPath);refreshed=report;return true;
        }
    }
}
