using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Security.Cryptography;
using System.Web.Script.Serialization;

namespace TwinKartLauncher.Gateway
{
    internal static class ProfileJson
    {
        static JavaScriptSerializer Serializer() { return new JavaScriptSerializer { MaxJsonLength = 32 * 1024 * 1024, RecursionLimit = 96 }; }
        public static Dictionary<string, object> Parse(string text) { return Object(Serializer().DeserializeObject(text)); }
        public static string Write(object value) { return Serializer().Serialize(value); }
        public static object Copy(object value) { return Serializer().DeserializeObject(Write(value)); }
        public static Dictionary<string, object> Object(object value)
        { var map = value as Dictionary<string, object>; if (map == null) throw new FormatException("Expected object."); return map; }
        public static Dictionary<string, object> Object(Dictionary<string, object> map, string key) { return Object(map[key]); }
        public static string Text(Dictionary<string, object> map, string key)
        { var text = map[key] as string; if (text == null) throw new FormatException("Expected text."); return text; }
        public static string Read(string path)
        { var file = new FileInfo(path); if (!file.Exists || file.Length > 32 * 1024 * 1024) throw new FormatException("External JSON size."); return File.ReadAllText(path, new UTF8Encoding(false, true)); }
    }

    public sealed class ProfileResult
    {
        public Dictionary<string, object> Response;
        public readonly List<Dictionary<string, object>> Notices = new List<Dictionary<string, object>>();
    }

    // Shared by authenticated connections. Every mutation is evaluated on a
    // private tree; only validated results are atomically committed to disk.
    public sealed class OfflineProfileStore
    {
        readonly object gate = new object();
        readonly string path, account, rulesJson;
        readonly Dictionary<string, object> defaults, parts;
        readonly TrackCatalog tracks;
        readonly NativeKartParameters physics;
        readonly OfflineRoomService rooms;
        readonly int pingPort;
        Dictionary<string, object> state;
        public const string SharedValue = "TwinKartOriginalParametersV1";

        public OfflineProfileStore(string profilePath, string initialProfile, string clientDefaults,
            string unlockCatalog, string tuningRules, string trackCatalog, string physicsCatalog,
            string vehicleParts, string accountId, int localPingPort, string tuningPlan = null, string vehicleAvailability = null,
            Func<RoomState,string> startRoom = null)
        {
            path = Path.GetFullPath(profilePath); account = accountId; pingPort = localPingPort;
            rooms = new OfflineRoomService(accountId,startRoom,unlockCatalog,vehicleParts,physicsCatalog,vehicleAvailability);
            defaults = ProfileJson.Parse(clientDefaults); parts = ProfileJson.Parse(vehicleParts);
            rulesJson = tuningRules; tracks = new TrackCatalog(trackCatalog);
            physics = new NativeKartParameters(physicsCatalog);
            string existing = File.Exists(path) ? ProfileJson.Read(path) : initialProfile;
            string granted = InventoryUnlocks.Apply(existing, unlockCatalog, vehicleAvailability).ProfileJson;
            state = ProfileJson.Parse(tuningPlan == null ? granted : OfflineGarage.Apply(granted, tuningRules, tuningPlan).ProfileJson);
            Commit(state);
        }
        public string Snapshot() { lock (gate) return ProfileJson.Write(state); }
        void Commit(Dictionary<string, object> next)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            string temporary = path + "." + Guid.NewGuid().ToString("N") + ".tmp";
            try
            {
                using (var output = new FileStream(temporary, FileMode.CreateNew, FileAccess.Write, FileShare.None))
                {
                    byte[] bytes = new UTF8Encoding(false).GetBytes(ProfileJson.Write(next));
                    output.Write(bytes, 0, bytes.Length); output.Flush(true);
                }
                if (File.Exists(path)) File.Replace(temporary, path, path + ".previous", true);
                else File.Move(temporary, path);
                state = next;
            }
            finally { if (File.Exists(temporary)) File.Delete(temporary); }
        }
        static Dictionary<string, object> Reply(string type, int id, int code = 0)
        { return new Dictionary<string, object> { { "_type", type }, { "reqId", id }, { "result", new Dictionary<string, object> { { "code", code } } }, { "desc", "" } }; }
        static string Now() { return DateTime.UtcNow.ToString("o", System.Globalization.CultureInfo.InvariantCulture); }
        static object Canonical(object value)
        {
            var map = value as IDictionary<string,object>;
            if (map != null) {
                var sorted = new SortedDictionary<string,object>(StringComparer.Ordinal);
                foreach (var pair in map) sorted.Add(pair.Key,Canonical(pair.Value));
                return sorted;
            }
            var array = value as object[];
            if (array != null) return array.Select(Canonical).ToArray();
            return value;
        }
        static string ResultSignature(IDictionary<string,object> request)
        {
            object flag;
            var effective = new Dictionary<string,object> {
                {"timeAttack",request["timeAttack"]},{"status",request["status"]},
                {"bCheat",request.TryGetValue("bCheat",out flag) ? flag : false}
            };
            using (var hash = SHA256.Create()) return Convert.ToBase64String(hash.ComputeHash(Encoding.UTF8.GetBytes(ProfileJson.Write(Canonical(effective)))));
        }
        static Dictionary<string,object> ReplayResult(Dictionary<string,object> profile, string gameId, string signature, int requestId)
        {
            object raw;
            if (!profile.TryGetValue("raceResultHistory",out raw)) return null;
            foreach (object value in (object[])raw) {
                var saved = ProfileJson.Object(value);
                if (!Object.Equals(saved["gameId"],gameId)) continue;
                if (!Object.Equals(saved["signature"],signature)) throw new ArgumentException("Conflicting result for completed race.");
                var reply = ProfileJson.Object(ProfileJson.Copy(saved["response"])); reply["reqId"] = requestId; return reply;
            }
            return null;
        }
        static void RememberResult(Dictionary<string,object> profile, string gameId, string signature, Dictionary<string,object> response)
        {
            object raw;
            var history = profile.TryGetValue("raceResultHistory",out raw) ? new List<object>((object[])raw) : new List<object>();
            var saved = ProfileJson.Object(ProfileJson.Copy(response)); saved.Remove("reqId");
            history.Add(new Dictionary<string,object>{{"gameId",gameId},{"signature",signature},{"response",saved}});
            while (history.Count > 16) history.RemoveAt(0);
            profile["raceResultHistory"] = history;
        }
        static void Overlay(Dictionary<string, object> destination, Dictionary<string, object> source)
        { foreach (var pair in source) destination[pair.Key] = ProfileJson.Copy(pair.Value); }
        HashSet<string> Owned(Dictionary<string, object> profile)
        {
            var result = new HashSet<string>(StringComparer.Ordinal);
            foreach (var bucket in ProfileJson.Object(profile, "inventory").Values)
            {
                object raw;
                if (!ProfileJson.Object(bucket).TryGetValue("itemList", out raw)) continue;
                foreach (object item in (object[])raw) result.Add(((string)item).Split(',')[0]);
            }
            return result;
        }
        void NotifyBody(ProfileResult result, Dictionary<string, object> profile)
        {
            var notice = physics.Payload(profile, account, SharedValue);
            notice["_type"] = "PcKartBodyParamUpdated"; result.Notices.Add(notice);
        }
        public ProfileResult Handle(IDictionary<string, object> request, int id)
        {
            string type = request["_type"] as string;
            lock (gate)
            {
                var next = ProfileJson.Parse(ProfileJson.Write(state));
                var result = new ProfileResult { Response = Reply("PrPacket", id) };
                var response = result.Response;
                bool changed = false;
                try
                {
                    if (OfflineRoomService.Handles(type)) return rooms.Handle(request,id,next,ProfileJson.Object(defaults,"login"));
                    switch (type)
                    {
                        case "PqLogin":
                            Overlay(response, ProfileJson.Object(defaults, "login"));
                            foreach (var pair in next) if (response.ContainsKey(pair.Key)) response[pair.Key] = ProfileJson.Copy(pair.Value);
                            response["_type"] = "PrLogin"; response["accountId"] = account;
                            response["loginTime"] = Now(); response["utcNow"] = Now(); response["accPlayTimeSec"] = 0;
                            response["sharedValue"] = SharedValue;
                            string region = ProfileJson.Text(response, "serverRegionId");
                            if (!DynamicDataCatalog.ValidName(region)) throw new FormatException("Invalid external region.");
                            ProfileJson.Object(response, "serverSetting")["pingServers"] = region + "=[127.0.0.1:" + pingPort + "];";
                            break;
                        case "PqInventory": response["_type"] = "PrInventory"; Overlay(response, ProfileJson.Object(next, "inventory")); break;
                        case "PqLoginComplete": NotifyBody(result, next); break;
                        case "PqEcho": response["_type"] = "PrEcho"; break;
                        case "PqServerTime": response["_type"] = "PrServerTime"; response["utc"] = Now(); break;
                        case "PqSetClientStage": response["_type"] = "PrSetClientStage"; break;
                        case "PqReadyGame":
                            if(!request.ContainsKey("ready") || !(request["ready"] is bool)) throw new ArgumentException("Expected ready boolean.");
                            // Readiness is local lobby state. Race travel is
                            // consumed on the original client's game thread;
                            // this reply does not claim a running match.
                            next["localReady"]=(bool)request["ready"]; changed=true; break;
                        case "PqChangeBeginnerSetting":
                            if (request.ContainsKey("beginnerFlag")) { next["beginnerFlag"] = request["beginnerFlag"]; changed = true; }
                            break;
                        case "PqAwsLatencyInfo":
                            object latency;
                            if (!request.TryGetValue("awsLatencyInfoList", out latency) || !(latency is object[])) throw new ArgumentException("Expected latency array.");
                            result.Notices.Add(new Dictionary<string, object> { { "_type", "PcAwsLatencyInfo" }, { "awsLatencyInfoList", ProfileJson.Copy(latency) } });
                            break;
                        case "PqOptionSettingUpdate": next["optionSetting"] = ProfileJson.Copy(request["optionSetting"]); changed = true; break;
                        case "PqSetQuickMatchData":
                            next["quickMatchData"] = ProfileJson.Copy(ProfileJson.Object(request["data"])); changed = true; break;
                        case "PqSetAutoReady":
                            if (!(request["autoReady"] is bool)) throw new ArgumentException("Expected auto-ready boolean.");
                            next["autoReady"] = request["autoReady"]; changed = true; break;
                        case "PqItemPresetOnUpdate":
                            var preset = ProfileJson.Object(ProfileJson.Copy(request["itemPreset"]));
                            var items = ProfileJson.Object(preset, "itemMap"); var owned = Owned(next);
                            foreach (var item in items.Values) if (!(item is string) || (!String.IsNullOrEmpty((string)item) && !owned.Contains((string)item))) throw new ArgumentException("Item not owned.");
                            string kart = ProfileJson.Text(items, "Kart");
                            object native;
                            if (ProfileJson.Object(parts, "vehicles").TryGetValue(kart, out native))
                                foreach (var part in ProfileJson.Object(native)) if (!items.ContainsKey(part.Key) || String.IsNullOrEmpty(items[part.Key] as string)) items[part.Key] = part.Value;
                            next["itemPresetOn"] = preset; response["_type"] = "PrItemPresetOnUpdate"; response["itemPreset"] = ProfileJson.Copy(preset);
                            NotifyBody(result, next); changed = true; break;
                        case "PqKartGearSet": case "PqKartGearReset":
                            string selected = request["kartItemId"] as string;
                            var plan = new Dictionary<string, object> { { "karts", new[] { selected } }, { "settings", type == "PqKartGearReset" ? EmptySettings() : request["gearSettings"] } };
                            next = ProfileJson.Parse(OfflineGarage.Apply(ProfileJson.Write(next), rulesJson, ProfileJson.Write(plan)).ProfileJson);
                            var upgraded = ProfileJson.Object(ProfileJson.Object(next, "ownKartsUpgradeInfo"), selected);
                            response["_type"] = type == "PqKartGearReset" ? "PrKartGearReset" : "PrKartGearSet";
                            response["kartItemId"] = selected; response["remainGears"] = OfflineTuning.DisplayGearBalance;
                            response["gearSettings"] = ProfileJson.Copy(ProfileJson.Object(upgraded, "gearInfo")["gearSettings"]);
                            result.Notices.Add(new Dictionary<string, object> { { "_type", "PcKartUpgradeInfo" }, { "kartItemId", selected }, { "upgradeInfo", ProfileJson.Copy(upgraded) } });
                            physics.Payload(next, account, SharedValue, selected); NotifyBody(result, next); changed = true; break;
                        case "PqKartUpgrade":
                            string upgradeKart = request["kartItemId"] as string;
                            var upgrade = ProfileJson.Object(ProfileJson.Object(next, "ownKartsUpgradeInfo"), upgradeKart);
                            int engine = Convert.ToInt32(upgrade["engineType"]);
                            int highestGrade = Convert.ToInt32(defaults["maximumUpgradeGrade"]);
                            int highestLevel = Convert.ToInt32(defaults["maximumUpgradeLevel"]);
                            if (highestGrade < 0 || highestGrade > 255 || highestLevel < 0 || highestLevel > 1000) throw new FormatException("Invalid external upgrade range.");
                            if (engine == 0) { upgrade["grade"] = Math.Min(highestGrade, Math.Max(0, Convert.ToInt32(upgrade["grade"])) + 1); upgrade["level"] = 0; }
                            else if (engine == 1) { upgrade["grade"] = highestGrade; upgrade["level"] = Math.Min(highestLevel, Convert.ToInt32(upgrade["level"]) + 1); }
                            else throw new ArgumentException("Unmapped engine type.");
                            ProfileJson.Object(upgrade, "gearInfo")["totalGearCount"] = OfflineTuning.DisplayGearBalance;
                            ProfileJson.Object(upgrade, "gearInfo")["remainGearCount"] = OfflineTuning.DisplayGearBalance;
                            response["_type"] = "PrKartUpgrade"; response["kartGrade"] = upgrade["grade"]; response["kartLevel"] = upgrade["level"];
                            result.Notices.Add(new Dictionary<string, object> { { "_type", "PcKartUpgradeInfo" }, { "kartItemId", upgradeKart }, { "upgradeInfo", ProfileJson.Copy(upgrade) } });
                            physics.Payload(next, account, SharedValue, upgradeKart); NotifyBody(result, next); changed = true; break;
                        case "PqTimeAttackGetTracks":
                            response["_type"] = "PrTimeAttackGetTracks";
                            response["trackList"] = new JavaScriptSerializer().DeserializeObject(tracks.TimeAttackRows(new HashSet<string>(ProfileJson.Object(next, "records").Keys)));
                            break;
                        case "PqTimeAttackGetRecords": response["_type"] = "PrTimeAttackGetRecords"; response["records"] = ProfileJson.Copy(next["records"]); break;
                        case "PqTimeAttackGetRankVersion": response["_type"] = "PrTimeAttackGetRankVersion"; response["version"] = next.ContainsKey("rankVersion") ? next["rankVersion"] : 1; break;
                        case "PqTimeAttackGetRankers":
                            LocalRankers(next,request["trackId"] as string,response); break;
                        case "PqTimeAttackPrepare": break;
                        case "PqApplySpeedBodyParamForSingleRace": response["_type"] = "PrApplySpeedBodyParamForSingleRace"; Overlay(response, physics.Payload(next, account, SharedValue)); break;
                        case "PqTimeAttackStart":
                            string track = request["trackId"] as string;
                            if (!tracks.Contains(track)) throw new ArgumentException("Unknown track.");
                            physics.Payload(next, account, SharedValue);
                            string gameId = Guid.NewGuid().ToString("N");
                            next["activeRace"] = new Dictionary<string, object> { { "gameId", gameId }, { "trackId", track }, { "startTime", Now() } };
                            response["_type"] = "PrTimeAttackStart"; Overlay(response, ProfileJson.Object(next, "activeRace"));
                            var startingRace = ProfileJson.Object(next,"activeRace");
                            startingRace["itemPresetOn"] = ProfileJson.Copy(next["itemPresetOn"]);
                            startingRace["itemEquipmentOn"] = next.ContainsKey("itemEquipmentOn") ? ProfileJson.Copy(next["itemEquipmentOn"]) : new Dictionary<string,object>();
                            var startingKart = ProfileJson.Text(ProfileJson.Object(ProfileJson.Object(next,"itemPresetOn"),"itemMap"),"Kart");
                            startingRace["upgradeInfo"] = ProfileJson.Copy(ProfileJson.Object(next,"ownKartsUpgradeInfo")[startingKart]);
                            startingRace["beginnerFlag"] = next.ContainsKey("beginnerFlag") ? next["beginnerFlag"] : 0;
                            startingRace["emblemId"] = next.ContainsKey("emblemItemIdOn") ? next["emblemItemIdOn"] : "";
                            changed = true; break;
                        case "PqTimeAttackUpdate":
                            var record = ProfileJson.Object(request["timeAttack"]);
                            if (!(record["recordTime"] is int || record["recordTime"] is long)) throw new ArgumentException("Race time must be an integer.");
                            long milliseconds = Convert.ToInt64(record["recordTime"]);
                            if (milliseconds < 0 || milliseconds > 24 * 60 * 60 * 1000) throw new ArgumentException("Invalid race time.");
                            if (!(request["status"] is int) || (int)request["status"] < 0) throw new ArgumentException("Invalid native result status.");
                            object cheat;
                            bool flagged = request.TryGetValue("bCheat",out cheat) && cheat is bool && (bool)cheat;
                            if (request.ContainsKey("bCheat") && !(cheat is bool)) throw new ArgumentException("Invalid result flag.");
                            if ((int)request["status"] == 0 && !flagged && milliseconds == 0) throw new ArgumentException("Completed race requires a positive time.");
                            string completedId = request["gameId"] as string;
                            if (String.IsNullOrEmpty(completedId)) throw new ArgumentException("Missing race identity.");
                            string signature = ResultSignature(request);
                            var replay = ReplayResult(next,completedId,signature,id);
                            if (replay != null) { result.Response = replay; return result; }
                            var active = ProfileJson.Object(next, "activeRace");
                            if (!Object.Equals(completedId, active["gameId"])) throw new ArgumentException("Race identity mismatch.");
                            if (!Object.Equals(record["trackId"], active["trackId"])) throw new ArgumentException("Race track mismatch.");
                            response["_type"] = "PrTimeAttackUpdate";
                            response["gameId"] = request["gameId"];
                            response["timeAttack"] = ProfileJson.Copy(record);
                            response["isUpdated"] = false;
                            response["dataStoreLink"] = new Dictionary<string,object> { {"url",""},{"verb",""},{"headers",new Dictionary<string,object>()},{"body",""},{"valueIndex",new object[0]},{"metadata",""} };
                            if ((int)request["status"] == 0 && !flagged && milliseconds > 0)
                            {
                                var records = ProfileJson.Object(next, "records");
                                string completedTrack = ProfileJson.Text(active, "trackId"); object best;
                                if (!records.TryGetValue(completedTrack, out best) || milliseconds < Convert.ToInt64(ProfileJson.Object(best)["lapTime"]))
                                {
                                    var equipment = ProfileJson.Object(ProfileJson.Object(active, "itemPresetOn"), "itemMap");
                                    var vehicle = ProfileJson.Object(active,"upgradeInfo");
                                    records[completedTrack] = new Dictionary<string,object> {
                                        {"trackId",completedTrack},{"lapTime",milliseconds},{"mileage",record.ContainsKey("recordMileage")?record["recordMileage"]:0},
                                        {"kartId",equipment["Kart"]},{"kartLevel",vehicle.ContainsKey("level")?vehicle["level"]:0},{"kartGrade",vehicle["grade"]},
                                        {"beginnerFlag",active["beginnerFlag"]},{"replayPath",""},{"version",1},{"updateDate",Now()},
                                        {"preview",new Dictionary<string,object>{{"itemPresetOn",ProfileJson.Copy(active["itemPresetOn"])},{"itemEquipmentOn",ProfileJson.Copy(active["itemEquipmentOn"])}}},
                                        {"emblemId",active["emblemId"]}
                                    };
                                    int version = next.ContainsKey("rankVersion") ? Convert.ToInt32(next["rankVersion"]) : 1;
                                    next["rankVersion"] = version == Int32.MaxValue ? 1 : version + 1;
                                    if (!next.ContainsKey("recordUpgrades")) next["recordUpgrades"] = new Dictionary<string,object>();
                                    ProfileJson.Object(next,"recordUpgrades")[completedTrack] = ProfileJson.Copy(vehicle);
                                    response["isUpdated"] = true;
                                }
                            }
                            RememberResult(next,completedId,signature,response);
                            next.Remove("activeRace"); changed = true; break;
                        default:
                            object external;
                            if (type != null && ProfileJson.Object(defaults, "readOnlyReplies").TryGetValue(type, out external))
                            { Overlay(response, ProfileJson.Object(external)); response["reqId"] = id; }
                            else response = Reply("PrPacket", id, 10001);
                            break;
                    }
                    if (changed) Commit(next);
                    result.Response = response; return result;
                }
                catch (Exception problem)
                {
                    if (!(problem is ArgumentException || problem is FormatException || problem is KeyNotFoundException || problem is InvalidCastException || problem is OverflowException)) throw;
                    return new ProfileResult { Response = Reply("PrPacket", id, 10001) };
                }
            }
        }
        Dictionary<string, object> EmptySettings()
        {
            var values = new Dictionary<string, object>();
            foreach (var pair in ProfileJson.Object(ProfileJson.Parse(rulesJson), "rules"))
            {
                object compatible;
                if (ProfileJson.Object(pair.Value).TryGetValue("clientCompatible", out compatible) && compatible is bool && !(bool)compatible) continue;
                values.Add(pair.Key, 0);
            }
            return values;
        }
        void LocalRankers(Dictionary<string,object> profile, string track, Dictionary<string,object> response)
        {
            if (!tracks.Contains(track)) throw new ArgumentException("Unknown ranking track.");
            var mine = new List<object>(); var ranking = new List<object>(); object saved;
            if (ProfileJson.Object(profile,"records").TryGetValue(track,out saved))
            {
                var best = ProfileJson.Object(ProfileJson.Copy(saved)); mine.Add(best);
                var avatar = ProfileJson.Object(defaults,"login");
                if (avatar.ContainsKey("profile")) avatar = ProfileJson.Object(avatar,"profile");
                object country,platform,name;
                var row = new Dictionary<string,object> {
                    {"rank",1},{"accountId",account},{"country",avatar.TryGetValue("countryCode",out country)?country:""},
                    {"platform",avatar.TryGetValue("platform",out platform)?platform:0},
                    {"riderName",profile.TryGetValue("riderName",out name)?name:"Local Driver"},
                    {"lapTime",best["lapTime"]},{"preview",ProfileJson.Copy(best["preview"])},
                    {"kartLevel",best["kartLevel"]},{"kartGrade",best["kartGrade"]},
                    {"emblemItemIdOn",best["emblemId"]},{"replayPath",best["replayPath"]}
                };
                object upgrades;
                if (profile.TryGetValue("recordUpgrades",out upgrades) && ProfileJson.Object(upgrades).ContainsKey(track))
                    row["kartUpgradeInfo"] = ProfileJson.Copy(ProfileJson.Object(upgrades)[track]);
                ranking.Add(row);
            }
            response["_type"] = "PrTimeAttackGetRankers"; response["trackId"] = track;
            response["myRank"] = mine.Count; response["myRecord"] = mine;
            response["totalRank"] = ranking; response["platformRank"] = ProfileJson.Copy(ranking); response["countryRank"] = ProfileJson.Copy(ranking);
        }
    }
}
