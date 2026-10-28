using System;
using TwinKartLauncher.Gateway;

internal static class PacketContractTests
{
    public static void Run()
    {
        RoomPacketContract request;
        string error;
        const string minimal = @"{""_type"":""PqCreateCustomGame"",""reqId"":7,""observerMode"":false,""setting"":{""_type"":""CsRaceGame"",""track"":""Roulette_A01"",""gameMode"":{""raceMode"":4}}}";
        Check(PacketJsonCodec.TryReadRoomRequest(minimal, out request, out error), "minimal observed create shape: " + error);
        CreateCustomGameContract create = request as CreateCustomGameContract;
        Check(create != null && create.Setting.Track == "Roulette_A01" &&
              create.Setting.GameMode.RaceMode == 4 && create.Setting.Lap == null &&
              create.Setting.GameMode.Profile == null && create.HideGameKey == null,
              "typed create fields and omitted defaults");

        const string optional = @"{""_type"":""PqCreateCustomGame"",""reqId"":""R1"",""observerMode"":false,""hideGameKey"":true,""setting"":{""_type"":""CsRaceGame"",""track"":""Village_R01"",""gameMode"":{""raceMode"":1,""profile"":""""},""lap"":999,""levelOption"":"""",""customGameOption"":""""}}";
        Check(PacketJsonCodec.TryReadRoomRequest(optional, out request, out error), "optional typed create shape: " + error);
        create = request as CreateCustomGameContract;
        Check(create != null && create.HideGameKey == true && create.Setting.Lap == 999 &&
              create.Setting.LevelOption == "" && create.Setting.CustomGameOption == "",
              "present optional fields");
        Check(!PacketJsonCodec.TryReadRoomRequest(optional.Replace("\"hideGameKey\":true", "\"hideGameKey\":\"true\""), out request, out error),
              "string hideGameKey accepted");
        Check(!PacketJsonCodec.TryReadRoomRequest(minimal.Replace("\"raceMode\":4", "\"raceMode\":7"), out request, out error),
              "unrecognized mode accepted");
        Check(!PacketJsonCodec.TryReadRoomRequest(minimal.Replace("\"reqId\":7", "\"reqId\":{}"), out request, out error),
              "object request ID accepted");
        Check(!PacketJsonCodec.TryReadRoomRequest(@"{""_type"":""PqAddCustomGameAiMember"",""reqId"":1,""slot"":2.5}", out request, out error),
              "fractional AI slot accepted");

        CustomGamePushShape push;
        const string room = @"{""_type"":""PcCustomGame"",""sessionId"":""local-1"",""infoValid"":true,""info"":{""session"":{""_type"":""CsRaceGame"",""track"":""Roulette_A01"",""gameMode"":{""raceMode"":4}},""members"":[{""slot"":0,""ai"":false,""accountId"":""player"",""isSpectator"":false},{""slot"":2,""ai"":true}]}}";
        Check(PacketJsonCodec.TryReadCustomGamePush(room, out push, out error), "typed room push: " + error);
        Check(push.InfoValid && push.SessionFieldPresent && push.RaceSession != null &&
              push.MembersFieldPresent && push.MemberCount == 2 && push.AiMemberCount == 1 &&
              push.Members[1].Slot == 2 && push.Members[1].Ai == true,
              "room session and FCsMember array fields");
        Check(!PacketJsonCodec.TryReadCustomGamePush(room.Replace("\"ai\":true", "\"ai\":\"true\""), out push, out error),
              "string member.ai accepted");
        Check(!PacketJsonCodec.TryReadCustomGamePush(room.Replace("\"slot\":2", "\"slot\":2.5"), out push, out error),
              "fractional member slot accepted");
        Check(!PacketJsonCodec.TryReadCustomGamePush(room.Replace("\"members\":[", "\"members\":{\"bad\":1},\"unused\":["), out push, out error),
              "nonarray members accepted");
        Check(!PacketJsonCodec.TryReadCustomGamePush(@"{""_type"":""PcCustomGame"",""sessionId"":""local-1"",""infoValid"":true,""info"":{}}", out push, out error),
              "ready-looking room without session accepted");
        Check(PacketJsonCodec.TryReadCustomGamePush(@"{""_type"":""PcCustomGame"",""sessionId"":""local-1"",""infoValid"":false,""info"":{}}", out push, out error) &&
              !push.SessionFieldPresent && !push.MembersFieldPresent,
              "invalid-info shape rejected");
    }

    static void Check(bool condition, string message)
    {
        if (!condition) throw new Exception(message);
    }
}
