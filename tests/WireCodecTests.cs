using System;
using TwinKartLauncher.Gateway;

internal static class WireCodecTests
{
    static int Main()
    {
        try
        {
            const string json = "{\"_type\":\"PqEcho\",\"reqId\":1}";
            const string secret = "offline-test-secret";
            const string suffix = "3a3afbe9"; // independently computed SHA-256 prefix
            byte[] key = FromHex("603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4");
            byte[] iv = FromHex("000102030405060708090a0b0c0d0e0f");
            string base64Key = Convert.ToBase64String(key);
            string wire, error;
            WireDecodeResult decoded;

            byte[] nistPlain = FromHex("6bc1bee22e409f96e93d7e117393172a");
            byte[] nistCipher = WireCodec.EncryptCbcPkcs7(nistPlain, key, iv);
            Check(nistCipher.Length == 32 &&
                  ToHex(nistCipher, 0, 16) == "f58c4c04d6e5f1ba779eabfb5f7bfbd6",
                  "NIST SP 800-38A AES-256-CBC first block");

            Check(WireCodec.TryEncode(json, secret, null, out wire, out error), "plaintext encode: " + error);
            Check(wire == json + "@" + suffix, "known SHA-256 suffix and exact JSON spelling");
            Check(WireCodec.TryDecode(wire, null, secret, out decoded, out error), "hash verify: " + error);
            Check(decoded.Json == json && !decoded.WasEncrypted && decoded.HashStatus == WireHashStatus.Verified,
                  "verified plaintext fields");
            Check(WireCodec.TryDecode(wire, null, null, out decoded, out error) &&
                  decoded.Json == json && decoded.HashStatus == WireHashStatus.PresentUnverified,
                  "unknown hash secret must remain unverified");
            Check(WireCodec.TryDecode(json, "invalid-key", null, out decoded, out error) &&
                  decoded.HashStatus == WireHashStatus.NotPresent && !decoded.WasEncrypted,
                  "clear JSON bypasses key decoding");

            Check(WireCodec.TryEncodeWithIv(json, secret, base64Key, iv, out wire, out error),
                  "fixed-IV encode: " + error);
            byte[] envelope = Convert.FromBase64String(wire);
            Check(ToHex(envelope, 0, 16) == ToHex(iv, 0, 16) && envelope.Length >= 32,
                  "IV prefix and ciphertext length");
            Check(WireCodec.TryDecode(wire, base64Key, secret, out decoded, out error),
                  "encrypted decode: " + error);
            Check(decoded.Json == json && decoded.WasEncrypted && decoded.HashStatus == WireHashStatus.Verified,
                  "encrypted result fields");
            Check(!WireCodec.TryDecode(wire, "invalid-key", secret, out decoded, out error),
                  "malformed AES key accepted");
            Check(!WireCodec.TryEncode(json, null, "AA==", out wire, out error),
                  "short AES key accepted");
            Check(!WireCodec.TryEncodeWithIv(json, null, base64Key, new byte[15], out wire, out error),
                  "short IV accepted");

            const string unicodeJson = "{\"name\":\"雪❄\"}";
            Check(WireCodec.TryEncode(unicodeJson, "秘", null, out wire, out error) &&
                  wire == unicodeJson + "@ca70ec16", "Unicode UTF-8 SHA-256 suffix");
            Check(WireCodec.TryEncodeWithIv(unicodeJson, "秘", base64Key, iv, out wire, out error) &&
                  WireCodec.TryDecode(wire, base64Key, "秘", out decoded, out error) &&
                  decoded.Json == unicodeJson && decoded.HashStatus == WireHashStatus.Verified,
                  "Unicode encrypted UTF-8 roundtrip");

            string intact = json + "@" + suffix;
            Check(!WireCodec.TryDecode(intact.Replace("\"reqId\":1", "\"reqId\":2"), null,
                                       secret, out decoded, out error), "tampered JSON accepted");
            Check(!WireCodec.TryDecode(intact, null, "wrong-secret", out decoded, out error),
                  "wrong hash secret accepted");
            Check(!WireCodec.TryDecode(json + "@3A3AFBE9", null, secret, out decoded, out error),
                  "uppercase hash accepted");
            Check(!WireCodec.TryDecode(json, null, secret, out decoded, out error),
                  "missing required hash accepted");
            Check(!WireCodec.TryEncode("{\0}", null, null, out wire, out error), "NUL JSON accepted");
            Check(!WireCodec.TryDecode("{\0}", null, null, out decoded, out error), "NUL wire accepted");
            Check(!WireCodec.TryEncode("{", null, null, out wire, out error), "malformed JSON accepted for encode");
            Check(!WireCodec.TryDecode("{}garbage", null, null, out decoded, out error),
                  "trailing packet garbage accepted");
            Check(!WireCodec.TryEncode("{" + new string('a', WireCodec.MaxJsonBytes) + "}",
                                       null, null, out wire, out error), "oversized JSON accepted");

            Check(WireCodec.TryEncodeWithIv("{}", null, base64Key, iv, out wire, out error),
                  "padding fixture encode: " + error);
            byte[] paddedEnvelope = Convert.FromBase64String(wire);
            paddedEnvelope[15] ^= 0x10; // last PKCS#7 byte becomes 14^16 = 30
            Check(!WireCodec.TryDecode(Convert.ToBase64String(paddedEnvelope), base64Key,
                                       null, out decoded, out error), "invalid padding accepted");
            Check(!WireCodec.TryDecode(Convert.ToBase64String(iv), base64Key, null,
                                       out decoded, out error), "IV-only envelope accepted");
            byte[] wrongLength = new byte[33];
            Check(!WireCodec.TryDecode(Convert.ToBase64String(wrongLength), base64Key,
                                       null, out decoded, out error), "nonblock ciphertext accepted");
            Check(!WireCodec.TryDecode(wire + " ", base64Key, null, out decoded, out error),
                  "Base64 whitespace accepted");
            Console.WriteLine("PASS wire AES vector, SHA suffix, Unicode, bounds and malformed inputs");
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine("FAIL " + error);
            return 1;
        }
    }

    static byte[] FromHex(string hex)
    {
        byte[] bytes = new byte[hex.Length / 2];
        for (int i = 0; i < bytes.Length; i++)
            bytes[i] = Convert.ToByte(hex.Substring(i * 2, 2), 16);
        return bytes;
    }

    static string ToHex(byte[] bytes, int offset, int length)
    {
        char[] digits = "0123456789abcdef".ToCharArray();
        char[] chars = new char[length * 2];
        for (int i = 0; i < length; i++)
        {
            chars[i * 2] = digits[bytes[offset + i] >> 4];
            chars[i * 2 + 1] = digits[bytes[offset + i] & 15];
        }
        return new string(chars);
    }

    static void Check(bool condition, string message)
    {
        if (!condition) throw new Exception(message);
    }
}
