using System;
using System.Collections.Generic;
using TwinKartLauncher.Gateway;

static class PackedFieldLayoutTests
{
    static PackedField F(string name, int width) { return new PackedField(name, width); }
    static void Reject(Action action)
    { try { action(); } catch (ArgumentException) { return; } throw new Exception("Expected rejection"); }
    static int Main()
    {
        var source = new PackedFieldLayout(new[] { F("Acceleration", 4), F("Removed", 4), F("Enabled", 1), F("Count", 4) });
        var target = new PackedFieldLayout(new[] { F("enabled", 1), F("Added", 1), F("Count", 4), F("Acceleration", 4) });
        var defaults = new Dictionary<string, byte[]> { { "Added", new byte[] { 0 } } };
        byte[] input = { 0, 0, 128, 63, 9, 8, 7, 6, 1, 2, 0, 0, 0 };
        byte[] converted = source.ConvertTo(input, target, defaults);
        if (BitConverter.ToString(converted) != "01-00-02-00-00-00-00-00-80-3F") throw new Exception("Reorder/delete/insert failed");
        converted[0] = 9;
        if (input[8] != 1 || defaults["Added"][0] != 0) throw new Exception("Input mutation");
        Reject(() => source.ConvertTo(new byte[12], target, defaults));
        Reject(() => source.ConvertTo(new byte[14], target, defaults));
        Reject(() => source.ConvertTo(input, target, null));
        Reject(() => source.ConvertTo(input, target, new Dictionary<string, byte[]> { { "Added", new byte[2] } }));
        Reject(() => source.ConvertTo(input, target, new Dictionary<string, byte[]> { { "Added", new byte[1] }, { "Typo", new byte[1] } }));
        Reject(() => source.ConvertTo(input, new PackedFieldLayout(new[] { F("Enabled", 4) }), null));
        Reject(() => new PackedFieldLayout(new[] { F("one", 1), F("ONE", 1) }));
        Reject(() => new PackedFieldLayout(new PackedField[0]));
        Reject(() => new PackedField("bad/path", 1));
        Reject(() => new PackedField("value", 0));
        Console.WriteLine("PASS packed field insertion, deletion, reordering, width/length validation and input isolation"); return 0;
    }
}
