using System;
using System.Collections;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;

namespace TwinKartLauncher.Gateway
{
    // Object-level JSON contracts only. The original client's SHA-256/AES-CBC
    // WebSocket envelope cannot be emitted until prefix, key and IV are known.
    // These fields were independently observed in this client's request log
    // and Android/PC reflection; they are not a complete server implementation.
    public sealed class KtGameModeContract
    {
        public int RaceMode { get; internal set; }
        // These fields may be omitted by the client's JSON serializer. A null
        // value means absent, not an inferred default.
        public string Profile { get; internal set; }
    }

    public sealed class CsRaceGameContract
    {
        public string Track { get; internal set; }
        public KtGameModeContract GameMode { get; internal set; }
        public int? Lap { get; internal set; }
        public string LevelOption { get; internal set; }
        public string CustomGameOption { get; internal set; }
    }

    public abstract class RoomPacketContract
    {
        public object RequestId { get; internal set; }
        public abstract string PacketType { get; }
    }

    public sealed class CreateCustomGameContract : RoomPacketContract
    {
        public override string PacketType { get { return "PqCreateCustomGame"; } }
        public CsRaceGameContract Setting { get; internal set; }
        public bool ObserverMode { get; internal set; }
        public bool? HideGameKey { get; internal set; }
    }

    public sealed class JoinCustomGameContract : RoomPacketContract
    {
        public override string PacketType { get { return "PqJoinCustomGame"; } }
        public string SessionId { get; internal set; }
    }

    public sealed class AddCustomGameAiContract : RoomPacketContract
    {
        public override string PacketType { get { return "PqAddCustomGameAiMember"; } }
        public int Slot { get; internal set; }
    }

    public sealed class PlayCustomGameContract : RoomPacketContract
    {
        public override string PacketType { get { return "PqPlayCustomGame"; } }
        public bool HostMode { get; internal set; }
    }
    public sealed class CustomGameReadyContract : RoomPacketContract
    {
        public override string PacketType { get { return "PqCustomGameSetReady"; } }
        // Native signed Int32 field; values do not establish that a race host
        // has finished loading or that a participant is driving.
        public int ReadyState { get; internal set; }
    }
    public sealed class KickCustomGameMemberContract : RoomPacketContract
    {
        public override string PacketType { get { return "PqKickCustomGameMember"; } }
        public int Slot { get; internal set; }
    }
    public sealed class ChangeCustomGameSettingContract : RoomPacketContract
    {
        public override string PacketType { get { return "PqChangeCustomGameSetting"; } }
        public CsRaceGameContract Setting { get; internal set; }
    }
    public sealed class ExitCustomGameContract : RoomPacketContract
    {
        internal string Operation;
        public override string PacketType { get { return Operation; } }
    }

    public sealed class CustomGamePushShape
    {
        public string SessionId { get; internal set; }
        public bool InfoValid { get; internal set; }
        public bool SessionFieldPresent { get; internal set; }
        public CsRaceGameContract RaceSession { get; internal set; }
        public bool MembersFieldPresent { get; internal set; }
        public CsMemberContract[] Members { get; internal set; }
        public int MemberCount { get { return Members == null ? 0 : Members.Length; } }
        public int AiMemberCount
        {
            get
            {
                int count = 0;
                if (Members != null)
                    foreach (CsMemberContract member in Members)
                        if (member.Ai == true) count++;
                return count;
            }
        }
    }

    // Only the independently identified member fields needed to describe a
    // room are modeled. Their server-side required/default rules are unknown.
    public sealed class CsMemberContract
    {
        public int? Slot { get; internal set; }
        public bool? Ai { get; internal set; }
        public string AccountId { get; internal set; }
        public bool? IsSpectator { get; internal set; }
        public int? ReadyState { get; internal set; }
    }

    public static class PacketJsonCodec
    {
        public const int MaxJsonChars = 1024 * 1024;
        const int MaxShortString = 256;

        public static bool TryReadRoomRequest(string json, out RoomPacketContract request, out string error)
        {
            request = null;
            Dictionary<string, object> packet;
            if (!ObjectJson(json, out packet, out error)) return false;
            string type;
            object reqId;
            if (!RequiredString(packet, "_type", out type, false) ||
                !packet.TryGetValue("reqId", out reqId) || !ValidRequestId(reqId))
            { error = "Missing packet type or scalar reqId."; return false; }
            switch (type)
            {
                case "PqCreateCustomGame":
                    Dictionary<string, object> setting;
                    CsRaceGameContract race;
                    bool observer;
                    bool? hide = null;
                    object hideRaw;
                    if (!RequiredObject(packet, "setting", out setting) ||
                        !TryReadRaceGame(setting, out race, out error) ||
                        !RequiredBoolean(packet, "observerMode", out observer))
                    { if (error == null) error = "Invalid create-custom-game fields."; return false; }
                    if (packet.TryGetValue("hideGameKey", out hideRaw))
                    {
                        if (!(hideRaw is bool)) { error = "hideGameKey must be a boolean when present."; return false; }
                        hide = (bool)hideRaw;
                    }
                    request = new CreateCustomGameContract { RequestId = reqId,
                        Setting = race, ObserverMode = observer, HideGameKey = hide };
                    error = null; return true;
                case "PqJoinCustomGame":
                    string session;
                    if (!RequiredString(packet, "sessionId", out session, true))
                    { error = "sessionId must be a bounded string."; return false; }
                    request = new JoinCustomGameContract { RequestId = reqId, SessionId = session };
                    error = null; return true;
                case "PqAddCustomGameAiMember":
                    int slot;
                    if (!RequiredInt32(packet, "slot", out slot) || slot < 0)
                    { error = "slot must be a nonnegative Int32."; return false; }
                    request = new AddCustomGameAiContract { RequestId = reqId, Slot = slot };
                    error = null; return true;
                case "PqPlayCustomGame":
                    bool host;
                    if (!RequiredBoolean(packet, "hostMode", out host))
                    { error = "hostMode must be a boolean."; return false; }
                    request = new PlayCustomGameContract { RequestId = reqId, HostMode = host };
                    error = null; return true;
                case "PqCustomGameSetReady":
                    int readyState;
                    if (!RequiredInt32(packet, "readyState", out readyState) || readyState < 0)
                    { error = "readyState must be a nonnegative Int32."; return false; }
                    request = new CustomGameReadyContract { RequestId = reqId, ReadyState = readyState };
                    error = null; return true;
                case "PqKickCustomGameMember":
                    int kickedSlot;
                    if (!RequiredInt32(packet, "slot", out kickedSlot) || kickedSlot < 1 || kickedSlot > 7)
                    { error = "Only AI slots 1..7 may be removed."; return false; }
                    request = new KickCustomGameMemberContract { RequestId = reqId, Slot = kickedSlot };
                    error = null; return true;
                case "PqChangeCustomGameSetting":
                    Dictionary<string, object> changedSetting;
                    CsRaceGameContract changedRace;
                    if (!RequiredObject(packet, "setting", out changedSetting) || !TryReadRaceGame(changedSetting, out changedRace, out error)) return false;
                    request = new ChangeCustomGameSettingContract { RequestId = reqId, Setting = changedRace };
                    error = null; return true;
                case "PqLeaveCustomGame": case "PqAbortCustomGame":
                    request = new ExitCustomGameContract { RequestId = reqId, Operation = type };
                    error = null; return true;
                default:
                    error = "Room request type is not modeled."; return false;
            }
        }

        // This recognizes only a conservative CsRaceGame subset. The declared
        // info.session property is UObject*, not statically CsRaceGame, so a
        // different runtime subtype is reported as unsupported rather than cast.
        // KxSerialize visits PcCustomGame.info even when infoValid is false;
        // this validator therefore always requires an info object, without
        // claiming that JSON omission behavior is proven for all builds.
        public static bool TryReadCustomGamePush(string json, out CustomGamePushShape push, out string error)
        {
            push = null;
            Dictionary<string, object> packet, info;
            if (!ObjectJson(json, out packet, out error)) return false;
            string type, sessionId;
            bool infoValid;
            if (!RequiredString(packet, "_type", out type, false) || type != "PcCustomGame" ||
                !RequiredString(packet, "sessionId", out sessionId, true) ||
                !RequiredBoolean(packet, "infoValid", out infoValid) ||
                !RequiredObject(packet, "info", out info))
            { error = "PcCustomGame requires sessionId, infoValid and info shape."; return false; }
            var result = new CustomGamePushShape { SessionId = sessionId, InfoValid = infoValid };
            object rawSession;
            if (info.TryGetValue("session", out rawSession))
            {
                result.SessionFieldPresent = true;
                if (rawSession != null)
                {
                    var raceObject = rawSession as Dictionary<string, object>;
                    CsRaceGameContract race;
                    if (raceObject == null || !TryReadRaceGame(raceObject, out race, out error))
                    { if (error == null) error = "Unsupported or invalid info.session object."; return false; }
                    result.RaceSession = race;
                }
            }
            // A valid-looking room is not declared ready without a concrete
            // session object. Other CustomGameInfo fields remain unresolved.
            if (infoValid && result.RaceSession == null)
            { error = "infoValid room lacks a verified CsRaceGame session shape."; return false; }
            object members;
            if (info.TryGetValue("members", out members))
            {
                result.MembersFieldPresent = true;
                var array = members as IList;
                if (array == null) { error = "members must be an array when present."; return false; }
                var parsed = new List<CsMemberContract>();
                foreach (object item in array)
                {
                    var member = item as Dictionary<string, object>;
                    if (member == null) { error = "member must be an object."; return false; }
                    CsMemberContract parsedMember;
                    if (!TryReadMember(member, out parsedMember, out error)) return false;
                    parsed.Add(parsedMember);
                }
                result.Members = parsed.ToArray();
            }
            push = result;
            error = null; return true;
        }

        // Only _type, track, gameMode and raceMode are required by this
        // independently observed subset. Optional fields are checked when
        // present; omitted values are not silently guessed.
        static bool TryReadRaceGame(Dictionary<string, object> source, out CsRaceGameContract race, out string error)
        {
            race = null; error = null;
            string type, track, profile, level, custom;
            Dictionary<string, object> mode;
            int raceMode, lap;
            if (!RequiredString(source, "_type", out type, false) || type != "CsRaceGame")
            { error = "Expected CsRaceGame polymorphic type."; return false; }
            if (!RequiredString(source, "track", out track, false) ||
                !Regex.IsMatch(track, "^[A-Za-z0-9_-]{1,80}\\z") ||
                !RequiredObject(source, "gameMode", out mode) ||
                !RequiredInt32(mode, "raceMode", out raceMode) || raceMode < 1 || raceMode > 6 ||
                !OptionalString(mode, "profile", out profile) ||
                !OptionalInt32(source, "lap", out lap) ||
                !OptionalString(source, "levelOption", out level) ||
                !OptionalString(source, "customGameOption", out custom))
            { error = "CsRaceGame observed fields have invalid types or mode/track bounds."; return false; }
            race = new CsRaceGameContract { Track = track,
                GameMode = new KtGameModeContract { RaceMode = raceMode, Profile = profile },
                Lap = source.ContainsKey("lap") ? (int?)lap : null,
                LevelOption = level, CustomGameOption = custom };
            return true;
        }

        static bool TryReadMember(Dictionary<string, object> source, out CsMemberContract member, out string error)
        {
            member = null; error = null;
            object raw;
            int slot = 0;
            if (source.TryGetValue("slot", out raw) && !Int32(raw, out slot))
            { error = "member.slot must be Int32."; return false; }
            bool? ai = null, spectator = null;
            if (source.TryGetValue("ai", out raw))
            {
                if (!(raw is bool)) { error = "member.ai must be boolean."; return false; }
                ai = (bool)raw;
            }
            if (source.TryGetValue("isSpectator", out raw))
            {
                if (!(raw is bool)) { error = "member.isSpectator must be boolean."; return false; }
                spectator = (bool)raw;
            }
            int readyState;
            if (!OptionalInt32(source, "readyState", out readyState))
            { error = "member.readyState must be Int32."; return false; }
            string accountId;
            if (!OptionalString(source, "accountId", out accountId))
            { error = "member.accountId must be a bounded string."; return false; }
            member = new CsMemberContract {
                Slot = source.ContainsKey("slot") ? (int?)slot : null,
                Ai = ai, AccountId = accountId, IsSpectator = spectator,
                ReadyState = source.ContainsKey("readyState") ? (int?)readyState : null
            };
            return true;
        }

        static bool ObjectJson(string json, out Dictionary<string, object> packet, out string error)
        {
            packet = null; error = null;
            if (json == null || json.Length == 0 || json.Length > MaxJsonChars)
            { error = "JSON packet length is invalid."; return false; }
            try { packet = new JavaScriptSerializer().Deserialize<Dictionary<string, object>>(json); }
            catch (ArgumentException) { error = "Invalid JSON packet."; return false; }
            catch (InvalidOperationException) { error = "Invalid JSON packet."; return false; }
            if (packet == null) { error = "Expected a JSON object."; return false; }
            return true;
        }

        static bool RequiredObject(Dictionary<string, object> source, string name,
                                   out Dictionary<string, object> result)
        {
            object raw;
            if (source.TryGetValue(name, out raw) && (result = raw as Dictionary<string, object>) != null) return true;
            result = null; return false;
        }
        static bool RequiredString(Dictionary<string, object> source, string name,
                                   out string result, bool allowEmpty)
        {
            object raw;
            result = source.TryGetValue(name, out raw) ? raw as string : null;
            return result != null && result.Length <= MaxShortString && (allowEmpty || result.Length > 0);
        }
        static bool OptionalString(Dictionary<string, object> source, string name, out string result)
        {
            if (!source.ContainsKey(name)) { result = null; return true; }
            return RequiredString(source, name, out result, true);
        }
        static bool OptionalInt32(Dictionary<string, object> source, string name, out int result)
        {
            if (!source.ContainsKey(name)) { result = 0; return true; }
            return RequiredInt32(source, name, out result);
        }
        static bool RequiredBoolean(Dictionary<string, object> source, string name, out bool result)
        {
            object raw;
            if (source.TryGetValue(name, out raw) && raw is bool) { result = (bool)raw; return true; }
            result = false; return false;
        }
        static bool RequiredInt32(Dictionary<string, object> source, string name, out int result)
        {
            object raw;
            if (source.TryGetValue(name, out raw)) return Int32(raw, out result);
            result = 0; return false;
        }
        static bool Int32(object raw, out int result)
        {
            if (raw is int) { result = (int)raw; return true; }
            if (raw is long && (long)raw >= int.MinValue && (long)raw <= int.MaxValue)
            { result = (int)(long)raw; return true; }
            result = 0; return false;
        }
        static bool ValidRequestId(object raw)
        {
            return raw is int || raw is long ||
                (raw is string && ((string)raw).Length > 0 && ((string)raw).Length <= 128);
        }
    }
}
