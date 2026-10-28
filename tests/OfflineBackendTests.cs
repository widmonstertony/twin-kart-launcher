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
            Need(Code(Call(store,"PqTimeAttackUpdate",",\"gameId\":\"wrong\",\"timeAttack\":{\"trackId\":\"Test_R01\",\"recordTime\":1000}"))!=0,"Foreign race result accepted");
            Need(Code(Call(store,"PqTimeAttackUpdate",",\"gameId\":\""+raceId+"\",\"timeAttack\":{\"trackId\":\"Test_R01\",\"recordTime\":1000}"))==0,"Valid race result rejected");
            Need(store.Snapshot().Contains("\"lapTime\":1000"),"Local personal best not persisted");
            Need(Code(Call(store,"PqKartUpgrade",",\"kartItemId\":\"Kart:1\""))==0,"Offline upgrade retains a progression/currency gate");
            Need(Code(Call(store,"PqSetQuickMatchData",",\"data\":{\"mode\":\"synthetic\"}"))==0 && store.Snapshot().Contains("synthetic"),"Offline mode choice not persisted");
            Need(Code(Call(store,"PqSetQuickMatchData",",\"data\":17"))!=0,"Invalid mode-choice object accepted");
            var reload=new OfflineProfileStore(path,initial,defaults,unlock,rules,"{\"tracks\":[\"Test_R01\"]}",physics,"{\"vehicles\":{}}","1",54321);
            Need(reload.Snapshot()==store.Snapshot(),"Atomic persistence or restart lost state");
            Need(Code(Call(store,"PqUnsupported"))!=0,"Unknown operation falsely succeeded");
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
            return 0;
        }
        catch(Exception error) { Console.Error.WriteLine(error); return 1; }
        finally { Directory.Delete(folder,true); }
    }
}
