using System;
using System.Collections.Generic;

namespace TwinKartLauncher.Gateway
{
    // This class deliberately has no network sender. The client-visible auth,
    // room-info and race-session payloads remain unverified. A transition here
    // is local state only; it is never reported as client acceptance.
    public sealed class RoomState
    {
        readonly HashSet<int> aiSlots = new HashSet<int>();
        string sessionId;
        string track;
        int raceMode;
        bool joined;
        bool playRequested;
        bool hostMode;
        bool observerMode, hideGameKey;
        CsRaceGameContract setting;

        public string SessionId { get { return sessionId; } }
        public string Track { get { return track; } }
        public int RaceMode { get { return raceMode; } }
        public bool Joined { get { return joined; } }
        public bool PlayRequested { get { return playRequested; } }
        public bool HostMode { get { return hostMode; } }
        public bool ObserverMode { get { return observerMode; } }
        public bool HideGameKey { get { return hideGameKey; } }
        public CsRaceGameContract Setting { get { return Copy(setting); } }
        public int[] AiSlots { get { var values = new List<int>(aiSlots); values.Sort(); return values.ToArray(); } }
        public int AiCount { get { return aiSlots.Count; } }
        public bool HasAiSlot(int slot) { return aiSlots.Contains(slot); }

        public GatewayTransition Apply(string requestJson)
        {
            RoomPacketContract request;
            string error;
            if (!PacketJsonCodec.TryReadRoomRequest(requestJson, out request, out error))
                return GatewayTransition.Reject(error);
            CreateCustomGameContract create = request as CreateCustomGameContract;
            if (create != null) return Create(create);
            JoinCustomGameContract join = request as JoinCustomGameContract;
            if (join != null) return Join(join);
            AddCustomGameAiContract addAi = request as AddCustomGameAiContract;
            if (addAi != null) return AddAi(addAi);
            PlayCustomGameContract play = request as PlayCustomGameContract;
            if (play != null) return Play(play);
            var kick = request as KickCustomGameMemberContract;
            if (kick != null)
            {
                if (!joined || !aiSlots.Remove(kick.Slot)) return GatewayTransition.Reject("No AI member occupies that slot.", request.PacketType, request.RequestId);
                playRequested = false;
                return GatewayTransition.Accept(request.PacketType, request.RequestId, "AI member removed locally.");
            }
            var change = request as ChangeCustomGameSettingContract;
            if (change != null)
            {
                if (!joined) return GatewayTransition.Reject("Join a local room first.", request.PacketType, request.RequestId);
                setting = Copy(change.Setting); track = setting.Track; raceMode = setting.GameMode.RaceMode; playRequested = false;
                return GatewayTransition.Accept(request.PacketType, request.RequestId, "Local room setting changed.");
            }
            if (request is ExitCustomGameContract)
            {
                if (sessionId == null) return GatewayTransition.Reject("No room exists.", request.PacketType, request.RequestId);
                sessionId = null; setting = null; track = null; raceMode = 0;
                joined = playRequested = hostMode = observerMode = hideGameKey = false; aiSlots.Clear();
                return GatewayTransition.Accept(request.PacketType, request.RequestId, "Local room cleared.");
            }
            return GatewayTransition.Reject("Operation is not yet modeled.");
        }
        static CsRaceGameContract Copy(CsRaceGameContract value)
        {
            if (value == null) return null;
            return new CsRaceGameContract { Track = value.Track, Lap = value.Lap, LevelOption = value.LevelOption,
                CustomGameOption = value.CustomGameOption, GameMode = new KtGameModeContract { RaceMode = value.GameMode.RaceMode, Profile = value.GameMode.Profile } };
        }

        GatewayTransition Create(CreateCustomGameContract packet)
        {
            // The local enum table establishes modes 1..6. It does not prove a
            // selected track's packaged assets or a server's acceptance.
            sessionId = Guid.NewGuid().ToString("N");
            track = packet.Setting.Track;
            raceMode = packet.Setting.GameMode.RaceMode;
            setting = Copy(packet.Setting); observerMode = packet.ObserverMode; hideGameKey = packet.HideGameKey ?? false;
            joined = false;
            playRequested = false;
            hostMode = false;
            aiSlots.Clear();
            return GatewayTransition.Accept(packet.PacketType, packet.RequestId, "Room draft created locally; no client reply sent.");
        }

        GatewayTransition Join(JoinCustomGameContract packet)
        {
            if (sessionId == null)
                return GatewayTransition.Reject("No room draft exists.", packet.PacketType, packet.RequestId);
            // Empty sessionId was observed immediately after this same client's
            // create request; it is not a general wildcard for other rooms.
            if (packet.SessionId.Length != 0 && !string.Equals(packet.SessionId, sessionId, StringComparison.Ordinal))
                return GatewayTransition.Reject("Session ID does not match the local room.", packet.PacketType, packet.RequestId);
            joined = true;
            return GatewayTransition.Accept(packet.PacketType, packet.RequestId, "Local room joined; PcCustomGame payload remains unresolved.");
        }

        GatewayTransition AddAi(AddCustomGameAiContract packet)
        {
            if (!joined) return GatewayTransition.Reject("Join a local room first.", packet.PacketType, packet.RequestId);
            if (packet.Slot < 1 || packet.Slot > 7) return GatewayTransition.Reject("AI slot must be 1..7; slot zero belongs to the local driver.", packet.PacketType, packet.RequestId);
            if (!aiSlots.Add(packet.Slot))
                return GatewayTransition.Reject("Slot already contains an AI draft.", packet.PacketType, packet.RequestId);
            playRequested = false;
            return GatewayTransition.Accept(packet.PacketType, packet.RequestId, "AI slot recorded locally; no CsMember payload sent.");
        }

        GatewayTransition Play(PlayCustomGameContract packet)
        {
            if (!joined) return GatewayTransition.Reject("Join a local room first.", packet.PacketType, packet.RequestId);
            playRequested = true;
            hostMode = packet.HostMode;
            return GatewayTransition.Accept(packet.PacketType, packet.RequestId, "Play intent recorded; race session handshake unresolved.");
        }
    }

    public sealed class GatewayTransition
    {
        public bool Accepted { get; private set; }
        public string PacketType { get; private set; }
        public object RequestId { get; private set; }
        public string Diagnostic { get; private set; }
        GatewayTransition(bool accepted, string type, object reqId, string diagnostic)
        { Accepted = accepted; PacketType = type; RequestId = reqId; Diagnostic = diagnostic; }
        public static GatewayTransition Accept(string type, object reqId, string diagnostic)
        { return new GatewayTransition(true, type, reqId, diagnostic); }
        public static GatewayTransition Reject(string diagnostic, string type = null, object reqId = null)
        { return new GatewayTransition(false, type, reqId, diagnostic); }
    }
}
