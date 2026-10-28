using System;
using TwinKartLauncher.Gateway;

internal static class RoomStateTests
{
    static int Main()
    {
        try
        {
            RoomState room = new RoomState();
            Check(!room.Apply("{\"_type\":\"PqJoinCustomGame\",\"reqId\":1,\"sessionId\":\"\"}").Accepted, "join without room");
            Check(!room.Apply("{\"_type\":\"PqCreateCustomGame\",\"reqId\":2,\"setting\":{\"_type\":\"CsRaceGame\",\"track\":\"Roulette_A01\",\"gameMode\":{\"raceMode\":99}}}").Accepted, "unknown race mode");
            Check(!room.Apply("{\"_type\":\"PqCreateCustomGame\",\"reqId\":3,\"setting\":{\"_type\":\"CsRaceGame\",\"track\":\"../unsafe\",\"gameMode\":{\"raceMode\":4}}}").Accepted, "invalid track ID");
            GatewayTransition created = room.Apply("{\"_type\":\"PqCreateCustomGame\",\"reqId\":4,\"observerMode\":false,\"setting\":{\"_type\":\"CsRaceGame\",\"track\":\"Roulette_A01\",\"gameMode\":{\"raceMode\":4}}}");
            Check(created.Accepted && (int)created.RequestId == 4, "create and correlation");
            Check(room.SessionId != null && room.SessionId.Length == 32 && room.RaceMode == 4 && room.Track == "Roulette_A01", "room draft state");
            Check(!room.Apply("{\"_type\":\"PqJoinCustomGame\",\"reqId\":5,\"sessionId\":\"other\"}").Accepted, "wrong session");
            Check(room.Apply("{\"_type\":\"PqJoinCustomGame\",\"reqId\":6,\"sessionId\":\"\"}").Accepted && room.Joined, "observed empty join ID");
            Check(!room.Apply("{\"_type\":\"PqAddCustomGameAiMember\",\"reqId\":7,\"slot\":-1}").Accepted, "negative slot");
            Check(room.Apply("{\"_type\":\"PqAddCustomGameAiMember\",\"reqId\":8,\"slot\":2}").Accepted && room.HasAiSlot(2), "add AI slot");
            Check(!room.Apply("{\"_type\":\"PqAddCustomGameAiMember\",\"reqId\":9,\"slot\":2}").Accepted, "duplicate AI slot");
            Check(!room.Apply("{\"_type\":\"PqPlayCustomGame\",\"reqId\":10,\"hostMode\":\"true\"}").Accepted, "string hostMode");
            Check(room.Apply("{\"_type\":\"PqPlayCustomGame\",\"reqId\":11,\"hostMode\":true}").Accepted && room.PlayRequested && room.HostMode, "play intent");
            Check(!room.Apply("{\"_type\":\"PqUnobserved\",\"reqId\":12}").Accepted, "unmodeled operation accepted");
            Check(!room.Apply("not json").Accepted, "invalid JSON accepted");
            Check(!room.Apply("{\"_type\":\"PqJoinCustomGame\",\"sessionId\":\"\"}").Accepted, "missing reqId accepted");
            string oldSession = room.SessionId;
            Check(room.Apply("{\"_type\":\"PqCreateCustomGame\",\"reqId\":13,\"observerMode\":false,\"setting\":{\"_type\":\"CsRaceGame\",\"track\":\"Village_R01\",\"gameMode\":{\"raceMode\":1}}}").Accepted, "second room create");
            Check(room.SessionId != oldSession && !room.Joined && !room.PlayRequested && room.AiCount == 0, "new room did not reset state");
            PacketContractTests.Run();
            Console.WriteLine("PASS room state transitions and strict input validation");
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine("FAIL " + error);
            return 1;
        }
    }

    static void Check(bool condition, string message)
    {
        if (!condition) throw new Exception(message);
    }
}
