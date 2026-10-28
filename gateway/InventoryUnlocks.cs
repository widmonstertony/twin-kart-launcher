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

        public static UnlockResult Apply(string profileJson, string catalogJson)
        {
            var json = Serializer();
            // Parsing creates private trees. Failure never partially edits a caller's save.
            var state = Map(json.DeserializeObject(profileJson), "profile");
            var catalog = Map(json.DeserializeObject(catalogJson), "catalog");
            var entries = List(Required(catalog, "items"), "catalog.items");
            if (entries.Count > 100000) throw new FormatException("Resource catalog exceeds limit.");
            var grants = new SortedDictionary<string, int>(StringComparer.Ordinal);
            foreach (object raw in entries)
            {
                var entry = Map(raw, "catalog item");
                string id = Identifier(Required(entry, "id"));
                object quantity = Required(entry, "quantity");
                if (!(quantity is int) || (int)quantity < 1) throw new FormatException("Quantity must be a positive int32.");
                int previous;
                if (grants.TryGetValue(id, out previous) && previous != (int)quantity)
                    throw new FormatException("Conflicting quantities for a resource.");
                grants[id] = (int)quantity;
            }
            var suppliedUpgrades = Map(Required(catalog, "kartUpgrades"), "catalog.kartUpgrades");
            foreach (var pair in suppliedUpgrades)
            {
                Identifier(pair.Key);
                if (!pair.Key.StartsWith("Kart:", StringComparison.Ordinal) || !grants.ContainsKey(pair.Key))
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
                if (!upgrades.ContainsKey(pair.Key)) { upgrades.Add(pair.Key, pair.Value); ++upgradeCount; }
            var missing = new List<string>();
            // Body IDs are explicit in the external catalog: Kart also includes parts.
            object bodies;
            if (catalog.TryGetValue("kartBodies", out bodies))
                foreach (object raw in List(bodies, "kartBodies"))
                {
                    string id = Identifier(raw);
                    if (!grants.ContainsKey(id) || !id.StartsWith("Kart:", StringComparison.Ordinal))
                        throw new FormatException("Body is absent from the grant catalog.");
                    if (!upgrades.ContainsKey(id)) missing.Add(id);
                }
            missing.Sort(StringComparer.Ordinal);
            return new UnlockResult { ProfileJson = json.Serialize(state), AddedItems = added,
                AddedKartUpgrades = upgradeCount, CatalogItems = grants.Count,
                KartsWithoutUpgradeRecord = missing.ToArray() };
        }
    }
}
