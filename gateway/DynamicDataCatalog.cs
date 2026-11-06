using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Web.Script.Serialization;

namespace TwinKartLauncher.Gateway
{
    // Caller-supplied data is external to the source repository. This catalog
    // serves bounded UTF-8 text documents, not executable code or reply templates.
    public sealed class DynamicDataDocument
    {
        public string Name { get; set; }
        public string Content { get; set; }
    }

    public sealed class DynamicDataCatalog
    {
        sealed class Entry { public string Text, Hash; }
        readonly Dictionary<string, Entry> entries = new Dictionary<string, Entry>(StringComparer.Ordinal);
        public static bool ValidName(string name)
        {
            if (string.IsNullOrEmpty(name) || name.Length > 96 || name == "." || name == "..") return false;
            foreach (char c in name)
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                      (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return false;
            return true;
        }

        public static DynamicDataCatalog Load(string path,bool offlineEvents = false,bool chaserArena = false,bool hideRepaired = false,bool factoryRun = false)
        {
            var file = new FileInfo(path);
            if (!file.Exists || file.Length > 4 * 1024 * 1024) throw new ArgumentException("Invalid external data catalog size");
            var json = new JavaScriptSerializer { MaxJsonLength = 4 * 1024 * 1024, RecursionLimit = 8 };
            return new DynamicDataCatalog(json.Deserialize<DynamicDataDocument[]>(File.ReadAllText(file.FullName, new UTF8Encoding(false, true))),offlineEvents,chaserArena,hideRepaired,factoryRun);
        }

        static string OfflineEvents(bool chaserArena,bool hideRepaired,bool factoryRun)
        {
            // Original local schedule values, matching our independently
            // constructed native menu rows. No expired regional event data or
            // unsupported later-client mode numbers are enabled by this list.
            int[] modes={113,113,115,112,110,113,112,105,107,104,116,118};
            string[] profiles={"speed_solo_ghost","speed_solo_derek","speed_squad_skill","item_squad_miso_tiera","item_solo_brodi",
                "s062_speed_solo_dao","s062_item_squad_runner_attacker","s062_spitem_solo_skill","s062_spitem_squad_skill","s062_flag_skill","cubecubeitem_solo","cubecubeitem_squad"};
            var events=new List<object>();
            Action<int> addLocal=mode=>events.Add(new {
                id="TwinKart_LocalMode_"+mode.ToString(System.Globalization.CultureInfo.InvariantCulture),sortIndex=events.Count+1,gameType=1,
                gameMode=new {raceMode=mode,profile=""},openTime=new[]{"2000-01-01T00:00:00~2099-12-31T23:59:59"},
                timeList=new object[0],seq=0,noParty=PacketJsonCodec.LocalTeamSize(mode)==1,onlyObserverGame=false,matchPool="",dayOneLeagueEventId="",
                finishTimeMins=14,finalizeTimeMins=20,rewardTimeHours=72,req=new {preset=new object[0],engineType=0,text=""},
                customGameOption="",additionalOption="",majorityVictoryEventId=""
            });
            foreach(int mode in new[]{101,102,103})addLocal(mode);
            for(int i=0;i<modes.Length;++i)events.Add(new {
                id="TwinKart_"+profiles[i],sortIndex=events.Count+1,gameType=1,gameMode=new {raceMode=modes[i],profile=profiles[i]},
                openTime=new[]{"2000-01-01T00:00:00~2099-12-31T23:59:59"},timeList=new object[0],seq=0,
                noParty=PacketJsonCodec.LocalTeamSize(modes[i])==1,onlyObserverGame=false,matchPool="",dayOneLeagueEventId="",
                finishTimeMins=14,finalizeTimeMins=20,rewardTimeHours=72,req=new {preset=new object[0],engineType=0,text=""},
                customGameOption="",additionalOption="",majorityVictoryEventId=""
            });
            foreach(int mode in new[]{104,105,106,107,110,111,112,113,114,115,116,118})addLocal(mode);
            if(factoryRun)events.Add(new {id="TwinKart_FactoryRun",sortIndex=events.Count+1,gameType=1,
                gameMode=new {raceMode=100,profile=""},openTime=new[]{"2000-01-01T00:00:00~2099-12-31T23:59:59"},
                timeList=new object[0],seq=0,noParty=false,onlyObserverGame=false,matchPool="",dayOneLeagueEventId="",
                finishTimeMins=14,finalizeTimeMins=20,rewardTimeHours=72,req=new {preset=new object[0],engineType=0,text=""},
                customGameOption="",additionalOption="",majorityVictoryEventId=""});
            if(chaserArena)events.Add(new {id="TwinKart_Chaser",sortIndex=events.Count+1,gameType=1,
                gameMode=new {raceMode=108,profile=""},openTime=new[]{"2000-01-01T00:00:00~2099-12-31T23:59:59"},
                timeList=new object[0],seq=0,noParty=false,onlyObserverGame=false,matchPool="",dayOneLeagueEventId="",
                finishTimeMins=14,finalizeTimeMins=20,rewardTimeHours=72,req=new {preset=new object[0],engineType=0,text=""},
                customGameOption="",additionalOption="",majorityVictoryEventId=""});
            if(hideRepaired)events.Add(new {id="TwinKart_Hide_Repaired",sortIndex=events.Count+1,gameType=1,
                gameMode=new {raceMode=109,profile=""},openTime=new[]{"2000-01-01T00:00:00~2099-12-31T23:59:59"},
                timeList=new object[0],seq=0,noParty=true,onlyObserverGame=false,matchPool="",dayOneLeagueEventId="",
                finishTimeMins=14,finalizeTimeMins=20,rewardTimeHours=72,req=new {preset=new object[0],engineType=0,text=""},
                customGameOption="",additionalOption="",majorityVictoryEventId=""});
            events.Add(new {id="TwinKart_FreeDrive",sortIndex=events.Count+1,gameType=1,
                gameMode=new {raceMode=1,profile=NativeRaceOptions.FreeDriveProfile},openTime=new[]{"2000-01-01T00:00:00~2099-12-31T23:59:59"},
                timeList=new object[0],seq=0,noParty=true,onlyObserverGame=false,matchPool="",dayOneLeagueEventId="",
                finishTimeMins=14,finalizeTimeMins=20,rewardTimeHours=72,req=new {preset=new object[0],engineType=0,text=""},
                customGameOption="",additionalOption="",majorityVictoryEventId=""});
            return new JavaScriptSerializer().Serialize(new {events=events});
        }
        public DynamicDataCatalog(DynamicDataDocument[] documents,bool offlineEvents = false,bool chaserArena = false,bool hideRepaired = false,bool factoryRun = false)
        {
            if (documents == null || documents.Length > 512) throw new ArgumentException("Invalid document count");
            var selected=new List<DynamicDataDocument>();bool hasModeLab=false;
            foreach(var document in documents) {
                if(offlineEvents && document!=null && document.Name=="ModeLabSet" && document.Content!=null) {
                    selected.Add(new DynamicDataDocument {Name=document.Name,Content=OfflineEvents(chaserArena,hideRepaired,factoryRun)});hasModeLab=true;
                } else selected.Add(document);
            }
            if(offlineEvents && !hasModeLab)selected.Add(new DynamicDataDocument {Name="ModeLabSet",Content=OfflineEvents(chaserArena,hideRepaired,factoryRun)});
            if(selected.Count>512)throw new ArgumentException("Invalid document count");
            long total = 0;
            foreach (var document in selected)
            {
                if (document == null || !ValidName(document.Name) || document.Content == null || entries.ContainsKey(document.Name))
                    throw new ArgumentException("Invalid or duplicate dynamic data name");
                byte[] bytes = new UTF8Encoding(false, true).GetBytes(document.Content);
                total += bytes.Length;
                if (bytes.Length > 600 * 1024 || total > 3 * 1024 * 1024)
                    throw new ArgumentException("Dynamic data exceeds limits");
                string hash;
                using (var sha = SHA256.Create()) hash = BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-", "").ToLowerInvariant();
                entries.Add(document.Name, new Entry { Text = document.Content, Hash = hash });
            }
        }

        public bool TryReply(string name, int requestId, out Dictionary<string, object> response)
        {
            Entry entry; response = null;
            if (!ValidName(name) || !entries.TryGetValue(name, out entry)) return false;
            response = new Dictionary<string, object> {
                { "_type", "PrGetPublicDynData" }, { "reqId", requestId },
                { "result", new Dictionary<string, object> { { "code", 0 } } }, { "desc", "" },
                { "name", name }, { "hash", entry.Hash }, { "hasData", true },
                { "isBinary", false }, { "data", entry.Text }, { "isCompressed", false },
                { "uncompressedSize", 0 }, { "compressedData", "" }
            };
            return true;
        }
    }
}
