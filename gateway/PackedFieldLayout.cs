using System;
using System.Collections.Generic;

namespace TwinKartLauncher.Gateway
{
    public sealed class PackedField
    {
        public string Name { get; private set; }
        public int Width { get; private set; }
        public PackedField(string name, int width)
        {
            if (String.IsNullOrEmpty(name) || name.Length > 96 || width < 1 || width > 16)
                throw new ArgumentException("Invalid field definition");
            foreach (char c in name)
                if (!(Char.IsLetterOrDigit(c) || c == '_')) throw new ArgumentException("Invalid field name");
            Name = name; Width = width;
        }
    }

    // Converts packed scalar records using explicit, externally established layouts.
    // No vehicle values, client addresses, encryption keys or game assets are included.
    public sealed class PackedFieldLayout
    {
        readonly PackedField[] fields;
        public int ByteCount { get; private set; }
        public PackedFieldLayout(IEnumerable<PackedField> definition)
        {
            if (definition == null) throw new ArgumentNullException("definition");
            var entries = new List<PackedField>();
            var names = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (PackedField field in definition)
            {
                if (field == null || !names.Add(field.Name) || entries.Count >= 512)
                    throw new ArgumentException("Invalid or duplicate layout field");
                entries.Add(field); ByteCount = checked(ByteCount + field.Width);
            }
            if (entries.Count == 0) throw new ArgumentException("Empty layout");
            fields = entries.ToArray();
        }

        public byte[] ConvertTo(byte[] source, PackedFieldLayout destination,
            IDictionary<string, byte[]> explicitDefaults)
        {
            if (source == null || source.Length != ByteCount)
                throw new ArgumentException("Packed source length does not match layout");
            if (destination == null) throw new ArgumentNullException("destination");
            var offsets = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
            var widths = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
            int cursor = 0;
            foreach (PackedField field in fields)
            { offsets.Add(field.Name, cursor); widths.Add(field.Name, field.Width); cursor += field.Width; }
            var defaults = new Dictionary<string, byte[]>(StringComparer.OrdinalIgnoreCase);
            if (explicitDefaults != null)
                foreach (var entry in explicitDefaults) defaults.Add(entry.Key, entry.Value);
            var usedDefaults = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            var result = new byte[destination.ByteCount]; cursor = 0;
            foreach (PackedField field in destination.fields)
            {
                int offset;
                if (offsets.TryGetValue(field.Name, out offset))
                {
                    if (widths[field.Name] != field.Width) throw new ArgumentException("Field width mismatch");
                    Buffer.BlockCopy(source, offset, result, cursor, field.Width);
                }
                else
                {
                    byte[] value;
                    if (!defaults.TryGetValue(field.Name, out value) || value == null || value.Length != field.Width)
                        throw new ArgumentException("Missing explicit field value");
                    Buffer.BlockCopy(value, 0, result, cursor, field.Width); usedDefaults.Add(field.Name);
                }
                cursor += field.Width;
            }
            if (usedDefaults.Count != defaults.Count) throw new ArgumentException("Unused explicit field value");
            return result;
        }
    }
}
