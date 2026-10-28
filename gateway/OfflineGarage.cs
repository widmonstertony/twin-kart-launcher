using System;
using System.Collections.Generic;
using System.Web.Script.Serialization;

namespace TwinKartLauncher.Gateway
{
    public sealed class GarageResult
    {
        public string ProfileJson { get; internal set; }
        public int UpdatedKarts { get; internal set; }
        public int FieldsPerKart { get; internal set; }
    }

    // An alternate offline editing path when the original garage UI still
    // enforces its own point/class/fixed-field gates. This edits a JSON snapshot,
    // never the game's data tables or a live database. Save persistence is owned
    // by the caller. The external schema must match the actual client version.
    public static class OfflineGarage
    {
        static Dictionary<string, object> Map(object value, string label)
        {
            var result = value as Dictionary<string, object>;
            if (result == null) throw new FormatException(label + " must be an object.");
            return result;
        }
        static object Get(Dictionary<string, object> value, string key)
        {
            object result;
            if (!value.TryGetValue(key, out result)) throw new FormatException("Missing " + key);
            return result;
        }
        static int Number(object value, string label)
        {
            if (!(value is int) || (int)value < 0) throw new FormatException(label + " must be a nonnegative int32.");
            return (int)value;
        }

        // plan: {"karts":["Kart:..."],"maximum":true} or
        // {"karts":["Kart:..."],"settings":{"field":level}}.
        // "*" explicitly selects every existing upgrade record. It cannot
        // manufacture missing vehicle physics or enable absent engine features.
        public static GarageResult Apply(string profileJson, string rulesJson, string planJson)
        {
            var json = new JavaScriptSerializer { MaxJsonLength = 32 * 1024 * 1024, RecursionLimit = 96 };
            var state = Map(json.DeserializeObject(profileJson), "profile");
            var rules = Map(Get(Map(json.DeserializeObject(rulesJson), "schema"), "rules"), "rules");
            var plan = Map(json.DeserializeObject(planJson), "plan");
            var schema = new Dictionary<string, int[]>(StringComparer.Ordinal);
            foreach (var entry in rules)
            {
                var rule = Map(entry.Value, "rule"); object compatible;
                if (rule.TryGetValue("clientCompatible", out compatible))
                {
                    if (!(compatible is bool)) throw new FormatException("clientCompatible must be boolean.");
                    if (!(bool)compatible) continue;
                }
                var levels = Get(rule, "values") as object[];
                if (levels == null || levels.Length == 0) throw new FormatException("Empty tuning rule.");
                var values = new int[levels.Length];
                for (int i = 0; i < values.Length; ++i) values[i] = Number(levels[i], entry.Key);
                schema.Add(entry.Key, values);
            }
            if (schema.Count == 0) throw new FormatException("No verified compatible tuning fields.");
            var selected = Get(plan, "karts") as object[];
            if (selected == null || selected.Length == 0) throw new FormatException("Select at least one kart.");
            var upgrades = Map(Get(state, "ownKartsUpgradeInfo"), "upgrades");
            var targets = new HashSet<string>(StringComparer.Ordinal);
            foreach (object raw in selected)
            {
                string id = raw as string;
                if (id == "*") { foreach (string key in upgrades.Keys) targets.Add(key); }
                else if (id == null || !id.StartsWith("Kart:", StringComparison.Ordinal) || !upgrades.ContainsKey(id))
                    throw new ArgumentException("Selected kart has no upgrade record.");
                else targets.Add(id);
            }
            object maximum;
            bool useMaximum = plan.TryGetValue("maximum", out maximum) && maximum is bool && (bool)maximum;
            if (plan.ContainsKey("maximum") && !(maximum is bool)) throw new FormatException("maximum must be boolean.");
            if (useMaximum && plan.ContainsKey("settings")) throw new FormatException("Choose maximum or settings.");
            var requested = useMaximum ? OfflineTuning.Maximum(schema) : new Dictionary<string, int>(StringComparer.Ordinal);
            if (!useMaximum)
                foreach (var entry in Map(Get(plan, "settings"), "settings")) requested.Add(entry.Key, Number(entry.Value, entry.Key));
            foreach (string id in targets)
            {
                var gear = Map(Get(Map(upgrades[id], "upgrade"), "gearInfo"), "gearInfo");
                var previous = new Dictionary<string, int>(StringComparer.Ordinal);
                foreach (var entry in Map(Get(gear, "gearSettings"), "gearSettings"))
                {
                    // -1 is the observed client lock sentinel. The offline
                    // preset removes that lock by selecting supported level 0.
                    if (entry.Value is int && (int)entry.Value == -1) previous.Add(entry.Key, 0);
                    else previous.Add(entry.Key, Number(entry.Value, entry.Key));
                }
                var allocated = OfflineTuning.Allocate(schema, previous, requested, false);
                gear["gearSettings"] = allocated;
                gear["totalGearCount"] = OfflineTuning.DisplayGearBalance;
                gear["remainGearCount"] = OfflineTuning.DisplayGearBalance;
            }
            return new GarageResult { ProfileJson = json.Serialize(state), UpdatedKarts = targets.Count, FieldsPerKart = schema.Count };
        }
    }
}
