using System;
using System.Collections.Generic;
using System.Globalization;
using System.Security.Cryptography;
using System.Text;

namespace TwinKartLauncher.Gateway
{
    // Original wire adapter for the existing first-party room state model.
    // A native room snapshot is not a running race session. Starting a race
    // remains explicitly unavailable until its host/session protocol is ready.
    internal sealed class OfflineRoomService
    {
        readonly RoomState room = new RoomState();
        readonly string account;
        internal OfflineRoomService(string localAccount) { account = localAccount; }
        internal static bool Handles(string type)
        {
            switch (type)
            {
                case "PqCreateCustomGame": case "PqJoinCustomGame": case "PqAddCustomGameAiMember":
                case "PqKickCustomGameMember": case "PqChangeCustomGameSetting": case "PqLeaveCustomGame":
                case "PqAbortCustomGame": case "PqPlayCustomGame": case "PqCustomGameSetReady": return true;
                default: return false;
            }
        }
        static Dictionary<string,object> Object(params object[] fields)
        {
            var value = new Dictionary<string,object>(StringComparer.Ordinal);
            for (int i=0; i<fields.Length; i+=2) value.Add((string)fields[i], fields[i+1]);
            return value;
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
            GatewayTransition transition = room.Apply(ProfileJson.Write(request));
            bool start = type == "PqPlayCustomGame";
            var reply = new ProfileResult { Response = Object("_type","PrPacket","reqId",id,
                "result",Object("code",transition.Accepted && !start ? 0 : 10001),"desc","") };
            if (!transition.Accepted) return reply;
            if (start)
            {
                reply.Response["desc"] = "Local room exists; race host/session integration is not ready.";
                return reply;
            }
            if (type == "PqLeaveCustomGame" || type == "PqAbortCustomGame") return reply;
            if (type == "PqCreateCustomGame") reply.Notices.Add(Object("_type","PcPartyCreateJoinCustomGame"));
            reply.Notices.Add(Snapshot(profile,login));
            return reply;
        }
    }
}
