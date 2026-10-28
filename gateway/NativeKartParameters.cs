using System;
using System.Collections.Generic;
using System.Security.Cryptography;
using System.Text;

namespace TwinKartLauncher.Gateway
{
    // Explicit externally measured packed fields, captures and effect tables.
    // No hard-coded vendor performance values or proprietary assets.
    public sealed class NativeKartParameters
    {
        readonly Dictionary<string, object> data;
        public NativeKartParameters(string catalogJson) { data = ProfileJson.Parse(catalogJson); }
        public Dictionary<string, object> Payload(Dictionary<string, object> state, string account, string shared, string kart = null)
        {
            if (kart == null) kart = ProfileJson.Text(ProfileJson.Object(ProfileJson.Object(state, "itemPresetOn"), "itemMap"), "Kart");
            var captures = ProfileJson.Object(data, "vehicles");
            object entry;
            if (!captures.TryGetValue(kart, out entry)) throw new ArgumentException("Vehicle has no external physics capture.");
            var capture = ProfileJson.Object(entry);
            var upgrade = ProfileJson.Object(ProfileJson.Object(state, "ownKartsUpgradeInfo"), kart);
            if (Convert.ToInt32(upgrade["engineType"]) != Convert.ToInt32(capture["engineType"]))
                throw new ArgumentException("Engine representation differs from external capture.");
            byte[] body = Convert.FromBase64String(ProfileJson.Text(capture, "packedBase"));
            var fields = ProfileJson.Object(data, "fields");
            var before = Effect(ProfileJson.Object(capture, "settings"));
            var after = Effect(ProfileJson.Object(ProfileJson.Object(upgrade, "gearInfo"), "gearSettings"));
            var changed = new HashSet<string>(before.Keys, StringComparer.Ordinal);
            changed.UnionWith(after.Keys);
            foreach (string name in changed)
            {
                var field = ProfileJson.Object(fields, name);
                int offset = Convert.ToInt32(field["offset"]);
                string kind = ProfileJson.Text(field, "kind");
                double oldValue = before.ContainsKey(name) ? before[name] : 0;
                double newValue = after.ContainsKey(name) ? after[name] : 0;
                if (oldValue == newValue) continue;
                if (offset < 0 || offset + (kind == "bool" ? 1 : 4) > body.Length) throw new FormatException("Field outside body.");
                if (kind == "bool") body[offset] = (byte)(newValue != 0 ? 1 : 0);
                else if (kind == "u32")
                {
                    uint current = BitConverter.ToUInt32(body, offset);
                    long value = checked((long)current - (long)oldValue + (long)newValue);
                    if (value < 0 || value > UInt32.MaxValue) throw new ArgumentOutOfRangeException(name);
                    Buffer.BlockCopy(BitConverter.GetBytes((uint)value), 0, body, offset, 4);
                }
                else if (kind == "f32")
                {
                    float value = (float)((float)(BitConverter.ToSingle(body, offset) - (float)oldValue) + (float)newValue);
                    if (Single.IsNaN(value) || Single.IsInfinity(value)) throw new FormatException("Nonfinite physics value.");
                    Buffer.BlockCopy(BitConverter.GetBytes(value), 0, body, offset, 4);
                }
                else throw new FormatException("Unknown scalar encoding.");
            }
            byte[] key, iv;
            using (var sha = SHA256.Create())
            {
                key = sha.ComputeHash(Encoding.UTF8.GetBytes(account + shared + "_key"));
                var digest = sha.ComputeHash(Encoding.UTF8.GetBytes(account + shared + "_iv"));
                iv = new byte[16]; Buffer.BlockCopy(digest, 0, iv, 0, 16);
            }
            byte[] encrypted;
            using (var aes = Aes.Create())
            {
                aes.Key = key; aes.IV = iv; aes.Mode = CipherMode.CBC; aes.Padding = PaddingMode.PKCS7;
                using (var transform = aes.CreateEncryptor()) encrypted = transform.TransformFinalBlock(body, 0, body.Length);
            }
            Array.Clear(key, 0, key.Length);
            return new Dictionary<string, object> { { "speedBodyParamStr", Convert.ToBase64String(encrypted) }, { "itemBodyParamStr", "" } };
        }
        Dictionary<string, double> Effect(Dictionary<string, object> settings)
        {
            var result = new Dictionary<string, double>(StringComparer.Ordinal);
            foreach (var rule in ProfileJson.Object(data, "effects"))
            {
                object raw;
                int level = settings.TryGetValue(rule.Key, out raw) ? Convert.ToInt32(raw) : 0;
                if (level <= 0) continue;
                var levels = ProfileJson.Object(rule.Value);
                object row;
                if (!levels.TryGetValue(level.ToString(System.Globalization.CultureInfo.InvariantCulture), out row))
                    throw new ArgumentException("No external effect row for requested level.");
                foreach (var field in ProfileJson.Object(row))
                {
                    // Each output field has exactly one controlling rule. An
                    // overlap needs an independently verified operation order.
                    if (result.ContainsKey(field.Key)) throw new FormatException("Overlapping effect fields.");
                    result.Add(field.Key, Convert.ToDouble(field.Value));
                }
            }
            return result;
        }
    }
}
