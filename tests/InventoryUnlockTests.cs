using System;
using System.Web.Script.Serialization;
using TwinKartLauncher.Gateway;

class InventoryUnlockTests
{
    static void Need(bool ok, string reason) { if (!ok) throw new Exception(reason); }
    static int Main()
    {
        string save = "{\"inventory\":{\"inventoryData\":{\"itemList\":[\"Character:1,1\"]},\"inventoryDataInitial\":{\"itemList\":[\"Kart:1,1\"]},\"inventoryDataCash\":{\"itemList\":[\"Cash:1,999\"]}},\"ownKartsUpgradeInfo\":{\"Kart:1\":{\"grade\":5,\"custom\":true}},\"records\":{\"best\":123},\"itemPresetOn\":{\"selected\":\"Kart:1\"}}";
        string catalog = "{\"items\":[{\"id\":\"Kart:1\",\"quantity\":1},{\"id\":\"Kart:2\",\"quantity\":1},{\"id\":\"Character:2\",\"quantity\":1}],\"kartBodies\":[\"Kart:1\",\"Kart:2\"],\"kartUpgrades\":{\"Kart:1\":{\"gearInfo\":{},\"engineType\":0,\"grade\":0}}}";
        var result = InventoryUnlocks.Apply(save, catalog);
        Need(result.AddedItems == 2 && result.AddedKartUpgrades == 0, "Grant count or saved upgrade preservation failed");
        Need(result.KartsWithoutUpgradeRecord.Length == 1 && result.KartsWithoutUpgradeRecord[0] == "Kart:2", "Missing physics prerequisite was concealed");
        Need(result.ProfileJson.Contains("999") && result.ProfileJson.Contains("\"custom\":true") && result.ProfileJson.Contains("\"best\":123"), "Player state changed");
        var again = InventoryUnlocks.Apply(result.ProfileJson, catalog);
        Need(again.AddedItems == 0 && again.ProfileJson == result.ProfileJson, "Not idempotent or duplicates added across buckets");
        bool rejected = false;
        try { InventoryUnlocks.Apply(save, catalog.Replace("\"quantity\":1", "\"quantity\":1.5")); } catch (FormatException) { rejected = true; }
        Need(rejected, "Fractional inventory quantity accepted");
        rejected = false;
        try { InventoryUnlocks.Apply(save, catalog.Replace("Character:2", "../private")); } catch (FormatException) { rejected = true; }
        Need(rejected, "Invalid resource accepted");
        var schema = new System.Collections.Generic.Dictionary<string, int[]>();
        var request = new System.Collections.Generic.Dictionary<string, int>();
        for (int i = 0; i < 6; ++i) { schema.Add("field" + i, new[] { 0, 10 }); request.Add("field" + i, 10); }
        var allocation = OfflineTuning.Allocate(schema, new System.Collections.Generic.Dictionary<string, int>(), request, false);
        Need(allocation.Count == 6 && allocation["field5"] == 10, "Allocation above original shared budget rejected");
        rejected = false;
        request["field0"] = 100;
        try { OfflineTuning.Allocate(schema, allocation, request, false); } catch (ArgumentOutOfRangeException) { rejected = true; }
        Need(rejected && allocation["field0"] == 10, "Unsupported numeric level accepted or previous preset mutated");
        string garageSave = "{\"ownKartsUpgradeInfo\":{\"Kart:1\":{\"grade\":1,\"gearInfo\":{\"gearSettings\":{\"fixed\":0,\"drift\":0}}},\"Kart:2\":{\"grade\":2,\"gearInfo\":{\"gearSettings\":{\"fixed\":0,\"drift\":4}}}},\"records\":{\"best\":123}}";
        string rules = "{\"rules\":{\"fixed\":{\"tunable\":false,\"values\":[0,8,3]},\"drift\":{\"values\":[0,4,9]}}}";
        var garage = OfflineGarage.Apply(garageSave, rules, "{\"karts\":[\"*\"],\"maximum\":true}");
        Need(garage.UpdatedKarts == 2 && garage.FieldsPerKart == 2 && garage.ProfileJson.Contains("\"fixed\":8") && garage.ProfileJson.Contains("\"drift\":9"), "Maximum plan failed for unsorted levels or fixed field");
        Need(garage.ProfileJson.Contains("\"grade\":1") && garage.ProfileJson.Contains("\"best\":123"), "Garage changed grade or records");
        var single = OfflineGarage.Apply(garageSave, rules, "{\"karts\":[\"Kart:1\"],\"settings\":{\"fixed\":3}}");
        var parsed = new JavaScriptSerializer().DeserializeObject(single.ProfileJson) as System.Collections.Generic.Dictionary<string, object>;
        var karts = (System.Collections.Generic.Dictionary<string, object>)parsed["ownKartsUpgradeInfo"];
        var untouched = (System.Collections.Generic.Dictionary<string, object>)((System.Collections.Generic.Dictionary<string, object>)karts["Kart:2"])["gearInfo"];
        Need((int)((System.Collections.Generic.Dictionary<string, object>)untouched["gearSettings"])["drift"] == 4, "Single-kart plan changed another kart");
        rejected = false;
        try { OfflineGarage.Apply(garageSave, rules, "{\"karts\":[\"Kart:1\",\"Kart:999\"],\"maximum\":true}"); } catch (ArgumentException) { rejected = true; }
        Need(rejected && garageSave.Contains("\"fixed\":0"), "Missing upgrade fabricated or original JSON changed");
        var unavailable = rules.Replace("\"tunable\":false", "\"tunable\":false,\"clientCompatible\":false");
        var compatibleOnly = OfflineGarage.Apply(garageSave, unavailable, "{\"karts\":[\"Kart:1\"],\"maximum\":true}");
        Need(compatibleOnly.FieldsPerKart == 1 && !compatibleOnly.ProfileJson.Contains("\"fixed\":8"), "Unmapped client feature presented as a supported maximum");
        var tracks = new TrackCatalog("{\"tracks\":[\"Test_R01\",\"Test_I02\",\"Test_R01\",\"Test_R01_Reverse\"]}");
        var names = tracks.All; names[0] = "changed";
        Need(tracks.All.Length == 3 && tracks.Contains("Test_R01") && !tracks.Contains("changed"), "Track deduplication or defensive copy failed");
        string rows = tracks.TimeAttackRows(new System.Collections.Generic.HashSet<string>(new[] { "Test_I02" }));
        Need(rows.Contains("\"trackId\":\"Test_R01\",\"count\":0") && rows.Contains("\"trackId\":\"Test_I02\",\"count\":1") && rows.Contains("Test_R01_Reverse"), "Tracks gated on records or variants lost");
        rejected = false;
        try { new TrackCatalog("{\"tracks\":[\"../Test\"]}"); } catch (FormatException) { rejected = true; }
        Need(rejected, "Invalid track path accepted");
        Console.WriteLine("Inventory migration: global ownership, idempotence, preserved currency/progress/upgrades and missing prerequisite checks passed.");
        Console.WriteLine("Offline garage: all-kart maximum, custom preset, fixed fields, isolation and missing physics checks passed. Track selection has no record/progression gate.");
        return 0;
    }
}
