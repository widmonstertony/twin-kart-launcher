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
        Need(Code(Call(store,"PqPreStartTrackSel"))!=0,"Track preload accepted outside a joined room");
        var created=Call(store,"PqCreateCustomGame",",\"observerMode\":false,\"setting\":{\"_type\":\"CsRaceGame\",\"track\":\"Test_R01\",\"gameMode\":{\"raceMode\":4}}");
        var draft=RoomSnapshot(created);
        Need(created.Notices.Count==2 && draft.MemberCount==1,"Room context notification or owner roster missing");
        Need(Code(Call(store,"PqJoinCustomGame",",\"sessionId\":\"foreign\""))!=0,"Foreign room accepted");
        var joined=RoomSnapshot(Call(store,"PqJoinCustomGame",",\"sessionId\":\"\""));
        Need(joined.SessionId==draft.SessionId,"Join replaced room identity");
        Need(Code(Call(store,"PqPreStartTrackSel"))==0,"Joined room could not start native track preload");
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
        var left=Call(store,"PqLeaveCustomGame");
        Need(Code(left)==0,"Room exit failed");
        var invalidated=left.Notices.Find(value=>(string)value["_type"]=="PcCustomGame");
        Need(invalidated!=null && Object.Equals(invalidated["infoValid"],false),"Room exit did not invalidate the client snapshot");
        CustomGamePushShape invalidShape;string invalidError;
        Need(PacketJsonCodec.TryReadCustomGamePush(ProfileJson.Write(invalidated),out invalidShape,out invalidError) && !invalidShape.InfoValid && invalidShape.SessionId==joined.SessionId,
            "Room exit invalidation lost the prior native room identity or shape");
        Need(Code(Call(store,"PqLeaveCustomGame"))==0,"Repeated room exit was not idempotent");
        Need(Code(Call(store,"PqJoinCustomGame",",\"sessionId\":\"\""))!=0,"Exited room survived");
    }
    static void NativeLobbySetupChecks(OfflineProfileStore store)
    {
        var missing=Call(store,"PqRegisterMatch",",\"queue\":[],\"useDefaultLatencyInfo\":true");
        Need(Code(missing)!=0,"Main-lobby setup accepted without an original mode selection");
        var listPacket=ProfileJson.Parse("{\"_type\":\"PqRegisterMatch\",\"reqId\":1,\"queue\":[{\"raceMode\":6,\"profile\":\"\"}],\"trackBanList\":[],\"matchTutorial\":false,\"useDefaultLatencyInfo\":true}");
        var listQueue=new System.Collections.ArrayList();listQueue.Add(((object[])listPacket["queue"])[0]);listPacket["queue"]=listQueue;
        var listOpened=store.Handle(listPacket,1);
        Need(Code(listOpened)==0,"Client IList match queue was rejected");
        var listJoined=RoomSnapshot(Call(store,"PqJoinCustomGame",",\"sessionId\":\"\""));
        Need(listJoined.RaceSession.GameMode.RaceMode==6 && listJoined.AiMemberCount==7,"Client IList lobby mode was not preserved");
        Need(Code(Call(store,"PqLeaveCustomGame"))==0,"IList native setup room did not close cleanly");
        var wrapped=Call(store,"PqRegisterMatch",",\"queue\":{\"gameModes\":[{\"raceMode\":6,\"profile\":\"\"}]},\"trackBanList\":[],\"matchTutorial\":false,\"useDefaultLatencyInfo\":true");
        Need(Code(wrapped)==0,"Current client queue.gameModes envelope was rejected");
        var wrappedJoined=RoomSnapshot(Call(store,"PqJoinCustomGame",",\"sessionId\":\"\""));
        Need(wrappedJoined.RaceSession.GameMode.RaceMode==6 && wrappedJoined.AiMemberCount==7,"Current client queue.gameModes selection was not preserved");
        Need(Code(Call(store,"PqLeaveCustomGame"))==0,"Wrapped native setup room did not close cleanly");
        var opened=Call(store,"PqRegisterMatch",",\"queue\":[{\"raceMode\":5,\"profile\":\"\"}],\"trackBanList\":[],\"matchTutorial\":false,\"useDefaultLatencyInfo\":true");
        Need(Code(opened)==0 && (string)opened.Response["_type"]=="PrRegisterMatch" && opened.Notices.Count==1 && (string)opened.Notices[0]["_type"]=="PcPartyCreateJoinCustomGame","Lobby mode did not enter the native setup surface");
        var joined=RoomSnapshot(Call(store,"PqJoinCustomGame",",\"sessionId\":\"\""));
        Need(joined.RaceSession.GameMode.RaceMode==5 && joined.RaceSession.Track=="Roulette_A01" && joined.AiMemberCount==7,"Native setup lost selected duo mode, random-map choice, or full AI roster");
        Need(Code(Call(store,"PqLeaveCustomGame"))==0,"Native setup room did not close cleanly");
        var recreated=Call(store,"PqRegisterMatch",",\"queue\":[{\"raceMode\":4,\"profile\":\"\"}],\"trackBanList\":[],\"matchTutorial\":false,\"useDefaultLatencyInfo\":true");
        Need(Code(recreated)==0,"Quick-room create-sequence setup failed");
        var createAfterRegister=RoomSnapshot(Call(store,"PqCreateCustomGame",",\"observerMode\":false,\"setting\":{\"_type\":\"CsRaceGame\",\"track\":\"Test_R01\",\"gameMode\":{\"raceMode\":4}}"));
        var joinedAfterCreate=RoomSnapshot(Call(store,"PqJoinCustomGame",",\"sessionId\":\"\""));
        Need(createAfterRegister.AiMemberCount==7 && joinedAfterCreate.MemberCount==8 && joinedAfterCreate.AiMemberCount==7 &&
            joinedAfterCreate.RaceSession.Track=="Test_R01" && joinedAfterCreate.RaceSession.GameMode.RaceMode==4,
            "Register/create/join lost quick-room AI members or valid native selection");
        Need(Code(Call(store,"PqLeaveCustomGame"))==0,"Quick-room create-sequence room did not close cleanly");
        Need(Code(Call(store,"PqSetQuickMatchData",",\"data\":{\"gameModes\":[{\"raceMode\":5,\"profile\":\"\"}],\"grandprix\":\"\"}"))==0,"Original quick mode was not stored");
        var liveSelection=Call(store,"PqRegisterMatch",",\"queue\":[{\"raceMode\":2,\"profile\":\"\"}],\"trackBanList\":[],\"matchTutorial\":false,\"useDefaultLatencyInfo\":true");
        Need(Code(liveSelection)==0,"Live lobby selection was incorrectly replaced by stale saved quick-match data");
        var liveJoined=RoomSnapshot(Call(store,"PqJoinCustomGame",",\"sessionId\":\"\""));
        Need(liveJoined.RaceSession.GameMode.RaceMode==2 && liveJoined.AiMemberCount==7,"Live lobby mode was not authoritative");
        Need(Code(Call(store,"PqLeaveCustomGame"))==0,"Second native setup room did not close cleanly");
    }
    static void VisibleRandomAiRosterChecks()
    {
        var items=new List<string>();var characterRows=new List<string>();var vehicleRows=new List<string>();var bodies=new List<string>();
        for(int index=1;index<=9;++index)
        {
            items.Add("{\"id\":\"Character:"+index+"\",\"quantity\":1}");
            items.Add("{\"id\":\"Costume:"+index+"\",\"quantity\":1}");
            items.Add("{\"id\":\"Kart:"+index+"\",\"quantity\":1}");
            characterRows.Add("\"Character:"+index+"\":{\"Costume\":\"Costume:"+index+"\"}");
            vehicleRows.Add("\"Kart:"+index+"\":{}");bodies.Add("\"Kart:"+index+"\"");
        }
        string unlock="{\"items\":["+String.Join(",",items.ToArray())+"],\"kartBodies\":["+String.Join(",",bodies.ToArray())+"]}";
        string parts="{\"characters\":{"+String.Join(",",characterRows.ToArray())+"},\"vehicles\":{"+String.Join(",",vehicleRows.ToArray())+"}}";
        var nativeVehicles=new List<string>();
        for(int index=1;index<=8;++index)nativeVehicles.Add("\"Kart:"+index+"\":{}");
        string nativeParameters="{\"vehicles\":{"+String.Join(",",nativeVehicles.ToArray())+"}}";
        var availabilityCharacters=new List<string>();var availabilityBodies=new List<string>();
        for(int index=1;index<=9;++index) {
            availabilityCharacters.Add("{\"character\":\"Character:"+index+"\",\"costume\":\"Costume:"+index+"\",\"registered\":"+(index<=8?"1":"0")+"}");
            availabilityBodies.Add("{\"id\":\"Kart:"+index+"\",\"registered\":"+(index<=8?"1":"0")+"}");
        }
        string availability="{\"characters\":["+String.Join(",",availabilityCharacters.ToArray())+"],\"bodies\":["+String.Join(",",availabilityBodies.ToArray())+"]}";
        var service=new OfflineRoomService("1",null,unlock,parts,nativeParameters,availability);
        var profile=ProfileJson.Parse("{\"itemPresetOn\":{\"itemMap\":{\"Character\":\"Character:1\",\"Costume\":\"Costume:1\",\"Kart\":\"Kart:1\"}},\"riderName\":\"Test\"}");
        var login=ProfileJson.Parse("{}");
        Func<string,string,ProfileResult> send=(type,extra)=>service.Handle(ProfileJson.Parse("{\"_type\":\""+type+"\",\"reqId\":1"+extra+"}"),1,profile,login);
        Need(Code(send("PqRegisterMatch",",\"queue\":{\"gameModes\":[{\"raceMode\":6}]},\"trackBanList\":[]"))==0,"Random-roster room registration failed");
        Need(Code(send("PqCreateCustomGame",",\"observerMode\":false,\"setting\":{\"_type\":\"CsRaceGame\",\"track\":\"Test_R01\",\"gameMode\":{\"raceMode\":6}}"))==0,
            "Random-roster quick room create continuation failed");
        var joined=send("PqJoinCustomGame",",\"sessionId\":\"\"");
        var notice=joined.Notices.Find(value=>(string)value["_type"]=="PcCustomGame");
        var members=(System.Collections.IList)ProfileJson.Object(notice,"info")["members"];
        Need(members.Count==8,"Random-roster quick create/join did not preserve seven AI previews");
        var characters=new HashSet<string>(StringComparer.Ordinal);var karts=new HashSet<string>(StringComparer.Ordinal);
        for(int index=1;index<members.Count;++index)
        {
            var member=ProfileJson.Object(members[index]);string character=(string)member["chId"],kart=(string)member["kartId"],costume=(string)member["costume"];
            Need(costume==character.Replace("Character:","Costume:"),"AI character and costume were not paired");
            Need(character!="Character:9","AI room preview selected a character absent from the native client table");
            Need(kart!="Kart:9","AI room preview selected a kart absent from the native PC vehicle table");
            Need(characters.Add(character) && karts.Add(kart),"AI room preview repeated a character or kart");
        }
        Need(characters.Count==7 && karts.Count==7,"AI room preview did not expose seven randomized appearances");
    }
    static void NativeRoomChecks(string folder)
    {
        string control=Path.Combine(folder,"native-room.control"),report=Path.Combine(folder,"native-tracks.json");
        string sha=new string('a',64);
        Func<string,int,int,string> row=(id,mask,laps)=>"{\"TrackId\":\""+id+"\",\"NativeDefinitionStatus\":\"native-definition-present\",\"WorldMatch\":\"exact-base-world-record\",\"AiPathMatch\":\"exact-base-ai-record\",\"NativeModeMask\":"+mask+",\"NativeLaps\":"+laps+"}";
        File.WriteAllText(report,"{\"ClientSha256\":\""+sha+"\",\"NativeDefinitionsComplete\":true,\"Tracks\":["+
            row("Test_R01",6,2)+","+row("Flag_F01",16,999)+","+row("Spitem_S01",8,3)+
            ",{\"TrackId\":\"Missing_R01\",\"NativeDefinitionStatus\":\"no-native-definition-in-client\",\"WorldMatch\":\"unmatched\",\"AiPathMatch\":\"unmatched\",\"NativeModeMask\":0}]}");
        var launch=new NativeRoomLaunch(control,report,sha);
        var service=new OfflineRoomService("1",launch.Start);
        var profile=ProfileJson.Parse("{\"itemPresetOn\":{\"itemMap\":{}},\"riderName\":\"Test\"}");
        var login=ProfileJson.Parse("{}");
        Func<string,string,ProfileResult> send=(type,extra)=>service.Handle(ProfileJson.Parse("{\"_type\":\""+type+"\",\"reqId\":1"+extra+"}"),1,profile,login);
        string setting=",\"setting\":{\"_type\":\"CsRaceGame\",\"track\":\"Test_R01\",\"gameMode\":{\"raceMode\":1},\"lap\":999}";
        Need(Code(send("PqPlayCustomGame",",\"hostMode\":true"))!=0,"Native start accepted outside room");
        send("PqCreateCustomGame",",\"observerMode\":false"+setting);send("PqJoinCustomGame",",\"sessionId\":\"\"");
        for(int slot=1;slot<=7;++slot)send("PqAddCustomGameAiMember",",\"slot\":"+slot);
        Need(Code(send("PqPlayCustomGame",",\"hostMode\":true"))!=0 && !File.Exists(control),"Unready native bridge accepted start");
        File.WriteAllText(control+".ready",new string('b',64));
        Need(Code(send("PqPlayCustomGame",",\"hostMode\":true"))!=0,"Foreign client readiness accepted");
        File.WriteAllText(control+".ready",sha);
        string reportText=File.ReadAllText(report);File.Move(report,report+".waiting");
        Need(Code(send("PqPlayCustomGame",",\"hostMode\":true"))!=0 && !File.Exists(control),"Missing session catalog accepted a room start");
        File.Move(report+".waiting",report);
        File.WriteAllText(report,reportText.Replace(sha,new string('b',64)));
        Need(Code(send("PqPlayCustomGame",",\"hostMode\":true"))!=0 && !File.Exists(control),"Foreign session catalog accepted");
        File.WriteAllText(report,reportText);
        foreach(int mode in new[]{1,2,3,4,5,6,101,102,103,104,105,106,107}) {
            string track=mode==104?"Flag_F01":mode>=105?"Spitem_S01":"Test_R01";
            var changed=send("PqChangeCustomGameSetting",",\"setting\":{\"_type\":\"CsRaceGame\",\"track\":\""+track+"\",\"gameMode\":{\"raceMode\":"+mode+"},\"lap\":999}");
            Need(Code(changed)==0,"Native event mode rejected by room parser");
            var started=send("PqPlayCustomGame",",\"hostMode\":true");
            Need(Code(started)==0 && started.Notices.Count==0,"Native room button failed or fabricated a host/result push");
            string before=File.ReadAllText(control);string[] fields=before.Trim().Split(' ');
            Need(fields.Length==6 && fields[0]=="TKRACE1" && fields[1].Length==32 && fields[2]==track && fields[3]==mode.ToString() && fields[4]=="7" && fields[5]==(mode==104?"3":mode>=105?"3":"2"),"Room selection lost at native loading boundary");
            Need(Code(send("PqPlayCustomGame",",\"hostMode\":true"))!=0 && File.ReadAllText(control)==before,"Repeated click overlapped native loading");
            File.WriteAllText(control+"."+fields[1]+".result","TKRACEACK1 "+fields[1]+" 1\n");
        }
        string saved=File.ReadAllText(control);
        send("PqChangeCustomGameSetting",setting.Replace("Test_R01","Missing_R01"));
        Need(Code(send("PqPlayCustomGame",",\"hostMode\":true"))!=0 && File.ReadAllText(control)==saved,"Explicit missing map was silently replaced");
        send("PqChangeCustomGameSetting",setting.Replace("Test_R01","Flag_F01"));
        Need(Code(send("PqPlayCustomGame",",\"hostMode\":true"))!=0 && File.ReadAllText(control)==saved,"Incompatible map/mode accepted");
        send("PqChangeCustomGameSetting",setting.Replace("Test_R01","Roulette_A01"));
        Need(Code(send("PqPlayCustomGame",",\"hostMode\":true"))==0 && File.ReadAllText(control).Contains(" Test_R01 1 7 2"),"Native random choice drew an incompatible map");
        var fields2=File.ReadAllText(control).Trim().Split(' ');File.WriteAllText(control+"."+fields2[1]+".result","done");
        send("PqChangeCustomGameSetting",setting.Replace("\"raceMode\":1","\"raceMode\":3"));
        send("PqKickCustomGameMember",",\"slot\":7");saved=File.ReadAllText(control);
        Need(Code(send("PqPlayCustomGame",",\"hostMode\":true"))!=0 && File.ReadAllText(control)==saved,"Incomplete native team accepted");
        Console.WriteLine("PASS native custom-room start: original packets, 13 modes, exact maps/default laps/AI, readiness, double-click guard and missing-resource rejection. No game started.");
    }
    static int Main(string[] args)
    {
        string folder = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "test-scratch", "TwinKartBackendTest-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(folder);
        try
        {
            NativeRoomChecks(folder);
            VisibleRandomAiRosterChecks();
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
            NativeLobbySetupChecks(store);
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
            if(args.Length>=1)
            {
                string data=Path.GetFullPath(args[0]);
                Func<string,string> read=name=>ProfileJson.Read(Path.Combine(data,name+".private.json"));
                string availability=args.Length==2?ProfileJson.Read(Path.GetFullPath(args[1])):null;
                var actual=new OfflineProfileStore(Path.Combine(folder,"external-save.json"),read("initial-profile"),read("client-defaults"),read("unlock-catalog"),read("gear-rules"),read("tracks"),read("native-parameters"),read("vehicle-parts"),"100000000000000001",54321,null,availability);
                Need(Code(Call(actual,"PqLogin"))==0,"External login integration failed");
                Need(Code(Call(actual,"PqLoginComplete"))==0,"External native-parameter integration failed");
                Need(((object[])Call(actual,"PqTimeAttackGetTracks").Response["trackList"]).Length==234,"External track count changed");
                if(availability!=null) {
                    Need(Code(Call(actual,"PqRegisterMatch",",\"queue\":{\"gameModes\":[{\"raceMode\":6}]},\"trackBanList\":[]"))==0,"External native-appearance room registration failed");
                    var externalRoom=RoomSnapshot(Call(actual,"PqJoinCustomGame",",\"sessionId\":\"\""));
                    Need(externalRoom.AiMemberCount==7,"External native-appearance evidence did not produce seven visible AI members");
                }
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
