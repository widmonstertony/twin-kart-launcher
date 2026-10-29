using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Security.Cryptography;
using TwinKartLauncher.Gateway;

static class OfflineBackendTests
{
    static void Need(bool value, string message) { if (!value) throw new Exception(message); }
    static ProfileResult Call(OfflineProfileStore store, string type, string extra = "")
    { return store.Handle(ProfileJson.Parse("{\"_type\":\"" + type + "\",\"reqId\":1" + extra + "}"), 1); }
    static int Code(ProfileResult result) { return (int)ProfileJson.Object(result.Response,"result")["code"]; }
    static byte[] Decrypt(string value)
    {
        using (var hash = SHA256.Create()) using (var aes = Aes.Create())
        {
            aes.Key = hash.ComputeHash(Encoding.UTF8.GetBytes("1" + OfflineProfileStore.SharedValue + "_key"));
            var iv = hash.ComputeHash(Encoding.UTF8.GetBytes("1" + OfflineProfileStore.SharedValue + "_iv"));
            var shortIv = new byte[16]; Buffer.BlockCopy(iv,0,shortIv,0,16); aes.IV=shortIv;
            var bytes=Convert.FromBase64String(value);
            using(var decrypt=aes.CreateDecryptor()) return decrypt.TransformFinalBlock(bytes,0,bytes.Length);
        }
    }
    static CustomGamePushShape RoomSnapshot(ProfileResult result)
    {
        Need(Code(result)==0,"Room operation failed");
        var notice=result.Notices.Find(value=>(string)value["_type"]=="PcCustomGame");
        Need(notice!=null,"Missing native room notice");
        CustomGamePushShape shape; string error;
        Need(PacketJsonCodec.TryReadCustomGamePush(ProfileJson.Write(notice),out shape,out error),"Native contract rejected room: "+error);
        return shape;
    }
    static void RoomChecks(OfflineProfileStore store)
    {
        Need(Code(Call(store,"PqCustomGameSetReady",",\"readyState\":1"))!=0,"Readiness accepted outside a joined room");
        Need(Code(Call(store,"PqAddCustomGameAiMember",",\"slot\":1"))!=0,"AI added before room join");
        var created=Call(store,"PqCreateCustomGame",",\"observerMode\":false,\"setting\":{\"_type\":\"CsRaceGame\",\"track\":\"Test_R01\",\"gameMode\":{\"raceMode\":4}}");
        var draft=RoomSnapshot(created);
        Need(created.Notices.Count==2 && draft.MemberCount==1,"Room context notification or owner roster missing");
        Need(Code(Call(store,"PqJoinCustomGame",",\"sessionId\":\"foreign\""))!=0,"Foreign room accepted");
        var joined=RoomSnapshot(Call(store,"PqJoinCustomGame",",\"sessionId\":\"\""));
        Need(joined.SessionId==draft.SessionId,"Join replaced room identity");
        Need(RoomSnapshot(Call(store,"PqCustomGameSetReady",",\"readyState\":1")).Members[0].ReadyState==1,"Native owner readiness not included in room notification");
        Need(Code(Call(store,"PqCustomGameSetReady",",\"readyState\":1.5"))!=0 && Code(Call(store,"PqCustomGameSetReady",",\"readyState\":-1"))!=0,"Malformed readiness accepted");
        Need(Code(Call(store,"PqAddCustomGameAiMember",",\"slot\":0"))!=0,"AI replaced local driver");
        for(int slot=1;slot<=7;slot++)
        {
            var snapshot=RoomSnapshot(Call(store,"PqAddCustomGameAiMember",",\"slot\":"+slot));
            Need(snapshot.AiMemberCount==slot && snapshot.MemberCount==slot+1,"AI roster size incorrect");
            Need(snapshot.Members[0].ReadyState==0,"Roster change retained stale driver readiness");
            var identities=new HashSet<string>(); foreach(var member in snapshot.Members) Need(identities.Add(member.AccountId),"Duplicate local member identity");
        }
        Need(Code(Call(store,"PqAddCustomGameAiMember",",\"slot\":8"))!=0,"Ninth grid slot accepted");
        Need(Code(Call(store,"PqAddCustomGameAiMember",",\"slot\":2"))!=0,"Duplicate AI accepted");
        Need(RoomSnapshot(Call(store,"PqKickCustomGameMember",",\"slot\":2")).AiMemberCount==6,"AI removal failed");
        var changed=RoomSnapshot(Call(store,"PqChangeCustomGameSetting",",\"setting\":{\"_type\":\"CsRaceGame\",\"track\":\"Test_Reverse\",\"gameMode\":{\"raceMode\":1},\"lap\":3}"));
        Need(changed.RaceSession.GameMode.RaceMode==1 && changed.RaceSession.Track=="Test_Reverse" && changed.AiMemberCount==6,"Mode change lost roster or native setting");
        Need(Code(Call(store,"PqPlayCustomGame",",\"hostMode\":true"))!=0 && !store.Snapshot().Contains("\"activeRace\""),"Room draft falsely reported a running AI race");
        Need(Code(Call(store,"PqLeaveCustomGame"))==0,"Room exit failed");
        Need(Code(Call(store,"PqJoinCustomGame",",\"sessionId\":\"\""))!=0,"Exited room survived");
    }
    static int Main(string[] args)
    {
        string folder = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "test-scratch", "TwinKartBackendTest-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(folder);
        try
        {
            string initial="{\"inventory\":{\"inventoryData\":{\"itemList\":[\"Kart:1,1\"]}},\"itemPresetOn\":{\"itemMap\":{\"Kart\":\"Kart:1\"}},\"ownKartsUpgradeInfo\":{\"Kart:1\":{\"engineType\":0,\"grade\":0,\"gearInfo\":{\"gearSettings\":{\"fixed\":2}}}},\"records\":{},\"riderName\":\"Synthetic\"}";
            string defaults="{\"login\":{\"serverRegionId\":\"test\",\"serverSetting\":{},\"itemPresetOn\":{},\"ownKartsUpgradeInfo\":{},\"riderName\":\"\"},\"readOnlyReplies\":{},\"maximumUpgradeGrade\":3,\"maximumUpgradeLevel\":0}";
            string unlock="{\"items\":[{\"id\":\"Kart:1\",\"quantity\":1},{\"id\":\"Character:2\",\"quantity\":1}],\"kartBodies\":[\"Kart:1\"],\"kartUpgrades\":{}}";
            string rules="{\"rules\":{\"fixed\":{\"tunable\":false,\"values\":[0,2,5]}}}";
            string physics="{\"fields\":{\"value\":{\"offset\":0,\"kind\":\"f32\"}},\"vehicles\":{\"Kart:1\":{\"engineType\":0,\"packedBase\":\"AAAgQQ==\",\"settings\":{\"fixed\":2}}},\"effects\":{\"fixed\":{\"2\":{\"value\":6},\"5\":{\"value\":9}}}}";
            string path=Path.Combine(folder,"save.json");
            var store=new OfflineProfileStore(path,initial,defaults,unlock,rules,"{\"tracks\":[\"Test_R01\",\"Test_Reverse\"]}",physics,"{\"vehicles\":{}}","1",54321);
            Need(Code(Call(store,"PqLogin"))==0,"Profile login failed");
            Need(Code(Call(store,"PqReadyGame",",\"ready\":true"))==0 && store.Snapshot().Contains("\"localReady\":true"),"Local lobby readiness rejected");
            string beforeInvalidReady=store.Snapshot();
            Need(Code(Call(store,"PqReadyGame",",\"ready\":\"true\""))!=0 && store.Snapshot()==beforeInvalidReady,"Malformed ready request mutated profile");
            Need(!store.Snapshot().Contains("\"activeRace\""),"Readiness fabricated a race session");
            Need(ProfileJson.Write(Call(store,"PqInventory").Response).Contains("Character:2,1"),"New backend inventory grants absent");
            Need(((object[])Call(store,"PqTimeAttackGetTracks").Response["trackList"]).Length==2,"Track catalogue gates entries");
            var gear=Call(store,"PqKartGearSet",",\"kartItemId\":\"Kart:1\",\"gearSettings\":{\"fixed\":5}");
            Need(Code(gear)==0 && gear.Notices.Count==2,"Original tuning response/notices failed");
            var native=Decrypt((string)gear.Notices[1]["speedBodyParamStr"]);
            Need(native.Length==4 && BitConverter.ToSingle(native,0)==13.0f,"Captured baseline must remove old effect before adding new");
            string previous=store.Snapshot();
            Need(Code(Call(store,"PqKartGearSet",",\"kartItemId\":\"Kart:1\",\"gearSettings\":{\"fixed\":999}"))!=0 && store.Snapshot()==previous,"Invalid plan partially persisted");
            Need(Code(Call(store,"PqTimeAttackStart",",\"trackId\":\"Missing\""))!=0,"Unknown track started");
            var race=Call(store,"PqTimeAttackStart",",\"trackId\":\"Test_R01\"");
            Need(Code(race)==0 && ((string)race.Response["gameId"]).Length==32,"Race identity not issued");
            string raceId=(string)race.Response["gameId"];
            Need(Code(Call(store,"PqTimeAttackUpdate",",\"status\":0,\"gameId\":\"wrong\",\"timeAttack\":{\"trackId\":\"Test_R01\",\"recordTime\":1000}"))!=0,"Foreign race result accepted");
            Need(Code(Call(store,"PqKartUpgrade",",\"kartItemId\":\"Kart:1\""))==0,"Mid-session upgrade setup failed");
            Need(Code(Call(store,"PqTimeAttackUpdate",",\"status\":0,\"gameId\":\""+raceId+"\",\"timeAttack\":{\"trackId\":\"Test_R01\",\"recordTime\":1.5}"))!=0,"Fractional race time accepted");
            var finish=Call(store,"PqTimeAttackUpdate",",\"status\":0,\"gameId\":\""+raceId+"\",\"timeAttack\":{\"trackId\":\"Test_R01\",\"recordTime\":1000}");
            Need(Code(finish)==0 && (bool)finish.Response["isUpdated"] && (string)finish.Response["gameId"]==raceId,"Native result acknowledgement incomplete");
            Need(store.Snapshot().Contains("\"lapTime\":1000"),"Local personal best not persisted");
            string afterFinish=store.Snapshot();
            var retryPacket=ProfileJson.Parse("{\"_type\":\"PqTimeAttackUpdate\",\"reqId\":77,\"bCheat\":false,\"timeAttack\":{\"recordTime\":1000,\"trackId\":\"Test_R01\"},\"status\":0,\"gameId\":\""+raceId+"\"}");
            var retry=store.Handle(retryPacket,77);
            Need(Code(retry)==0 && (int)retry.Response["reqId"]==77 && (bool)retry.Response["isUpdated"] && store.Snapshot()==afterFinish,"Result retry changed state, lost acknowledgement or reused old request correlation");
            Need(Code(Call(store,"PqTimeAttackUpdate",",\"status\":0,\"gameId\":\""+raceId+"\",\"timeAttack\":{\"trackId\":\"Test_R01\",\"recordTime\":999}"))!=0 && store.Snapshot()==afterFinish,"Conflicting completed result accepted");
            var rank=Call(store,"PqTimeAttackGetRankers",",\"trackId\":\"Test_R01\"");
            Need((int)rank.Response["myRank"]==1 && (int)ProfileJson.Object(((List<object>)rank.Response["myRecord"])[0])["kartGrade"]==0,"Record did not preserve starting vehicle configuration");
            Need((int)Call(store,"PqTimeAttackGetRankVersion").Response["version"]==2,"New best did not invalidate ranking cache");
            var abandoned=Call(store,"PqTimeAttackStart",",\"trackId\":\"Test_R01\"");
            string nextActive=store.Snapshot();
            Need(Code(store.Handle(retryPacket,78))==0 && store.Snapshot()==nextActive,"Old result retry consumed a new active race");
            var abandon=Call(store,"PqTimeAttackUpdate",",\"status\":1,\"gameId\":\""+(string)abandoned.Response["gameId"]+"\",\"timeAttack\":{\"trackId\":\"Test_R01\",\"recordTime\":1}");
            Need(Code(abandon)==0 && !(bool)abandon.Response["isUpdated"] && store.Snapshot().Contains("\"lapTime\":1000"),"Abandoned race replaced personal best");
            Need(Code(Call(store,"PqKartUpgrade",",\"kartItemId\":\"Kart:1\""))==0,"Offline upgrade retains a progression/currency gate");
            Need(Code(Call(store,"PqSetQuickMatchData",",\"data\":{\"mode\":\"synthetic\"}"))==0 && store.Snapshot().Contains("synthetic"),"Offline mode choice not persisted");
            Need(Code(Call(store,"PqSetQuickMatchData",",\"data\":17"))!=0,"Invalid mode-choice object accepted");
            var reload=new OfflineProfileStore(path,initial,defaults,unlock,rules,"{\"tracks\":[\"Test_R01\"]}",physics,"{\"vehicles\":{}}","1",54321);
            Need(reload.Snapshot()==store.Snapshot(),"Atomic persistence or restart lost state");
            string beforeReplay=reload.Snapshot();
            Need(Code(reload.Handle(retryPacket,79))==0 && reload.Snapshot()==beforeReplay,"Completed result acknowledgement lost after restart");
            for(int index=0;index<17;index++) {
                var boundedRace=Call(reload,"PqTimeAttackStart",",\"trackId\":\"Test_R01\"");
                Need(Code(Call(reload,"PqTimeAttackUpdate",",\"status\":1,\"gameId\":\""+(string)boundedRace.Response["gameId"]+"\",\"timeAttack\":{\"trackId\":\"Test_R01\",\"recordTime\":0}"))==0,"Abandonment journal fixture failed");
            }
            Need(((object[])ProfileJson.Parse(reload.Snapshot())["raceResultHistory"]).Length==16 && Code(reload.Handle(retryPacket,80))!=0,"Result history unbounded or expired result replayed");
            Need(Code(Call(store,"PqUnsupported"))!=0,"Unknown operation falsely succeeded");
            RoomChecks(store);
            if(args.Length==1)
            {
                string data=Path.GetFullPath(args[0]);
                Func<string,string> read=name=>ProfileJson.Read(Path.Combine(data,name+".private.json"));
                var actual=new OfflineProfileStore(Path.Combine(folder,"external-save.json"),read("initial-profile"),read("client-defaults"),read("unlock-catalog"),read("gear-rules"),read("tracks"),read("native-parameters"),read("vehicle-parts"),"100000000000000001",54321);
                Need(Code(Call(actual,"PqLogin"))==0,"External login integration failed");
                Need(Code(Call(actual,"PqLoginComplete"))==0,"External native-parameter integration failed");
                Need(((object[])Call(actual,"PqTimeAttackGetTracks").Response["trackList"]).Length==234,"External track count changed");
                Console.WriteLine("PASS external client dataset: original profile, native body payload and 234 track rows; no game started.");
            }
            Console.WriteLine("PASS original backend: inventory, fixed-field tuning, native effect rebasing, atomic rollback/restart and track/session validation.");
            Console.WriteLine("PASS native room adapter: create/join, seven AI slots, readiness, kick, mode change, identity isolation and explicit race-not-ready boundary.");
            Console.WriteLine("PASS result replay: stable acknowledgement across request IDs, property ordering and restart; conflicting result rejected; new active race preserved.");
            return 0;
        }
        catch(Exception error) { Console.Error.WriteLine(error); return 1; }
        finally { Directory.Delete(folder,true); }
    }
}
