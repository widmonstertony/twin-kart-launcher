using System;
using System.Collections.Generic;

namespace TwinKartLauncher.Gateway
{
    // Offline sandbox policy. Level values come from an external client table;
    // there are no currency, progression, kart-class or shared point-budget gates.
    public static class OfflineTuning
    {
        public const int DisplayGearBalance = 1000000;
        public static Dictionary<string, int> Maximum(IDictionary<string, int[]> supportedValues)
        {
            if (supportedValues == null) throw new ArgumentNullException("supportedValues");
            var requested = new Dictionary<string, int>(StringComparer.Ordinal);
            foreach (var field in supportedValues)
            {
                if (field.Value == null || field.Value.Length == 0) throw new ArgumentException("No numeric levels for " + field.Key);
                int highest = field.Value[0];
                foreach (int value in field.Value) highest = Math.Max(highest, value);
                requested.Add(field.Key, highest);
            }
            return Allocate(supportedValues, new Dictionary<string, int>(), requested, false);
        }
        public static Dictionary<string, int> Allocate(
            IDictionary<string, int[]> supportedValues,
            IDictionary<string, int> previous,
            IDictionary<string, int> requested, bool reset)
        {
            if (supportedValues == null || previous == null || requested == null)
                throw new ArgumentNullException("Tuning inputs");
            foreach (var entry in requested)
                if (!supportedValues.ContainsKey(entry.Key)) throw new ArgumentException("Unknown tuning field: " + entry.Key);
            var output = new Dictionary<string, int>(StringComparer.Ordinal);
            foreach (var field in supportedValues)
            {
                if (field.Value == null || field.Value.Length == 0) throw new ArgumentException("No numeric levels for " + field.Key);
                int value;
                if (reset) value = 0;
                else if (!requested.TryGetValue(field.Key, out value) && !previous.TryGetValue(field.Key, out value)) value = 0;
                // Only numeric levels encoded by this client are valid. Vehicle-
                // specific caps and original fixed/tunable flags are not applied.
                if (Array.IndexOf(field.Value, value) < 0) throw new ArgumentOutOfRangeException(field.Key, "No client data for requested level.");
                output.Add(field.Key, value);
            }
            return output;
        }
    }
}
