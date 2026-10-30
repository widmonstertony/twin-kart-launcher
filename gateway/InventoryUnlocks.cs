using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;

namespace TwinKartLauncher.Gateway
{
    public sealed class UnlockResult
    {
        public string ProfileJson { get; internal set; }
        public int AddedItems { get; internal set; }
        public int AddedKartUpgrades { get; internal set; }
        public int CatalogItems { get; internal set; }
        public int RemovedUnavailableKartBodies { get; internal set; }
        public string[] KartsWithoutUpgradeRecord { get; internal set; }
    }

    // External catalogs supply identifiers and optional client-specific upgrade
    // records. No vendor item data or response template is compiled into this file.
    public static class InventoryUnlocks
    {
        static readonly Regex ItemId = new Regex(@"^[A-Za-z][A-Za-z0-9_]*:[0-9]{1,18}$", RegexOptions.CultureInvariant);
        static JavaScriptSerializer Serializer()
        { return new JavaScriptSerializer { MaxJsonLength = 32 * 1024 * 1024, RecursionLimit = 96 }; }

        static Dictionary<string, object> Map(object value, string label)
        {
            var map = value as Dictionary<string, object>;
            if (map == null) throw new FormatException(label + " must be an object.");
            return map;
        }
        static object Required(Dictionary<string, object> map, string key)
        {
            object value;
            if (!map.TryGetValue(key, out value)) throw new FormatException("Missing " + key);
            return value;
        }
        static string Identifier(object value)
        {
            string id = value as string;
            if (id == null || !ItemId.IsMatch(id)) throw new FormatException("Invalid resource identifier.");
            return id;
        }
        static List<object> List(object value, string label)
        {
            var array = value as object[];
            if (array != null) return new List<object>(array);
            var list = value as List<object>;
            if (list == null) throw new FormatException(label + " must be an array.");
            return list;
        }

        static HashSet<string> AvailableBodies(string availabilityJson, HashSet<string> catalogBodies)
        {
            if (availabilityJson == null) return null;
            var root = Map(Serializer().DeserializeObject(availabilityJson), "vehicle availability");
            var rows = List(Required(root, "bodies"), "vehicle availability bodies");
            if (rows.Count > 100000) throw new FormatException("Vehicle availability exceeds limit.");
            var seen = new HashSet<string>(StringComparer.Ordinal);
            var available = new HashSet<string>(StringComparer.Ordinal);
            foreach (object raw in rows)
            {
                var row = Map(raw, "vehicle availability row");
                string id = Identifier(Required(row, "id"));
                object status = Required(row, "registered");
                if (!id.StartsWith("Kart:", StringComparison.Ordinal) || !catalogBodies.Contains(id) ||
                    !(status is int) || ((int)status != 0 && (int)status != 1) || !seen.Add(id))
                    throw new FormatException("Invalid or duplicate vehicle availability row.");
                if ((int)status == 1) available.Add(id);
            }
            if (seen.Count != catalogBodies.Count) throw new FormatException("Vehicle availability is incomplete for this unlock catalog.");
            return available;
        }

        public static UnlockResult Apply(string profileJson, string catalogJson, string availabilityJson = null)
        {
            var json = Serializer();
            // Parsing creates private trees. Failure never partially edits a caller's save.
            var state = Map(json.DeserializeObject(profileJson), "profile");
            var catalog = Map(json.DeserializeObject(catalogJson), "catalog");
            var entries = List(Required(catalog, "items"), "catalog.items");
            if (entries.Count > 100000) throw new FormatException("Resource catalog exceeds limit.");
            var allGrants = new SortedDictionary<string, int>(StringComparer.Ordinal);
            foreach (object raw in entries)
            {
                var entry = Map(raw, "catalog item");
                string id = Identifier(Required(entry, "id"));
                object quantity = Required(entry, "quantity");
                if (!(quantity is int) || (int)quantity < 1) throw new FormatException("Quantity must be a positive int32.");
                int previous;
                if (allGrants.TryGetValue(id, out previous) && previous != (int)quantity)
                    throw new FormatException("Conflicting quantities for a resource.");
                allGrants[id] = (int)quantity;
            }
            var catalogBodies = new HashSet<string>(StringComparer.Ordinal);
            object rawBodies;
            if (catalog.TryGetValue("kartBodies", out rawBodies))
                foreach (object raw in List(rawBodies, "kartBodies"))
                {
                    string id = Identifier(raw);
                    if (!allGrants.ContainsKey(id) || !id.StartsWith("Kart:", StringComparison.Ordinal) || !catalogBodies.Add(id))
                        throw new FormatException("Body is absent from the grant catalog or duplicated.");
                }
            var availableBodies = AvailableBodies(availabilityJson, catalogBodies);
            var grants = new SortedDictionary<string, int>(StringComparer.Ordinal);
            foreach (var pair in allGrants)
                if (!catalogBodies.Contains(pair.Key) || availableBodies == null || availableBodies.Contains(pair.Key)) grants.Add(pair.Key, pair.Value);
            var suppliedUpgrades = Map(Required(catalog, "kartUpgrades"), "catalog.kartUpgrades");
            foreach (var pair in suppliedUpgrades)
            {
                Identifier(pair.Key);
                if (!pair.Key.StartsWith("Kart:", StringComparison.Ordinal) || !allGrants.ContainsKey(pair.Key))
                    throw new FormatException("Upgrade must belong to a catalog kart.");
                var upgrade = Map(pair.Value, "kart upgrade");
                Map(Required(upgrade, "gearInfo"), "gearInfo");
                Required(upgrade, "engineType"); Required(upgrade, "grade");
            }
            var inventory = Map(Required(state, "inventory"), "inventory");
            var owned = new HashSet<string>(StringComparer.Ordinal);
            foreach (var bucket in inventory)
            {
                var contents = Map(bucket.Value, "inventory bucket");
                object rawList;
                if (!contents.TryGetValue("itemList", out rawList)) continue;
                foreach (object raw in List(rawList, "itemList"))
                {
                    string text = raw as string;
                    int comma = text == null ? -1 : text.IndexOf(',');
                    if (comma < 1) throw new FormatException("Invalid saved inventory row.");
                    owned.Add(Identifier(text.Substring(0, comma)));
                }
            }
            object destination;
            if (!inventory.TryGetValue("inventoryData", out destination))
            {
                destination = new Dictionary<string, object> { { "itemList", new object[0] } };
                inventory["inventoryData"] = destination;
            }
            var target = Map(destination, "inventoryData");
            var targetList = List(Required(target, "itemList"), "inventoryData.itemList");
            int removed = 0;
            if (availableBodies != null)
            {
                for (int index = targetList.Count - 1; index >= 0; --index)
                {
                    string text = targetList[index] as string; int comma = text == null ? -1 : text.IndexOf(',');
                    if (comma < 1) throw new FormatException("Invalid saved inventory row.");
                    string id = Identifier(text.Substring(0, comma));
                    if (catalogBodies.Contains(id) && !availableBodies.Contains(id)) { targetList.RemoveAt(index); owned.Remove(id); ++removed; }
                }
                object presetObject;
                if (state.TryGetValue("itemPresetOn", out presetObject))
                {
                    var preset = Map(presetObject, "itemPresetOn"); object itemMapObject;
                    if (preset.TryGetValue("itemMap", out itemMapObject))
                    {
                        var itemMap = Map(itemMapObject, "itemPresetOn.itemMap"); object selected;
                        if (itemMap.TryGetValue("Kart", out selected) && selected is string && catalogBodies.Contains((string)selected) && !availableBodies.Contains((string)selected))
                            throw new FormatException("Selected kart body is unavailable in this client; profile was not changed.");
                    }
                    object selectedDirect;
                    if (preset.TryGetValue("selected", out selectedDirect) && selectedDirect is string && catalogBodies.Contains((string)selectedDirect) && !availableBodies.Contains((string)selectedDirect))
                        throw new FormatException("Selected kart body is unavailable in this client; profile was not changed.");
                }
            }
            int added = 0;
            foreach (var grant in grants)
                if (owned.Add(grant.Key))
                {
                    targetList.Add(grant.Key + "," + grant.Value.ToString(CultureInfo.InvariantCulture));
                    ++added;
                }
            target["itemList"] = targetList;
            object savedUpgrades;
            if (!state.TryGetValue("ownKartsUpgradeInfo", out savedUpgrades))
            { savedUpgrades = new Dictionary<string, object>(); state["ownKartsUpgradeInfo"] = savedUpgrades; }
            var upgrades = Map(savedUpgrades, "ownKartsUpgradeInfo");
            int upgradeCount = 0;
            foreach (var pair in suppliedUpgrades)
                if ((availableBodies == null || !catalogBodies.Contains(pair.Key) || availableBodies.Contains(pair.Key)) && !upgrades.ContainsKey(pair.Key)) { upgrades.Add(pair.Key, pair.Value); ++upgradeCount; }
            var missing = new List<string>();
            // Body IDs are explicit in the external catalog: Kart also includes parts.
            if (rawBodies != null)
                foreach (object raw in List(rawBodies, "kartBodies"))
                {
                    string id = Identifier(raw);
                    if (availableBodies != null && !availableBodies.Contains(id)) continue;
                    if (!upgrades.ContainsKey(id)) missing.Add(id);
                }
            missing.Sort(StringComparer.Ordinal);
            return new UnlockResult { ProfileJson = json.Serialize(state), AddedItems = added,
                AddedKartUpgrades = upgradeCount, CatalogItems = grants.Count, RemovedUnavailableKartBodies = removed,
                KartsWithoutUpgradeRecord = missing.ToArray() };
        }
    }
}
