using System;
using System.Collections.Generic;
using System.Globalization;
using System.Security.Cryptography;
using System.Text;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;

namespace TwinKartLauncher.Gateway
{
    // Original wire adapter for the existing first-party room state model.
    // Native room controls feed the same local loading bridge as lobby buttons.
    internal sealed class OfflineRoomService
    {
        sealed class AiCharacter
        {
            internal string Character;
            internal string Costume;
        }
        readonly RoomState room = new RoomState();
        readonly string account;
        readonly Func<RoomState,string> startRoom;
        readonly List<AiCharacter> aiCharacters = new List<AiCharacter>();
        readonly List<string> aiKarts = new List<string>();
        internal OfflineRoomService(string localAccount, Func<RoomState,string> start = null,
            string unlockCatalog = null, string vehicleParts = null, string nativeParameters = null, string appearanceAvailability = null)
        {
            account = localAccount; startRoom = start;
            LoadAiAppearances(unlockCatalog,vehicleParts,nativeParameters,appearanceAvailability);
        }
        internal static bool Handles(string type)
        {
            switch (type)
            {
                case "PqCreateCustomGame": case "PqJoinCustomGame": case "PqAddCustomGameAiMember":
                case "PqKickCustomGameMember": case "PqChangeCustomGameSetting": case "PqLeaveCustomGame":
                case "PqAbortCustomGame": case "PqPlayCustomGame": case "PqCustomGameSetReady":
                case "PqRegisterMatch": case "PqPreStartTrackSel": return true;
                default: return false;
            }
        }
        static Dictionary<string,object> Object(params object[] fields)
        {
            var value = new Dictionary<string,object>(StringComparer.Ordinal);
            for (int i=0; i<fields.Length; i+=2) value.Add((string)fields[i], fields[i+1]);
            return value;
        }
        static object[] Sequence(object value)
        {
            var array=value as object[];
            if(array!=null)return array;
            var list=value as System.Collections.IList;
            if(list==null)return null;
            var copy=new object[list.Count];
            list.CopyTo(copy,0);
            return copy;
        }
        static object[] MatchQueue(object value)
        {
            var direct=Sequence(value);
            if(direct!=null)return direct;
            var envelope=value as IDictionary<string,object>;
            object gameModes;
            if(envelope==null || !envelope.TryGetValue("gameModes",out gameModes))return null;
            return Sequence(gameModes);
        }
        void LoadAiAppearances(string unlockCatalog,string vehicleParts,string nativeParameters,string appearanceAvailability)
        {
            if(String.IsNullOrEmpty(unlockCatalog) || String.IsNullOrEmpty(vehicleParts))return;
            var unlock=ProfileJson.Parse(unlockCatalog);
            var parts=ProfileJson.Parse(vehicleParts);
            HashSet<string> nativeVehicles=null;
            if(!String.IsNullOrEmpty(nativeParameters))
            {
                object rawNativeVehicles;
                var native=ProfileJson.Parse(nativeParameters);
                var nativeMap=native.TryGetValue("vehicles",out rawNativeVehicles) ? rawNativeVehicles as Dictionary<string,object> : null;
                if(nativeMap==null)return;
                nativeVehicles=new HashSet<string>(nativeMap.Keys,StringComparer.Ordinal);
            }
            object rawItems,rawBodies,rawCharacters,rawVehicles;
            if(!unlock.TryGetValue("items",out rawItems) || !unlock.TryGetValue("kartBodies",out rawBodies) ||
               !parts.TryGetValue("characters",out rawCharacters) || !parts.TryGetValue("vehicles",out rawVehicles))return;
            var known=new HashSet<string>(StringComparer.Ordinal);
            var itemRows=Sequence(rawItems);
            if(itemRows!=null)foreach(var row in itemRows)
            {
                var item=row as Dictionary<string,object>;object id;
                if(item!=null && item.TryGetValue("id",out id) && id is string)known.Add((string)id);
            }
            var bodies=new HashSet<string>(StringComparer.Ordinal);
            var bodyRows=Sequence(rawBodies);
            if(bodyRows!=null)foreach(var row in bodyRows)if(row is string)bodies.Add((string)row);
            HashSet<string> nativeCharacterPairs=null,availableBodies=null;
            if(!String.IsNullOrEmpty(appearanceAvailability))
            {
                var availability=ProfileJson.Parse(appearanceAvailability);object rawCharacterRows,rawAvailableBodies;
                var characterRows=availability.TryGetValue("characters",out rawCharacterRows)?Sequence(rawCharacterRows):null;
                var availableBodyRows=availability.TryGetValue("bodies",out rawAvailableBodies)?Sequence(rawAvailableBodies):null;
                if(characterRows==null || availableBodyRows==null)throw new FormatException("Native appearance evidence is incomplete.");
                nativeCharacterPairs=new HashSet<string>(StringComparer.Ordinal);var seenCharacters=new HashSet<string>(StringComparer.Ordinal);
                foreach(var raw in characterRows)
                {
                    var row=raw as Dictionary<string,object>;object character,costume,status;
                    if(row==null || !row.TryGetValue("character",out character) || !(character is string) || !known.Contains((string)character) ||
                       !row.TryGetValue("costume",out costume) || !(costume is string) || !known.Contains((string)costume) ||
                       !row.TryGetValue("registered",out status) || !(status is int) || ((int)status!=0 && (int)status!=1) || !seenCharacters.Add((string)character))
                        throw new FormatException("Invalid native character evidence.");
                    if((int)status==1)nativeCharacterPairs.Add((string)character+"\n"+(string)costume);
                }
                availableBodies=new HashSet<string>(StringComparer.Ordinal);var seenBodies=new HashSet<string>(StringComparer.Ordinal);
                foreach(var raw in availableBodyRows)
                {
                    var row=raw as Dictionary<string,object>;object id,status;
                    if(row==null || !row.TryGetValue("id",out id) || !(id is string) || !bodies.Contains((string)id) ||
                       !row.TryGetValue("registered",out status) || !(status is int) || ((int)status!=0 && (int)status!=1) || !seenBodies.Add((string)id))
                        throw new FormatException("Invalid native kart evidence.");
                    if((int)status==1)availableBodies.Add((string)id);
                }
                if(nativeCharacterPairs.Count<8 || availableBodies.Count<8)throw new FormatException("Insufficient native room-preview appearances.");
            }
            var characters=rawCharacters as Dictionary<string,object>;
            if(characters!=null)foreach(var pair in characters.OrderBy(value=>value.Key,StringComparer.Ordinal))
            {
                if(!known.Contains(pair.Key))continue;
                var outfit=pair.Value as Dictionary<string,object>;object costume;
                if(outfit==null || !outfit.TryGetValue("Costume",out costume) || !(costume is string) || !known.Contains((string)costume) ||
                   (nativeCharacterPairs!=null && !nativeCharacterPairs.Contains(pair.Key+"\n"+(string)costume)))continue;
                aiCharacters.Add(new AiCharacter { Character=pair.Key, Costume=(string)costume });
            }
            var vehicles=rawVehicles as Dictionary<string,object>;
            if(vehicles!=null)foreach(var pair in vehicles.OrderBy(value=>value.Key,StringComparer.Ordinal))
                // Unlock/reference catalogs can contain historical IDs whose
                // body art is not instantiable by this PC client.  The native
                // parameter table is extracted from the supported client and
                // is therefore the minimum room-preview availability gate.
                if(known.Contains(pair.Key) && bodies.Contains(pair.Key) &&
                   (nativeVehicles==null || nativeVehicles.Contains(pair.Key)) &&
                   (availableBodies==null || availableBodies.Contains(pair.Key)))aiKarts.Add(pair.Key);
        }
        int AiCatalogIndex(string kind,int slot,int count)
        {
            using(var hash=SHA256.Create())
            {
                byte[] value=hash.ComputeHash(Encoding.UTF8.GetBytes(room.SessionId+":"+kind));
                uint offset=BitConverter.ToUInt32(value,0);
                return (int)((offset+(uint)(slot-1))%(uint)count);
            }
        }
        static Dictionary<string,object> Setting(CsRaceGameContract value)
        {
            var mode = Object("raceMode", value.GameMode.RaceMode);
            if (value.GameMode.Profile != null) mode.Add("profile", value.GameMode.Profile);
            var result = Object("_type", "CsRaceGame", "track", value.Track, "gameMode", mode);
            if (value.Lap.HasValue) result.Add("lap", value.Lap.Value);
            if (value.LevelOption != null) result.Add("levelOption", value.LevelOption);
            if (value.CustomGameOption != null) result.Add("customGameOption", value.CustomGameOption);
            return result;
        }
        string AiIdentity(int slot)
        {
            using (var hash = SHA256.Create())
            {
                byte[] bytes = hash.ComputeHash(Encoding.UTF8.GetBytes(room.SessionId + ":" + slot.ToString(CultureInfo.InvariantCulture)));
                ulong id = (BitConverter.ToUInt64(bytes, 0) & 0x7fffffffffffffffUL) + 1;
                string identity = id.ToString(CultureInfo.InvariantCulture);
                // Keep the local AI and driver identities distinct even in a
                // synthetic test account or an unlikely hash collision.
                return identity == account ? (id + 1).ToString(CultureInfo.InvariantCulture) : identity;
            }
        }
        Dictionary<string,object> Member(int slot, Dictionary<string,object> profile, Dictionary<string,object> login)
        {
            bool ai = slot != 0;
            var items = ProfileJson.Object(ProfileJson.Object(profile,"itemPresetOn"),"itemMap");
            var avatar = login.ContainsKey("profile") ? ProfileJson.Object(ProfileJson.Copy(login["profile"])) : Object();
            string identity = ai ? AiIdentity(slot) : account;
            object name;
            string displayName = ai ? "AI " + slot.ToString(CultureInfo.InvariantCulture) : profile.TryGetValue("riderName",out name) ? name as string : "Local Driver";
            avatar["riderName"] = displayName ?? "Local Driver";
            avatar["platformProfileName"] = avatar["riderName"];
            avatar["platformProfileId"] = identity;
            object character, kart, costume;
            items.TryGetValue("Character",out character); items.TryGetValue("Kart",out kart); items.TryGetValue("Costume",out costume);
            if(ai && aiCharacters.Count!=0)
            {
                var selected=aiCharacters[AiCatalogIndex("character",slot,aiCharacters.Count)];
                character=selected.Character;costume=selected.Costume;
            }
            if(ai && aiKarts.Count!=0)kart=aiKarts[AiCatalogIndex("kart",slot,aiKarts.Count)];
            return Object("accountId",identity,"profile",avatar,"slot",slot,"isSpectator",!ai && room.ObserverMode,
                "joinId",slot+1,"chId",character??"","kartId",kart??"","costume",costume??"",
                "state",1,"readyState",ai ? 0 : room.ReadyState,"crossPlay",false,"ai",ai);
        }
        Dictionary<string,object> Snapshot(Dictionary<string,object> profile, Dictionary<string,object> login)
        {
            var members = new List<object> { Member(0,profile,login) };
            foreach (int slot in room.AiSlots) members.Add(Member(slot,profile,login));
            var info = Object("bossAccountId",account,"gameKey","","hideGameKey",room.HideGameKey,
                "session",Setting(room.Setting),"maxMemberCount",8,"members",members,
                "observerMode",room.ObserverMode,"observers",new object[0],
                "gameOption",Object("hideKart",false,"hideCharacter",false),
                "gameData",Object("player",Object(),"team",new object[0],"raceResult",new object[0],"adUrl",""));
            return Object("_type","PcCustomGame","sessionId",room.SessionId,"infoValid",true,"info",info);
        }
        internal ProfileResult Handle(IDictionary<string,object> request, int id, Dictionary<string,object> profile, Dictionary<string,object> login)
        {
            // Validate source objects before mutating the room model.
            ProfileJson.Object(ProfileJson.Object(profile,"itemPresetOn"),"itemMap");
            if (login.ContainsKey("profile")) ProfileJson.Object(login["profile"]);
            string type = request["_type"] as string;
            if(type=="PqRegisterMatch") return OpenNativeRaceSetup(request,id,profile);
            if(type=="PqPreStartTrackSel")
            {
                // The original packet has only _type/reqId.  A successful,
                // side-effect-free reply lets the client begin its own selected
                // track preload before Play, instead of taking the failure path.
                return new ProfileResult { Response=Object("_type","PrPacket","reqId",id,
                    "result",Object("code",room.Joined?0:10001),"desc",room.Joined?"":"Join a local room first.") };
            }
            bool exit=type=="PqLeaveCustomGame" || type=="PqAbortCustomGame";
            Dictionary<string,object> invalidatedRoom=null;
            if(exit && room.SessionId!=null && room.Setting!=null)
            {
                // Keep the last complete native shape long enough to publish
                // a final invalidation after the state model clears it.
                invalidatedRoom=Snapshot(profile,login);
                invalidatedRoom["infoValid"]=false;
            }
            GatewayTransition transition = room.Apply(ProfileJson.Write(request));
            bool start = type == "PqPlayCustomGame";
            var reply = new ProfileResult { Response = Object("_type","PrPacket","reqId",id,
                "result",Object("code",transition.Accepted && !start ? 0 : 10001),"desc","") };
            if (!transition.Accepted) return reply;
            if (start)
            {
                string failure;
                try { failure = startRoom == null ? "Native local loading bridge is unavailable in this session." : startRoom(room); }
                catch(InvalidDataException) { failure="本次赛道目录与客户端不匹配，请重新启动本地会话。"; }
                catch(IOException) { failure="本次比赛目录或请求文件不可用，请返回大厅后重试。"; }
                catch(FormatException) { failure="本次赛道目录无效，请重新启动本地会话。"; }
                catch(ArgumentException) { failure="本次比赛设置无效。"; }
                reply.Response["result"] = Object("code",failure == null ? 0 : 10001);
                reply.Response["desc"] = failure ?? "Local loading requested.";
                return reply;
            }
            if (exit)
            {
                if(invalidatedRoom!=null)reply.Notices.Add(invalidatedRoom);
                return reply;
            }
            if (type == "PqCreateCustomGame") reply.Notices.Add(Object("_type","PcPartyCreateJoinCustomGame"));
            reply.Notices.Add(Snapshot(profile,login));
            return reply;
        }

        ProfileResult OpenNativeRaceSetup(IDictionary<string,object> request,int id,Dictionary<string,object> profile)
        {
            var response=Object("_type","PrRegisterMatch","reqId",id,"result",Object("code",10001),"desc","");
            var reply=new ProfileResult { Response=response };
            try {
                object rawQueue;
                if(!request.TryGetValue("queue",out rawQueue))
                    throw new ArgumentException("Missing native match queue.");
                // The bottom-right lobby tile puts the currently selected
                // solo/duo/squad mode in this request.  Some client builds do
                // not persist PqSetQuickMatchData first, so the live queue is
                // the authoritative selection for this transition.
                // Current Drift clients wrap the selected lobby tile as
                // queue.gameModes.  Earlier captures and the protocol fixture
                // use the list directly, so accept both original wire shapes.
                var queue=MatchQueue(rawQueue);
                if(queue==null)throw new ArgumentException("Missing native match queue.");
                if(queue.Length!=1)throw new ArgumentException("Select exactly one race mode.");
                var selected=ProfileJson.Object(queue[0]);object rawMode;
                if(!selected.TryGetValue("raceMode",out rawMode) || !(rawMode is int) || !PacketJsonCodec.LocalRaceMode((int)rawMode))
                    throw new ArgumentException("Unsupported race mode.");
                room.PrepareQuickRoom((int)rawMode,"Roulette_A01");
                response["result"]=Object("code",0);response["mmTicket"]=room.SessionId;
                object latency;response["useDefaultLatencyInfo"]=request.TryGetValue("useDefaultLatencyInfo",out latency) && latency is bool ? latency : true;
                // The original client follows this notice with its native join
                // request, then renders its own map/team/AI setup and loading UI.
                reply.Notices.Add(Object("_type","PcPartyCreateJoinCustomGame"));
            } catch(ArgumentException error) { response["desc"]=error.Message; }
              catch(FormatException error) { response["desc"]=error.Message; }
            return reply;
        }
    }

    // Session-owned bridge, created by the launcher host only. UI requests can
    // choose gameplay values, never a file path, executable or engine command.
    internal sealed class NativeRoomLaunch
    {
        sealed class Track { internal string Id; internal int Mask,Laps; }
        readonly string control,client,reportPath;
        readonly List<Track> tracks = new List<Track>();
        readonly object gate = new object();
        string lastIdentity;
        internal NativeRoomLaunch(string controlPath, string reportPath, string clientSha256)
        {
            if(!Regex.IsMatch(clientSha256??"","^[a-f0-9]{64}$")) throw new ArgumentException("Invalid client fingerprint.");
            control=Path.GetFullPath(controlPath);client=clientSha256;this.reportPath=Path.GetFullPath(reportPath);
        }
        void ReadTracks()
        {
            var report=ProfileJson.Parse(ProfileJson.Read(reportPath));
            if(!Object.Equals(report["ClientSha256"],client) || !Object.Equals(report["NativeDefinitionsComplete"],true))
                throw new InvalidDataException("Native track report differs from this client.");
            var ids=new HashSet<string>(StringComparer.Ordinal);var available=new List<Track>();
            foreach(var item in (object[])report["Tracks"]) {
                var row=ProfileJson.Object(item); object raw;
                string id=ProfileJson.Text(row,"TrackId");
                if(!Regex.IsMatch(id,"^[A-Za-z][A-Za-z0-9_]{0,95}$") || !ids.Add(id)) throw new FormatException("Invalid native track identity.");
                if(!Object.Equals(row["NativeDefinitionStatus"],"native-definition-present") ||
                    !Object.Equals(row["WorldMatch"],"exact-base-world-record")) continue;
                if(!row.TryGetValue("NativeModeMask",out raw) || !(raw is int) || (int)raw<0 || (int)raw>31) continue;
                int mask=(int)raw;
                // Circuit AI requires its exact path. Flag uses native arena navigation.
                if(!Object.Equals(row["AiPathMatch"],"exact-base-ai-record")) mask &= 16;
                int laps=row.TryGetValue("NativeLaps",out raw) && raw is int ? (int)raw : 0;
                if(mask!=0) available.Add(new Track {Id=id,Mask=mask,Laps=laps});
            }
            if(available.Count==0) throw new InvalidDataException("No native tracks available.");
            tracks.AddRange(available);
        }
        internal static NativeRoomLaunch FromEnvironment()
        {
            string path=Environment.GetEnvironmentVariable("TWIN_KART_ROOM_RACE_CONTROL");
            return String.IsNullOrEmpty(path)?null:new NativeRoomLaunch(path,
                Environment.GetEnvironmentVariable("TWIN_KART_ROOM_TRACK_REPORT"),
                Environment.GetEnvironmentVariable("TWIN_KART_ROOM_CLIENT_SHA256"));
        }
        static bool Supports(Track track,int mode)
        { return PacketJsonCodec.LocalRaceMode(mode) && (track.Mask & (1 << (mode==104?4:mode>=105?3:mode<=3 || mode>=101 && mode<=103?2:1)))!=0; }
        internal string Start(RoomState room)
        {
            lock(gate) {
                if(room==null || !room.Joined || room.ObserverMode) return "请先进入驾驶员房间。";
                if(!File.Exists(control+".ready") || File.ReadAllText(control+".ready").Trim()!=client)
                    return "本地比赛接口尚未就绪，请在大厅稍候。";
                // Session catalog is emitted after native startup; an old
                // baseline may legitimately lack mode/lap metadata entirely.
                if(tracks.Count==0) {
                    if(!File.Exists(reportPath))return "本次赛道目录正在准备，请在大厅稍候。";
                    ReadTracks();
                }
                // Repeated clicks cannot enqueue overlapping loading transitions.
                if(lastIdentity!=null && !File.Exists(control+"."+lastIdentity+".result")) return "上一场开赛请求正在处理。";
                int mode=room.RaceMode,ai=room.AiCount;
                int group=mode>=105?mode-105:mode>=101?mode-101:(mode-1)%3;
                int size=mode==104?4:group==0?1:group==1?2:4;
                if(!PacketJsonCodec.LocalRaceMode(mode) || ai<1 || ai>7 ||
                    (size>1 && (ai+1<size*2 || (ai+1)%size!=0))) return "请在房间添加 AI：双人组需 3、5 或 7 个，四人组需 7 个，个人赛需 1–7 个。";
                var setting=room.Setting;
                var eligible=tracks.Where(t=>Supports(t,mode)).ToArray();
                // Roulette_A01 is the original random-map choice, not an alias
                // for a missing explicit map. Only this choice may draw a map.
                Track track=eligible.FirstOrDefault(t=>t.Id==setting.Track);
                if(setting.Track=="Roulette_A01" && eligible.Length>0) {
                    byte[] draw=new byte[4];using(var rng=RandomNumberGenerator.Create())rng.GetBytes(draw);
                    track=eligible[BitConverter.ToUInt32(draw,0)%(uint)eligible.Length];
                }
                if(track==null) return "所选地图在本客户端缺少该模式的完整资源，请在游戏内重新选图。";
                int laps=setting.Lap??999;
                if(laps==999 || laps==0) laps=track.Laps>=1 && track.Laps<=9?track.Laps:3;
                if(laps<1 || laps>9) return "圈数需要为 1–9 或原厂默认值。";
                string identity=Guid.NewGuid().ToString("N");
                string text=String.Format(CultureInfo.InvariantCulture,"TKRACE1 {0} {1} {2} {3} {4}\n",identity,track.Id,mode,ai,laps);
                string temporary=control+"."+identity+".pending";
                try {
                    using(var stream=new FileStream(temporary,FileMode.CreateNew,FileAccess.Write,FileShare.None)) {
                        byte[] bytes=Encoding.ASCII.GetBytes(text);stream.Write(bytes,0,bytes.Length);stream.Flush(true);
                    }
                    if(File.Exists(control))File.Replace(temporary,control,null);else File.Move(temporary,control);
                    lastIdentity=identity;
                    return null; // Accepted loading intent; no host packet/result fabricated.
                } finally {if(File.Exists(temporary))File.Delete(temporary);}
            }
        }
    }
}
