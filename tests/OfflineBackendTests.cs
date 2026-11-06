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
        Need(Object.Equals(created.Notices[0]["sessionId"],draft.SessionId),"Native create notice omitted its actual room identity");
        Need(created.Notices.Count==2 && draft.MemberCount==1,"Room context notification or owner roster missing");
        Need(Code(Call(store,"PqJoinCustomGame",",\"sessionId\":\"foreign\""))!=0,"Foreign room accepted");
        var joined=RoomSnapshot(Call(store,"PqJoinCustomGame",",\"sessionId\":\"\""));
        Need(joined.SessionId==draft.SessionId,"Join replaced room identity");
        Need(Code(Call(store,"PqPreStartTrackSel"))!=0,"Draft without a native bridge falsely accepted the Start button");
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
        Need(Code(Call(store,"PqRegisterMatch",",\"queue\":{\"gameModes\":[{\"raceMode\":113,\"profile\":\"speed_solo_ghost\"}]}"))==0,"Native event lobby selection rejected");
        Need(Code(Call(store,"PqCreateCustomGame",",\"observerMode\":false,\"setting\":{\"_type\":\"CsRaceGame\",\"track\":\"Roulette_A01\",\"gameMode\":{\"raceMode\":113,\"profile\":\"\"}}"))==0,"Native event quick create continuation rejected");
        var eventJoined=RoomSnapshot(Call(store,"PqJoinCustomGame",",\"sessionId\":\"\""));
        Need(eventJoined.RaceSession.GameMode.RaceMode==113 && eventJoined.RaceSession.GameMode.Profile=="speed_solo_ghost", "Quick-room boundary dropped the event profile");
        Need(Code(Call(store,"PqLeaveCustomGame"))==0,"Native event room did not close cleanly");
        var opened=Call(store,"PqRegisterMatch",",\"queue\":[{\"raceMode\":5,\"profile\":\"\"}],\"trackBanList\":[],\"matchTutorial\":false,\"useDefaultLatencyInfo\":true");
        Need(Code(opened)==0 && (string)opened.Response["_type"]=="PrRegisterMatch" && opened.Notices.Count==1 && (string)opened.Notices[0]["_type"]=="PcPartyCreateJoinCustomGame","Lobby mode did not enter the native setup surface");
        var joined=RoomSnapshot(Call(store,"PqJoinCustomGame",",\"sessionId\":\"\""));
        Need(joined.RaceSession.GameMode.RaceMode==5 && joined.RaceSession.Track=="Roulette_A01" && joined.AiMemberCount==7,"Native setup lost selected duo mode, random-map choice, or full AI roster");
        Need(Object.Equals(opened.Notices[0]["sessionId"],joined.SessionId),"Lobby redirect supplied a blank/different room identity");
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
            Need(character!="Character:1" && kart!="Kart:1","AI room preview copied the human's character or kart");
            Need(characters.Add(character) && karts.Add(kart),"AI room preview repeated a character or kart");
        }
        Need(characters.Count==7 && karts.Count==7,"AI room preview did not expose seven randomized appearances");
        // These circuit profiles prescribe skills, not visible team clones.
        // The fixture deliberately lacks their old fixed character IDs: a
        // preview must retain the verified random catalog and human outfit.
        foreach(string entry in new[]{"112:item_squad_miso_tiera","112:s062_item_squad_runner_attacker",
            "110:item_solo_brodi","113:s062_speed_solo_dao","113:speed_solo_derek"}) {
            string[] fields=entry.Split(':');string mode=fields[0],eventProfile=fields[1];
            Need(Code(send("PqLeaveCustomGame",""))==0,"Random event room cleanup failed");
            Need(Code(send("PqRegisterMatch",",\"queue\":{\"gameModes\":[{\"raceMode\":"+mode+",\"profile\":\""+eventProfile+"\"}]},\"trackBanList\":[]"))==0,
                "Event skill identity replaced or rejected the random room preview");
            var eventJoined=send("PqJoinCustomGame",",\"sessionId\":\"\"");
            var eventNotice=eventJoined.Notices.Find(value=>(string)value["_type"]=="PcCustomGame");
            var eventMembers=(System.Collections.IList)ProfileJson.Object(eventNotice,"info")["members"];
            Need(eventMembers.Count==8 && (string)ProfileJson.Object(eventMembers[0])["chId"]=="Character:1","Event preview changed the human or population");
            var eventCharacters=new HashSet<string>(StringComparer.Ordinal);var eventKarts=new HashSet<string>(StringComparer.Ordinal);
            for(int seat=1;seat<8;++seat) {
                var member=ProfileJson.Object(eventMembers[seat]);string character=(string)member["chId"],kart=(string)member["kartId"];
                Need(character!="Character:1" && kart!="Kart:1" && eventCharacters.Add(character) && eventKarts.Add(kart),
                    "Event preview collapsed per-seat random appearances or copied the human");
            }
            var refreshed=send("PqCustomGameSetReady",",\"readyState\":1");
            var refreshedNotice=refreshed.Notices.Find(value=>(string)value["_type"]=="PcCustomGame");
            var refreshedMembers=(System.Collections.IList)ProfileJson.Object(refreshedNotice,"info")["members"];
            for(int seat=0;seat<8;++seat)Need((string)ProfileJson.Object(eventMembers[seat])["chId"]==(string)ProfileJson.Object(refreshedMembers[seat])["chId"] &&
                (string)ProfileJson.Object(eventMembers[seat])["kartId"]==(string)ProfileJson.Object(refreshedMembers[seat])["kartId"],"Ready notification rerolled an event appearance");
        }
    }
    static void PrototypeScheduleChecks()
    {
        for(int mask=0;mask<8;++mask) {
            bool factory=(mask&1)!=0,chaser=(mask&2)!=0,hide=(mask&4)!=0;
            var catalog=new DynamicDataCatalog(new DynamicDataDocument[0],true,chaser,hide,factory);
            Dictionary<string,object> reply;
            Need(catalog.TryReply("ModeLabSet",1,out reply),"Normal mode schedule missing");
            var doc=ProfileJson.Parse((string)reply["data"]);var rows=(object[])doc["events"];
            Need(rows.Length==28+(factory?1:0)+(chaser?1:0)+(hide?1:0),"Combined native/backend event count differs");
            var ids=new HashSet<string>(StringComparer.Ordinal);int seen=0;
            for(int i=0;i<rows.Length;++i) {
                var row=ProfileJson.Object(rows[i]);string id=(string)row["id"];
                Need(ids.Add(id) && (int)row["sortIndex"]==i+1,"Duplicate or unstable mode schedule ordering");
                var gameMode=ProfileJson.Object(row["gameMode"]);int mode=(int)gameMode["raceMode"];
                if(i<3 || (i>=15 && i<27)) {
                    int expected=new[]{101,102,103,104,105,106,107,110,111,112,113,114,115,116,118}[i<3?i:i-12];
                    Need(mode==expected && id=="TwinKart_LocalMode_"+expected.ToString() && Object.Equals(gameMode["profile"],""),"Lobby omitted or changed a native custom mode");
                }
                if(id=="TwinKart_FactoryRun"){seen|=1;Need(mode==100 && Object.Equals(row["noParty"],false),"Factory native cooperative schedule differs");}
                if(id=="TwinKart_Chaser"){seen|=2;Need(mode==108 && Object.Equals(row["noParty"],false),"Chaser native schedule differs");}
                if(id=="TwinKart_Hide_Repaired"){seen|=4;Need(mode==109 && Object.Equals(row["noParty"],true),"Hide native schedule differs");}
                if(i==rows.Length-1)Need(id=="TwinKart_FreeDrive" && mode==1 && Object.Equals(gameMode["profile"],NativeRaceOptions.FreeDriveProfile),"Free drive profile or menu position lost");
            }
            Need(seen==mask,"Prototype session gate leaked or lost a normal menu entry");
        }
        var disabled=new DynamicDataCatalog(new DynamicDataDocument[0],false,true,true,true);Dictionary<string,object> ignored;
        Need(!disabled.TryReply("ModeLabSet",1,out ignored),"Offline prototype schedule leaked into a regular catalog");
        Console.WriteLine("PASS Factory/Chaser/Hide schedule: all eight gate combinations, native menu order/counts, party contracts and disabled-catalog isolation.");
    }
    static void HideRoomChecks(string folder)
    {
        string control=Path.Combine(folder,"hide-room.control"),report=Path.Combine(folder,"not-created-hide-tracks.json"),sha=new string('b',64);
        File.WriteAllText(control+".ready",sha);
        var launch=new NativeRoomLaunch(control,report,sha,false,false,true);
        var service=new OfflineRoomService("1",launch.Start);
        var profile=ProfileJson.Parse("{\"itemPresetOn\":{\"itemMap\":{}},\"riderName\":\"Test\"}");var login=ProfileJson.Parse("{}");
        Func<string,string,ProfileResult> send=(type,extra)=>service.Handle(ProfileJson.Parse("{\"_type\":\""+type+"\",\"reqId\":1"+extra+"}"),1,profile,login);
        const string queue=",\"queue\":{\"gameModes\":[{\"raceMode\":109,\"profile\":\"\"}]}";
        var opened=send("PqRegisterMatch",queue);
        Need(Code(opened)==0 && opened.Notices.Count==0 && (string)opened.Response["mmTicket"]!=null,"Hide lobby did not request direct original travel");
        string text=File.ReadAllText(control);var fields=text.Trim().Split(' ');
        Need(fields.Length==2 && fields[0]=="TKHIDE_REPAIRED1" && Guid.ParseExact(fields[1],"N")!=Guid.Empty,"Exact normal Hide request malformed");
        Need(!File.Exists(report),"Hide fabricated or required a circuit catalog");
        Need(Code(send("PqRegisterMatch",queue))!=0 && File.ReadAllText(control)==text,"Pending Hide travel overwritten");
        var room=new RoomState();room.PrepareQuickRoom(109,"HideNSeekTestLevel");room.JoinPreparedHideScene();
        Need(room.Joined && room.HostMode && room.AiCount==7,"Dedicated Hide local roster malformed");
        Need(new NativeRoomLaunch(control,report,sha).Start(room)!=null && File.ReadAllText(control)==text,"Hide escaped explicit normal gates");
        File.WriteAllText(control+"."+fields[1]+".result","done");
        Need(room.Apply("{\"_type\":\"PqChangeCustomGameSetting\",\"reqId\":1,\"setting\":{\"_type\":\"CsRaceGame\",\"track\":\"Flag_F01\",\"gameMode\":{\"raceMode\":109}}}").Accepted && launch.Start(room)!=null && File.ReadAllText(control)==text,"Hide silently aliased another world");
        room=new RoomState();room.PrepareQuickRoom(109,"HideNSeekTestLevel");room.JoinPreparedHideScene();
        Need(room.Apply("{\"_type\":\"PqKickCustomGameMember\",\"reqId\":1,\"slot\":7}").Accepted && launch.Start(room)!=null,"Incomplete Hide roster admitted");
        Need(Code(send("PqRegisterMatch",queue.Replace("\"profile\":\"\"","\"profile\":\"other\"")))!=0,"Unowned Hide profile admitted");
        var disabled=new OfflineRoomService("1",new NativeRoomLaunch(control,report,sha).Start);
        Func<string,string,ProfileResult> denied=(type,extra)=>disabled.Handle(ProfileJson.Parse("{\"_type\":\""+type+"\",\"reqId\":1"+extra+"}"),1,profile,login);
        Need(Code(denied("PqRegisterMatch",",\"queue\":[{\"raceMode\":4}]"))==0,"Ordinary lobby regressed");
        var before=RoomSnapshot(denied("PqJoinCustomGame",",\"sessionId\":\"\""));
        Need(Code(denied("PqRegisterMatch",queue))!=0,"Disabled Hide accepted");
        var after=RoomSnapshot(denied("PqCustomGameSetReady",",\"readyState\":1"));
        Need(before.SessionId==after.SessionId && after.RaceSession.GameMode.RaceMode==4 && File.ReadAllText(control)==text,"Failed Hide registration destroyed the previous room");
        Dictionary<string,object> reply;
        var catalog=new DynamicDataCatalog(new DynamicDataDocument[0],true,false,true);
        Need(catalog.TryReply("ModeLabSet",1,out reply) && ((string)reply["data"]).Contains("TwinKart_Hide_Repaired"),"Normal Hide schedule missing");
        catalog=new DynamicDataCatalog(new DynamicDataDocument[0],true);
        Need(catalog.TryReply("ModeLabSet",1,out reply) && !((string)reply["data"]).Contains("TwinKart_Hide_Repaired"),"Disabled Hide schedule leaked");
        Console.WriteLine("PASS normal Hide lobby: direct exact-world travel, seven AI, no fake circuit row, pending/gate/profile/map/roster rejection and prior-room preservation; no game executed.");
    }
    static void ChaserRoomChecks(string folder)
    {
        string control=Path.Combine(folder,"chaser-room.control"),report=Path.Combine(folder,"chaser-tracks.json"),sha=new string('a',64);
        Func<string,string> row=id=>"{\"TrackId\":\""+id+"\",\"NativeDefinitionStatus\":\"native-definition-present\",\"WorldMatch\":\"exact-base-world-record\",\"AiPathMatch\":\"unmatched\",\"NativeModeMask\":16,\"NativeLaps\":999}";
        File.WriteAllText(report,"{\"ClientSha256\":\""+sha+"\",\"NativeDefinitionsComplete\":true,\"Tracks\":["+row("Flag_F01")+","+row("Flag_F02")+"]}");
        File.WriteAllText(control+".ready",sha);
        var launch=new NativeRoomLaunch(control,report,sha,false,true);
        var service=new OfflineRoomService("1",launch.Start);
        var profile=ProfileJson.Parse("{\"itemPresetOn\":{\"itemMap\":{}},\"riderName\":\"Test\"}");var login=ProfileJson.Parse("{}");
        Func<string,string,ProfileResult> send=(type,extra)=>service.Handle(ProfileJson.Parse("{\"_type\":\""+type+"\",\"reqId\":1"+extra+"}"),1,profile,login);
        Need(Code(send("PqRegisterMatch",",\"queue\":{\"gameModes\":[{\"raceMode\":108,\"profile\":\"\"}]}"))==0,"Chaser lobby registration failed");
        var joined=RoomSnapshot(send("PqJoinCustomGame",",\"sessionId\":\"\""));
        Need(joined.MemberCount==8 && joined.AiMemberCount==7 && joined.RaceSession.Track=="Flag_F01" && joined.RaceSession.GameMode.RaceMode==108,
            "Chaser lobby lost its exact arena or seven AI previews");
        Need(Code(send("PqPlayCustomGame",",\"hostMode\":true"))==0,"Chaser Play was treated as an impossible sixteen-per-team race");
        string text=File.ReadAllText(control);string[] fields=text.Trim().Split(' ');
        Need(fields.Length==6 && fields[0]=="TKRACE1" && fields[2]=="Flag_F01" && fields[3]=="108" && fields[4]=="7","Chaser source request malformed");
        File.WriteAllText(control+"."+fields[1]+".result","done");
        var room=new RoomState();room.PrepareQuickRoom(108,"Flag_F01");
        Need(room.Apply("{\"_type\":\"PqJoinCustomGame\",\"reqId\":1,\"sessionId\":\"\"}").Accepted,"Chaser isolated room join failed");
        Need(new NativeRoomLaunch(control,report,sha).Start(room)!=null && File.ReadAllText(control)==text,"Chaser escaped the explicit session gate");
        Need(room.Apply("{\"_type\":\"PqChangeCustomGameSetting\",\"reqId\":1,\"setting\":{\"_type\":\"CsRaceGame\",\"track\":\"Flag_F02\",\"gameMode\":{\"raceMode\":108}}}").Accepted &&
            launch.Start(room)!=null && File.ReadAllText(control)==text,"Unprepared arena was aliased to Chaser");
        room=new RoomState();room.PrepareQuickRoom(108,"Flag_F01");
        Need(room.Apply("{\"_type\":\"PqJoinCustomGame\",\"reqId\":1,\"sessionId\":\"\"}").Accepted,"Environment gate room join failed");
        string[] keys={"TWIN_KART_ROOM_RACE_CONTROL","TWIN_KART_ROOM_TRACK_REPORT","TWIN_KART_ROOM_CLIENT_SHA256","TWIN_KART_CHASER_COMPAT","TWIN_KART_CHASER_ARENA"};
        var saved=new Dictionary<string,string>();foreach(string key in keys)saved[key]=Environment.GetEnvironmentVariable(key);
        try {
            Environment.SetEnvironmentVariable(keys[0],control);Environment.SetEnvironmentVariable(keys[1],report);Environment.SetEnvironmentVariable(keys[2],sha);
            for(int mask=0;mask<4;++mask) {
                Environment.SetEnvironmentVariable(keys[3],(mask&1)!=0?"1":"0");Environment.SetEnvironmentVariable(keys[4],(mask&2)!=0?"1":"0");
                string before=File.ReadAllText(control);string failure=NativeRoomLaunch.FromEnvironment().Start(room);
                Need((failure==null)==(mask==3),"Chaser native launch accepted only half its environment capability pair");
                if(mask!=3)Need(File.ReadAllText(control)==before,"Disabled native mode changed the pending command");
            }
        } finally {foreach(string key in keys)Environment.SetEnvironmentVariable(key,saved[key]);}
        Console.WriteLine("PASS Chaser lobby source flow: original room, eight previews, exact arena, seven AI, native start intent, disabled-session rejection and unprepared-map isolation; no game executed.");
    }
    static void FactoryRoomChecks(string folder)
    {
        string control=Path.Combine(folder,"factory-room.control"),report=Path.Combine(folder,"factory-tracks.json"),sha=new string('a',64);
        Func<string,string> row=id=>"{\"TrackId\":\""+id+"\",\"NativeDefinitionStatus\":\"native-definition-present\",\"WorldMatch\":\"exact-base-world-record\",\"AiPathMatch\":\"exact-base-ai-record\",\"NativeModeMask\":6,\"NativeLaps\":5}";
        File.WriteAllText(report,"{\"ClientSha256\":\""+sha+"\",\"NativeDefinitionsComplete\":true,\"Tracks\":["+row("Factory_I01")+","+row("Test_R01")+"]}");
        string quickControl=Path.Combine(folder,"factory-lobby.control");File.WriteAllText(quickControl+".ready",sha);
        var quickLaunch=new NativeRoomLaunch(quickControl,report,sha,true);
        var quickService=new OfflineRoomService("1",quickLaunch.Start);
        var quickProfile=ProfileJson.Parse("{\"itemPresetOn\":{\"itemMap\":{}},\"riderName\":\"Test\"}");
        var quickLogin=ProfileJson.Parse("{}");
        Func<string,string,ProfileResult> quickSend=(type,extra)=>quickService.Handle(
            ProfileJson.Parse("{\"_type\":\""+type+"\",\"reqId\":1"+extra+"}"),1,quickProfile,quickLogin);
        var registered=quickSend("PqRegisterMatch",",\"queue\":{\"gameModes\":[{\"raceMode\":100,\"profile\":\"\"}]},\"useDefaultLatencyInfo\":true");
        Need(Code(registered)==0 && registered.Notices.Exists(x=>(string)x["_type"]=="PcPartyCreateJoinCustomGame"),
            "Factory lobby tile did not enter the original room flow");
        var quickJoined=RoomSnapshot(quickSend("PqJoinCustomGame",",\"sessionId\":\"\""));
        Need(quickJoined.MemberCount==4 && quickJoined.AiMemberCount==3 && quickJoined.RaceSession.Track=="Factory_I01" &&
            quickJoined.RaceSession.GameMode.RaceMode==100,"Factory lobby entry lost its map or cooperative roster");
        Need(Code(quickSend("PqPlayCustomGame",",\"hostMode\":true"))==0,"Factory lobby room could not submit Play");
        string[] quickFields=File.ReadAllText(quickControl).Trim().Split(' ');
        Need(quickFields.Length==3 && quickFields[0]=="TKFACTORY1" && quickFields[2]=="Factory_I01",
            "Factory lobby Play escaped its separate native population contract");
        File.WriteAllText(control+".ready",sha);
        var room=new RoomState();room.PrepareQuickRoom(100,"Factory_I01");
        Need(room.MaximumRacers==4 && room.AiCount==3,"Factory room used the ordinary eight-seat grid");
        Need(room.Apply("{\"_type\":\"PqJoinCustomGame\",\"reqId\":1,\"sessionId\":\"\"}").Accepted,"Factory room join failed");
        Need(!room.Apply("{\"_type\":\"PqAddCustomGameAiMember\",\"reqId\":1,\"slot\":4}").Accepted,"Factory room treated an Android as a cooperative seat");
        Need(new NativeRoomLaunch(control,report,sha).Start(room)!=null && !File.Exists(control),"Factory start escaped its manifest gate");
        var launch=new NativeRoomLaunch(control,report,sha,true);
        Need(launch.Start(room)==null,"Factory native request was not written");
        string[] fields=File.ReadAllText(control).Trim().Split(' ');
        Need(fields.Length==3 && fields[0]=="TKFACTORY1" && fields[2]=="Factory_I01","Factory start used ordinary population/options");
        string saved=File.ReadAllText(control);File.WriteAllText(control+"."+fields[1]+".result","done");
        Need(room.Apply("{\"_type\":\"PqKickCustomGameMember\",\"reqId\":1,\"slot\":3}").Accepted,"Factory AI kick failed");
        Need(launch.Start(room)!=null && File.ReadAllText(control)==saved,"Factory accepted an incomplete cooperative roster");
        Need(room.Apply("{\"_type\":\"PqAddCustomGameAiMember\",\"reqId\":1,\"slot\":3}").Accepted,"Factory cooperative seat restore failed");
        foreach(string setting in new[]{
            "\"track\":\"Test_R01\",\"gameMode\":{\"raceMode\":100}",
            "\"track\":\"Factory_I01\",\"gameMode\":{\"raceMode\":100},\"lap\":3",
            "\"track\":\"Factory_I01\",\"gameMode\":{\"raceMode\":100,\"profile\":\"Other\"}",
            "\"track\":\"Factory_I01\",\"gameMode\":{\"raceMode\":100},\"customGameOption\":\"?game.useskill=true\""}) {
            Need(room.Apply("{\"_type\":\"PqChangeCustomGameSetting\",\"reqId\":1,\"setting\":{\"_type\":\"CsRaceGame\","+setting+"}}").Accepted,"Factory rejection fixture could not set room");
            Need(launch.Start(room)!=null && File.ReadAllText(control)==saved,"Factory accepted arbitrary map/round/profile/rule values");
        }
        Need(room.Apply("{\"_type\":\"PqChangeCustomGameSetting\",\"reqId\":1,\"setting\":{\"_type\":\"CsRaceGame\",\"track\":\"Test_R01\",\"gameMode\":{\"raceMode\":6}}}").Accepted,"Factory-to-circuit room change failed");
        Need(room.AiCount==7 && room.MaximumRacers==8,"Factory room limits leaked into ordinary squad");
        Console.WriteLine("PASS Factory room: four racer seats, separate 36-Android contract, exact map/rules, manifest gate and circuit restoration. No game started.");
    }
    static void NativeRoomChecks(string folder)
    {
        string control=Path.Combine(folder,"native-room.control"),report=Path.Combine(folder,"native-tracks.json");
        string sha=new string('a',64);
        Func<string,int,int,string> row=(id,mask,laps)=>"{\"TrackId\":\""+id+"\",\"NativeDefinitionStatus\":\"native-definition-present\",\"WorldMatch\":\"exact-base-world-record\",\"AiPathMatch\":\"exact-base-ai-record\",\"NativeModeMask\":"+mask+",\"NativeLaps\":"+laps+"}";
        File.WriteAllText(report,"{\"ClientSha256\":\""+sha+"\",\"NativeDefinitionsComplete\":true,\"Tracks\":["+
            row("Test_R01",6,2)+","+row("Desert_I06",2,2)+","+row("Flag_F01",16,999)+","+row("Spitem_S01",8,3)+
            ",{\"TrackId\":\"Missing_R01\",\"NativeDefinitionStatus\":\"no-native-definition-in-client\",\"WorldMatch\":\"unmatched\",\"AiPathMatch\":\"unmatched\",\"NativeModeMask\":0}]}");
        var launch=new NativeRoomLaunch(control,report,sha);
        var service=new OfflineRoomService("1",launch.Start);
        var profile=ProfileJson.Parse("{\"itemPresetOn\":{\"itemMap\":{}},\"riderName\":\"Test\"}");
        var login=ProfileJson.Parse("{}");
        Func<string,string,ProfileResult> send=(type,extra)=>service.Handle(ProfileJson.Parse("{\"_type\":\""+type+"\",\"reqId\":1"+extra+"}"),1,profile,login);
        string setting=",\"setting\":{\"_type\":\"CsRaceGame\",\"track\":\"Test_R01\",\"gameMode\":{\"raceMode\":1},\"lap\":999}";
        Need(Code(send("PqPlayCustomGame",",\"hostMode\":true"))!=0,"Native start accepted outside room");
        var customCreated=RoomSnapshot(send("PqCreateCustomGame",",\"observerMode\":false"+setting));
        var customJoined=RoomSnapshot(send("PqJoinCustomGame",",\"sessionId\":\"\""));
        Need(customCreated.AiMemberCount==7 && customJoined.AiMemberCount==7,"Native custom room did not populate AI previews without an AddAI menu");
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
        foreach(int mode in new[]{1,2,3,4,5,6,101,102,103,104,105,106,107,110,111,112,113,114,115,116,118}) {
            string track=mode==104?"Flag_F01":mode>=105 && mode<=107?"Spitem_S01":mode==116 || mode==118?"Desert_I06":"Test_R01";
            var changed=send("PqChangeCustomGameSetting",",\"setting\":{\"_type\":\"CsRaceGame\",\"track\":\""+track+"\",\"gameMode\":{\"raceMode\":"+mode+"},\"lap\":999}");
            Need(Code(changed)==0,"Native event mode rejected by room parser");
            var started=send("PqPlayCustomGame",",\"hostMode\":true");
            Need(Code(started)==0 && started.Notices.Count==0,"Native room button failed or fabricated a host/result push");
            string before=File.ReadAllText(control);string[] fields=before.Trim().Split(' ');
            Need(fields.Length==6 && fields[0]=="TKRACE1" && fields[1].Length==32 && fields[2]==track && fields[3]==mode.ToString() && fields[4]=="7" && fields[5]==(mode==104 || mode>=105 && mode<=107?"3":"2"),"Room selection lost at native loading boundary");
            Need(Code(send("PqPlayCustomGame",",\"hostMode\":true"))!=0 && File.ReadAllText(control)==before,"Repeated click overlapped native loading");
            File.WriteAllText(control+"."+fields[1]+".result","TKRACEACK1 "+fields[1]+" 1\n");
        }
        string saved=File.ReadAllText(control);
        send("PqChangeCustomGameSetting",setting.Replace("\"raceMode\":1","\"raceMode\":116"));
        Need(Code(send("PqPlayCustomGame",",\"hostMode\":true"))!=0 && File.ReadAllText(control)==saved,"Cube accepted an ordinary map lacking an explicit dedicated-world binding");
        send("PqChangeCustomGameSetting",setting.Replace("Test_R01","Missing_R01"));
        Need(Code(send("PqPlayCustomGame",",\"hostMode\":true"))!=0 && File.ReadAllText(control)==saved,"Explicit missing map was silently replaced");
        send("PqChangeCustomGameSetting",setting.Replace("Test_R01","Flag_F01"));
        Need(Code(send("PqPlayCustomGame",",\"hostMode\":true"))!=0 && File.ReadAllText(control)==saved,"Incompatible map/mode accepted");
        send("PqChangeCustomGameSetting",setting.Replace("Test_R01","Roulette_A01"));
        Need(Code(send("PqPlayCustomGame",",\"hostMode\":true"))==0 && File.ReadAllText(control).Contains(" Test_R01 1 7 2"),"Native random choice drew an incompatible map");
        var fields2=File.ReadAllText(control).Trim().Split(' ');File.WriteAllText(control+"."+fields2[1]+".result","done");
        var prestart=send("PqPreStartTrackSel","");
        Need(Code(prestart)==0 && prestart.Notices.Count==0 && File.ReadAllText(control).Contains(" Test_R01 1 7 2"),"Roulette Start-button vote intent did not reach the checked local loading bridge");
        string pendingPrestart=File.ReadAllText(control);
        Need(Code(send("PqPreStartTrackSel",""))!=0 && File.ReadAllText(control)==pendingPrestart,"Repeated roulette Start button overlapped loading");
        fields2=pendingPrestart.Trim().Split(' ');File.WriteAllText(control+"."+fields2[1]+".result","done");
        string custom=setting.Replace("\"raceMode\":1","\"raceMode\":4,\"profile\":\"item_solo_test\"").Replace("\"lap\":999","\"lap\":999,\"customGameOption\":\"?item.UseSkill=false?item.ActiveSkillCoolTime=true\"");
        Need(Code(send("PqChangeCustomGameSetting",custom))==0 && Code(send("PqPlayCustomGame",",\"hostMode\":true"))==0,"Native custom gameplay options were not forwarded");
        string[] customFields=File.ReadAllText(control).Trim().Split(' ');
        Need(customFields.Length==8 && customFields[0]=="TKRACE2" && customFields[6]=="item_solo_test" && customFields[7]=="?item.UseSkill=false?item.ActiveSkillCoolTime=true","Profile/skill settings were dropped at the native boundary");
        File.WriteAllText(control+"."+customFields[1]+".result","done");
        string engineOptions=custom.Replace("?item.UseSkill=false?item.ActiveSkillCoolTime=true","?game.useskill=true?game.skill.activesp=80p");
        Need(Code(send("PqChangeCustomGameSetting",engineOptions))==0 && Code(send("PqPlayCustomGame",",\"hostMode\":true"))==0,"Actual original game settings were rejected");
        var engineFields=File.ReadAllText(control).Trim().Split(' ');
        Need(engineFields[7]=="?game.useskill=true?game.skill.activesp=80p","Original game setting keys were changed in transport");
        File.WriteAllText(control+"."+engineFields[1]+".result","done");
        foreach(string invalid in new[]{"?game=OtherGameMode","?item.UseSkill=true?ITEM.UseSkill=false","?item.UseSkill=x;quit","?game.command=quit","?game.skill.execute=bad"}) {
            string beforeInvalid=File.ReadAllText(control);
            Need(Code(send("PqChangeCustomGameSetting",custom.Replace("?item.UseSkill=false?item.ActiveSkillCoolTime=true",invalid)))==0,"Invalid-option boundary fixture setup failed");
            Need(Code(send("PqPlayCustomGame",",\"hostMode\":true"))!=0 && File.ReadAllText(control)==beforeInvalid,"Unsafe/duplicate option crossed the native boundary");
        }
        send("PqChangeCustomGameSetting",setting.Replace("\"raceMode\":1","\"raceMode\":3"));
        send("PqKickCustomGameMember",",\"slot\":7");saved=File.ReadAllText(control);
        Need(Code(send("PqPlayCustomGame",",\"hostMode\":true"))!=0 && File.ReadAllText(control)==saved,"Incomplete native team accepted");
        var freeDrive=send("PqRegisterMatch",",\"queue\":{\"gameModes\":[{\"raceMode\":1,\"profile\":\"twin_kart_free_drive\"}]}");
        Need(Code(freeDrive)==0 && RoomSnapshot(send("PqJoinCustomGame",",\"sessionId\":\"\"")).AiMemberCount==0,
            "Dedicated Free Drive lobby did not open a single human seat");
        Need(Code(send("PqChangeCustomGameSetting",setting.Replace("Test_R01","Missing_R01")))==0 &&
            Code(send("PqPlayCustomGame",",\"hostMode\":true"))!=0,"Free Drive accepted a missing track");
        string practiceSetting=setting.Replace("\"lap\":999","\"lap\":2,\"customGameOption\":\"?speed.IgnoreFaraway=false?speed.Draft=true?speed.Collision=true\"");
        Need(Code(send("PqChangeCustomGameSetting",practiceSetting))==0 && Code(send("PqPlayCustomGame",",\"hostMode\":true"))==0,
            "Free Drive could not start the original map picked in the room");
        string[] practice=File.ReadAllText(control).Trim().Split(' ');
        Need(practice.Length==8 && practice[0]=="TKRACE2" && practice[2]=="Test_R01" && practice[3]=="1" &&
            practice[4]=="0" && practice[6]==NativeRaceOptions.FreeDriveProfile && practice[7]=="-",
            "Selected map, dedicated profile or zero-AI room state lost between lobby and native bridge");
        Console.WriteLine("PASS native custom-room transport: 21 mode IDs, exact maps/default laps/AI, readiness, double-click guard and missing-resource rejection. Native event rules require separate game acceptance. No game started.");
    }
    static void NativeRandomPickerChecks(string folder)
    {
        string control=Path.Combine(folder,"picker.control"),report=Path.Combine(folder,"picker-tracks.json"),sha=new string('c',64);
        Func<string,int,bool,string> row=(id,mask,physical)=>"{\"TrackId\":\""+id+"\",\"NativeDefinitionStatus\":\"native-definition-present\",\"WorldMatch\":\""+(physical?"exact-base-world-record":"not-audited")+"\",\"AiPathMatch\":\""+(physical?"exact-base-ai-record":"not-audited")+"\",\"NativeModeMask\":"+mask+",\"NativeLaps\":2}";
        File.WriteAllText(report,"{\"ClientSha256\":\""+sha+"\",\"NativeDefinitionsComplete\":true,\"Tracks\":["+
            row("Map_A01",14,true)+","+row("Map_A02",14,true)+","+row("Flag_F01",16,true)+","+
            row("Roulette_A01",14,false)+","+row("Random_R01",6,false)+","+row("Random_R02",4,false)+"]}");
        File.WriteAllText(control+".ready",sha);
        var launch=new NativeRoomLaunch(control,report,sha);var room=new RoomState();
        foreach(string selector in new[]{"Roulette_A01","Random_R01","Random_R02"}) {
            int mode=selector=="Random_R02"?1:4;room.PrepareQuickRoom(mode,selector);
            Need(room.Apply("{\"_type\":\"PqJoinCustomGame\",\"reqId\":1,\"sessionId\":\"\"}").Accepted,"Picker fixture did not join");
            string previous=null;
            for(int index=0;index<24;++index) {
                Need(launch.Start(room)==null,"Original random/roulette picker failed");
                string[] fields=File.ReadAllText(control).Trim().Split(' ');
                Need((fields[2]=="Map_A01" || fields[2]=="Map_A02") && fields[2]!=previous,"Random selection repeated or sent a virtual picker to native travel");
                previous=fields[2];File.WriteAllText(control+"."+fields[1]+".result","done");
            }
        }
        foreach(var choice in new[]{new[]{"Random_R02","4"},new[]{"Roulette_A01","104"},new[]{"Random_R99","1"}}) {
            room.Apply("{\"_type\":\"PqChangeCustomGameSetting\",\"reqId\":1,\"setting\":{\"_type\":\"CsRaceGame\",\"track\":\""+choice[0]+"\",\"gameMode\":{\"raceMode\":"+choice[1]+"}}}");
            string previous=File.ReadAllText(control);
            Need(launch.Start(room)!=null && File.ReadAllText(control)==previous,"Random picker ignored original mode mask or accepted an invented selector");
        }
        // A Cube selector may draw only explicit dedicated-world bindings,
        // even when ordinary maps advertise the same item mode mask.
        File.WriteAllText(report,"{\"ClientSha256\":\""+sha+"\",\"NativeDefinitionsComplete\":true,\"Tracks\":["+
            row("Map_A01",2,true)+","+row("Desert_I06",2,true)+","+row("Village_I05",1,true)+","+row("World_I11",1,true)+","+row("Roulette_A01",14,false)+"]}");
        launch=new NativeRoomLaunch(control,report,sha);room=new RoomState();room.PrepareQuickRoom(116,"Roulette_A01");
        Need(room.Apply("{\"_type\":\"PqJoinCustomGame\",\"reqId\":1,\"sessionId\":\"\"}").Accepted,"Cube picker did not join");
        string previousCube=null;
        for(int index=0;index<12;++index) {
            Need(launch.Start(room)==null,"Cube roulette rejected the physical map pool");
            string[] fields=File.ReadAllText(control).Trim().Split(' ');
            Need((fields[2]=="Desert_I06" || fields[2]=="Village_I05" || fields[2]=="World_I11") && fields[2]!=previousCube,"Cube roulette drew an ordinary map or immediately repeated");
            previousCube=fields[2];File.WriteAllText(control+"."+fields[1]+".result","done");
        }
        Console.WriteLine("PASS original random selectors: Roulette_A01/Random_R01/Random_R02, native mode masks, physical-map pool, no immediate repeats and unknown-selector rejection. No game started.");
    }
    static int Main(string[] args)
    {
        string folder = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "test-scratch", "TwinKartBackendTest-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(folder);
        try
        {
            FactoryRoomChecks(folder);
            ChaserRoomChecks(folder);
            HideRoomChecks(folder);
            PrototypeScheduleChecks();
            NativeRoomChecks(folder);
            NativeRandomPickerChecks(folder);
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
            string practiceControl=Path.Combine(folder,"practice.control");
            var practiceStore=new OfflineProfileStore(Path.Combine(folder,"practice-save.json"),initial,defaults,unlock,rules,
                "{\"tracks\":[\"Test_R01\"]}",physics,"{\"vehicles\":{}}","1",54321,null,null,null,practiceControl);
            string practiceId=(string)Call(practiceStore,"PqTimeAttackStart",",\"trackId\":\"Test_R01\"").Response["gameId"];
            Need(File.ReadAllText(practiceControl+".practice-ticket")==practiceId+":Test_R01","Native practice ticket not tied to the actual race and map");
            File.WriteAllText(practiceControl+".practice-active",practiceId,Encoding.ASCII);
            var practiceRestart=Call(practiceStore,"PqTimeAttackUpdate",",\"restart\":true,\"status\":1,\"gameId\":\""+practiceId+"\",\"timeAttack\":{\"trackId\":\"Test_R01\",\"recordTime\":0}");
            var restartedPracticeId=(string)practiceRestart.Response["gameId"];
            Need(Code(practiceRestart)==0 && restartedPracticeId!=practiceId &&
                File.ReadAllText(practiceControl+".practice-ticket")==restartedPracticeId+":Test_R01" &&
                File.ReadAllText(practiceControl+".practice-active")==practiceId,
                "Native restart did not receive a fresh scoped ticket or overwrote the old marker");
            var practiceFinish=Call(practiceStore,"PqTimeAttackUpdate",",\"status\":0,\"gameId\":\""+restartedPracticeId+"\",\"timeAttack\":{\"trackId\":\"Test_R01\",\"recordTime\":456}");
            var practiceSave=ProfileJson.Parse(practiceStore.Snapshot());
            Need(Code(practiceFinish)==0 && !(bool)practiceFinish.Response["isUpdated"] &&
                ProfileJson.Object(practiceSave,"records").Count==0 && !practiceSave.ContainsKey("raceResultHistory") &&
                !practiceSave.ContainsKey("activeRace"),"Free drive wrote a result, ranking record or retained a finished race");
            var reused = Call(practiceStore,"PqTimeAttackUpdate",",\"status\":1,\"gameId\":\""+restartedPracticeId+"\",\"timeAttack\":{\"trackId\":\"Test_R01\",\"recordTime\":0}");
            Need(Code(reused)==0 && (string)reused.Response["gameId"]==restartedPracticeId && !(bool)reused.Response["isUpdated"] &&
                practiceStore.Snapshot()==ProfileJson.Write(practiceSave),
                "A native same-ID practice restart touched the saved profile or lost its unranked identity");
            string rankedAfterPractice=(string)Call(practiceStore,"PqTimeAttackStart",",\"trackId\":\"Test_R01\"").Response["gameId"];
            Need(rankedAfterPractice!=practiceId && File.ReadAllText(practiceControl+".practice-ticket")==rankedAfterPractice+":Test_R01",
                "New ranked attempt reused the practice identity");
            Need(Code(Call(practiceStore,"PqTimeAttackUpdate",",\"status\":1,\"gameId\":\""+restartedPracticeId+"\",\"timeAttack\":{\"trackId\":\"Test_R01\",\"recordTime\":0}"))!=0,
                "Stale unranked identity could update after a new ordinary Start");
            var rankedFinish=Call(practiceStore,"PqTimeAttackUpdate",",\"status\":0,\"gameId\":\""+rankedAfterPractice+"\",\"timeAttack\":{\"trackId\":\"Test_R01\",\"recordTime\":789}");
            Need(Code(rankedFinish)==0 && (bool)rankedFinish.Response["isUpdated"] &&
                practiceStore.Snapshot().Contains("\"lapTime\":789"),"Old practice marker affected the next ranked result");
            string protectedId=(string)Call(practiceStore,"PqTimeAttackStart",",\"trackId\":\"Test_R01\"").Response["gameId"];
            File.WriteAllText(practiceControl+".practice-active",protectedId,Encoding.ASCII);
            using (var unavailable=new FileStream(practiceControl+".practice-active",FileMode.Open,FileAccess.ReadWrite,FileShare.None))
            {
                var unavailableResult=Call(practiceStore,"PqTimeAttackUpdate",",\"status\":0,\"gameId\":\""+protectedId+"\",\"timeAttack\":{\"trackId\":\"Test_R01\",\"recordTime\":321}");
                Need(Code(unavailableResult)!=0 && practiceStore.Snapshot().Contains("\"lapTime\":789"),
                    "An unreadable practice marker let a result overwrite a normal time");
            }
            Console.WriteLine("PASS free drive: exact challenge ticket, unranked result discarded, next ordinary result preserved, unreadable marker fails closed.");
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
