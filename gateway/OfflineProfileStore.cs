using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
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
        readonly int pingPort;
        Dictionary<string, object> state;
        public const string SharedValue = "TwinKartOriginalParametersV1";

        public OfflineProfileStore(string profilePath, string initialProfile, string clientDefaults,
            string unlockCatalog, string tuningRules, string trackCatalog, string physicsCatalog,
            string vehicleParts, string accountId, int localPingPort, string tuningPlan = null)
        {
            path = Path.GetFullPath(profilePath); account = accountId; pingPort = localPingPort;
            defaults = ProfileJson.Parse(clientDefaults); parts = ProfileJson.Parse(vehicleParts);
            rulesJson = tuningRules; tracks = new TrackCatalog(trackCatalog);
            physics = new NativeKartParameters(physicsCatalog);
            string existing = File.Exists(path) ? ProfileJson.Read(path) : initialProfile;
            string granted = InventoryUnlocks.Apply(existing, unlockCatalog).ProfileJson;
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
                        case "PqTimeAttackGetRankVersion": response["_type"] = "PrTimeAttackGetRankVersion"; response["version"] = 1; break;
                        case "PqTimeAttackGetRankers":
                            response["_type"] = "PrTimeAttackGetRankers"; response["trackId"] = request["trackId"];
                            response["myRank"] = 0; response["myRecord"] = new object[0]; response["totalRank"] = new object[0]; response["platformRank"] = new object[0]; response["countryRank"] = new object[0]; break;
                        case "PqTimeAttackPrepare": break;
                        case "PqApplySpeedBodyParamForSingleRace": response["_type"] = "PrApplySpeedBodyParamForSingleRace"; Overlay(response, physics.Payload(next, account, SharedValue)); break;
                        case "PqTimeAttackStart":
                            string track = request["trackId"] as string;
                            if (!tracks.Contains(track)) throw new ArgumentException("Unknown track.");
                            physics.Payload(next, account, SharedValue);
                            string gameId = Guid.NewGuid().ToString("N");
                            next["activeRace"] = new Dictionary<string, object> { { "gameId", gameId }, { "trackId", track }, { "startTime", Now() } };
                            response["_type"] = "PrTimeAttackStart"; Overlay(response, ProfileJson.Object(next, "activeRace")); changed = true; break;
                        case "PqTimeAttackUpdate":
                            var active = ProfileJson.Object(next, "activeRace");
                            if (!Object.Equals(request["gameId"], active["gameId"])) throw new ArgumentException("Race identity mismatch.");
                            var record = ProfileJson.Object(request["timeAttack"]);
                            if (!Object.Equals(record["trackId"], active["trackId"])) throw new ArgumentException("Race track mismatch.");
                            long milliseconds = Convert.ToInt64(record["recordTime"]);
                            if (milliseconds < 0 || milliseconds > 24 * 60 * 60 * 1000) throw new ArgumentException("Invalid race time.");
                            response["_type"] = "PrTimeAttackUpdate";
                            if (milliseconds > 0)
                            {
                                var records = ProfileJson.Object(next, "records");
                                string completedTrack = ProfileJson.Text(active, "trackId"); object best;
                                if (!records.TryGetValue(completedTrack, out best) || milliseconds < Convert.ToInt64(ProfileJson.Object(best)["lapTime"]))
                                {
                                    var equipment = ProfileJson.Object(ProfileJson.Object(next, "itemPresetOn"), "itemMap");
                                    var vehicle = ProfileJson.Object(ProfileJson.Object(next, "ownKartsUpgradeInfo"), ProfileJson.Text(equipment, "Kart"));
                                    records[completedTrack] = new Dictionary<string,object> {
                                        {"trackId",completedTrack},{"lapTime",milliseconds},{"mileage",record.ContainsKey("recordMileage")?record["recordMileage"]:0},
                                        {"kartId",equipment["Kart"]},{"kartLevel",vehicle.ContainsKey("level")?vehicle["level"]:0},{"kartGrade",vehicle["grade"]},
                                        {"beginnerFlag",next.ContainsKey("beginnerFlag")?next["beginnerFlag"]:0},{"replayPath",""},{"version",1},{"updateDate",Now()},
                                        {"preview",new Dictionary<string,object>{{"itemPresetOn",ProfileJson.Copy(next["itemPresetOn"])},{"itemEquipmentOn",next.ContainsKey("itemEquipmentOn")?ProfileJson.Copy(next["itemEquipmentOn"]):new Dictionary<string,object>()}}},
                                        {"emblemId",next.ContainsKey("emblemItemIdOn")?next["emblemItemIdOn"]:""}
                                    };
                                }
                            }
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
    }
}
