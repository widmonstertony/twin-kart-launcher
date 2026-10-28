using System;
using System.Collections.Generic;
using System.Security.Cryptography;
using System.Text;
using System.Web.Script.Serialization;

namespace TwinKartLauncher.Gateway
{
    public enum WireHashStatus
    {
        NotPresent,
        PresentUnverified,
        Verified
    }

    public sealed class WireDecodeResult
    {
        // Candidate JSON text. PacketJsonCodec must validate its object shape.
        public string Json { get; internal set; }
        public bool WasEncrypted { get; internal set; }
        public WireHashStatus HashStatus { get; internal set; }
    }

    // Byte envelope based on independently observed Android interfaces. This
    // is not authentication, a WebSocket transport, or proof of PC compatibility.
    public static class WireCodec
    {
        public const int MaxJsonBytes = 1024 * 1024;
        public const int MaxWireChars = 2 * 1024 * 1024;
        const int MaxSecretChars = 4096;
        const int IvBytes = 16;
        const int KeyBytes = 32;
        const int HashSuffixChars = 9;
        const int MaxPlaintextBytes = MaxJsonBytes + HashSuffixChars;
        const int MaxCipherBytes = ((MaxPlaintextBytes / 16) + 1) * 16;
        const int MaxEnvelopeBytes = IvBytes + MaxCipherBytes;
        static readonly UTF8Encoding Utf8 = new UTF8Encoding(false, true);
        static readonly char[] Hex = "0123456789abcdef".ToCharArray();

        // A null or empty secret omits the digest suffix. A null or empty key
        // emits plaintext. The caller supplies the exact JSON byte spelling;
        // this method does not canonicalize or serialize an object.
        public static bool TryEncode(string json, string hashSecret, string base64Key,
                                     out string wire, out string error)
        {
            byte[] iv = null;
            if (!string.IsNullOrEmpty(base64Key))
            {
                iv = new byte[IvBytes];
                using (RandomNumberGenerator rng = RandomNumberGenerator.Create())
                    rng.GetBytes(iv);
            }
            return TryEncodeWithIv(json, hashSecret, base64Key, iv, out wire, out error);
        }

        // Fixed-IV seam for synthetic compatibility vectors only. Production
        // callers use TryEncode, which creates a new cryptographic random IV.
        internal static bool TryEncodeWithIv(string json, string hashSecret, string base64Key,
                                             byte[] iv, out string wire, out string error)
        {
            wire = null; error = null;
            byte[] jsonBytes;
            if (!TryJsonBytes(json, out jsonBytes, out error)) return false;
            if (!ValidSecret(hashSecret, out error)) return false;
            string plaintext = json;
            if (!string.IsNullOrEmpty(hashSecret))
            {
                string hash;
                if (!TryHashPrefix(json, hashSecret, out hash, out error)) return false;
                plaintext += "@" + hash;
            }
            if (string.IsNullOrEmpty(base64Key))
            {
                wire = plaintext;
                return true;
            }
            byte[] key;
            if (!TryKey(base64Key, out key, out error)) return false;
            if (iv == null || iv.Length != IvBytes)
            { error = "IV must contain exactly 16 bytes."; return false; }
            byte[] plainBytes;
            try { plainBytes = Utf8.GetBytes(plaintext); }
            catch (EncoderFallbackException) { error = "Plaintext contains invalid UTF-16."; return false; }
            if (plainBytes.Length > MaxPlaintextBytes)
            { error = "Plaintext is too large."; return false; }
            try
            {
                byte[] cipher = EncryptCbcPkcs7(plainBytes, key, iv);
                byte[] envelope = new byte[IvBytes + cipher.Length];
                Buffer.BlockCopy(iv, 0, envelope, 0, IvBytes);
                Buffer.BlockCopy(cipher, 0, envelope, IvBytes, cipher.Length);
                wire = Convert.ToBase64String(envelope);
                return true;
            }
            catch (CryptographicException)
            { error = "Encryption failed."; return false; }
        }

        // A clear message beginning with '{' follows the Android receive
        // branch and does not require the AES key. With no hash secret, a
        // syntactically recognizable suffix is stripped but stays unverified.
        // Decryption never means account or session authentication succeeded.
        public static bool TryDecode(string wire, string base64Key, string hashSecret,
                                     out WireDecodeResult result, out string error)
        {
            result = null; error = null;
            if (string.IsNullOrEmpty(wire) || wire.Length > MaxWireChars || wire.IndexOf('\0') >= 0)
            { error = "Wire text is absent, oversized, or contains NUL."; return false; }
            if (!ValidSecret(hashSecret, out error)) return false;
            bool encrypted = wire[0] != '{';
            string plaintext = wire;
            if (encrypted)
            {
                byte[] key;
                if (!TryKey(base64Key, out key, out error)) return false;
                byte[] envelope;
                if (!TryStrictBase64(wire, MaxEnvelopeBytes, out envelope))
                { error = "Encrypted envelope is not canonical bounded Base64."; return false; }
                if (envelope.Length < IvBytes + IvBytes ||
                    (envelope.Length - IvBytes) % IvBytes != 0)
                { error = "Encrypted envelope has an invalid ciphertext length."; return false; }
                byte[] iv = new byte[IvBytes];
                Buffer.BlockCopy(envelope, 0, iv, 0, IvBytes);
                byte[] cipher = new byte[envelope.Length - IvBytes];
                Buffer.BlockCopy(envelope, IvBytes, cipher, 0, cipher.Length);
                byte[] plainBytes;
                try { plainBytes = DecryptCbcPkcs7(cipher, key, iv); }
                catch (CryptographicException)
                { error = "Ciphertext or padding is invalid."; return false; }
                if (plainBytes.Length > MaxPlaintextBytes)
                { error = "Decrypted plaintext is too large."; return false; }
                try { plaintext = Utf8.GetString(plainBytes); }
                catch (DecoderFallbackException)
                { error = "Decrypted plaintext is not valid UTF-8."; return false; }
            }
            byte[] bytes;
            if (!TryBoundedUtf8(plaintext, MaxPlaintextBytes, out bytes) ||
                plaintext.Length == 0 || plaintext[0] != '{')
            { error = "Plaintext is not a bounded JSON object candidate."; return false; }

            bool suffix = HasHashSuffix(plaintext);
            string json = suffix ? plaintext.Substring(0, plaintext.Length - HashSuffixChars) : plaintext;
            if (json.Length == 0 || json[0] != '{')
            { error = "Hash suffix has no JSON object candidate."; return false; }
            WireHashStatus status = suffix ? WireHashStatus.PresentUnverified : WireHashStatus.NotPresent;
            if (!string.IsNullOrEmpty(hashSecret))
            {
                if (!suffix) { error = "Expected hash suffix is missing or malformed."; return false; }
                string expected;
                if (!TryHashPrefix(json, hashSecret, out expected, out error)) return false;
                if (!EqualHashPrefix(plaintext, expected))
                { error = "Hash suffix does not match."; return false; }
                status = WireHashStatus.Verified;
            }
            byte[] jsonBytes;
            if (!TryJsonBytes(json, out jsonBytes, out error)) return false;
            result = new WireDecodeResult { Json = json, WasEncrypted = encrypted, HashStatus = status };
            return true;
        }

        // Internal primitive is independently tested against the first block
        // of the published NIST SP 800-38A AES-256-CBC example. PKCS#7 adds a
        // further block when input length is already 16 bytes.
        internal static byte[] EncryptCbcPkcs7(byte[] plaintext, byte[] key, byte[] iv)
        {
            using (Aes aes = Aes.Create())
            {
                aes.KeySize = 256; aes.BlockSize = 128;
                aes.Mode = CipherMode.CBC; aes.Padding = PaddingMode.PKCS7;
                aes.Key = key; aes.IV = iv;
                using (ICryptoTransform transform = aes.CreateEncryptor())
                    return transform.TransformFinalBlock(plaintext, 0, plaintext.Length);
            }
        }

        static byte[] DecryptCbcPkcs7(byte[] ciphertext, byte[] key, byte[] iv)
        {
            using (Aes aes = Aes.Create())
            {
                aes.KeySize = 256; aes.BlockSize = 128;
                aes.Mode = CipherMode.CBC; aes.Padding = PaddingMode.PKCS7;
                aes.Key = key; aes.IV = iv;
                using (ICryptoTransform transform = aes.CreateDecryptor())
                    return transform.TransformFinalBlock(ciphertext, 0, ciphertext.Length);
            }
        }

        static bool TryJsonBytes(string json, out byte[] bytes, out string error)
        {
            bytes = null; error = null;
            if (string.IsNullOrEmpty(json) || json[0] != '{' || json.IndexOf('\0') >= 0)
            { error = "JSON must start with '{' and contain no NUL."; return false; }
            if (!TryBoundedUtf8(json, MaxJsonBytes, out bytes))
            { error = "JSON is oversized or contains invalid UTF-16."; return false; }
            try
            {
                var parser = new JavaScriptSerializer {
                    MaxJsonLength = MaxJsonBytes,
                    RecursionLimit = 64
                };
                if (!(parser.DeserializeObject(json) is Dictionary<string, object>))
                { error = "Expected a JSON object."; return false; }
            }
            catch (ArgumentException)
            { error = "JSON object is malformed."; return false; }
            catch (InvalidOperationException)
            { error = "JSON object is malformed or too deeply nested."; return false; }
            return true;
        }

        static bool TryBoundedUtf8(string value, int maximum, out byte[] bytes)
        {
            bytes = null;
            try { bytes = Utf8.GetBytes(value); }
            catch (EncoderFallbackException) { return false; }
            return bytes.Length <= maximum;
        }

        static bool ValidSecret(string secret, out string error)
        {
            error = null;
            if (string.IsNullOrEmpty(secret)) return true;
            if (secret.Length > MaxSecretChars || secret.IndexOf('\0') >= 0)
            { error = "Hash secret is oversized or contains NUL."; return false; }
            byte[] bytes;
            if (!TryBoundedUtf8(secret, MaxSecretChars * 4, out bytes))
            { error = "Hash secret contains invalid UTF-16."; return false; }
            return true;
        }

        static bool TryKey(string base64Key, out byte[] key, out string error)
        {
            key = null; error = null;
            if (string.IsNullOrEmpty(base64Key) || base64Key.Length != 44 ||
                !TryStrictBase64(base64Key, KeyBytes, out key) || key.Length != KeyBytes)
            { error = "AES key must be canonical Base64 for exactly 32 bytes."; return false; }
            return true;
        }

        static bool TryStrictBase64(string value, int maximumBytes, out byte[] decoded)
        {
            decoded = null;
            if (string.IsNullOrEmpty(value) || value.Length % 4 != 0 ||
                value.Length > MaxWireChars) return false;
            foreach (char ch in value)
                if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                      (ch >= '0' && ch <= '9') || ch == '+' || ch == '/' || ch == '='))
                    return false;
            try { decoded = Convert.FromBase64String(value); }
            catch (FormatException) { return false; }
            return decoded.Length <= maximumBytes &&
                   string.Equals(Convert.ToBase64String(decoded), value, StringComparison.Ordinal);
        }

        static bool TryHashPrefix(string json, string secret, out string prefix, out string error)
        {
            prefix = null; error = null;
            byte[] combined;
            try { combined = Utf8.GetBytes(json + secret); }
            catch (EncoderFallbackException)
            { error = "Hash input contains invalid UTF-16."; return false; }
            byte[] hash;
            using (SHA256 sha = SHA256.Create()) hash = sha.ComputeHash(combined);
            var chars = new char[8];
            for (int i = 0; i < 4; i++)
            {
                chars[2 * i] = Hex[hash[i] >> 4];
                chars[2 * i + 1] = Hex[hash[i] & 15];
            }
            prefix = new string(chars);
            return true;
        }

        static bool HasHashSuffix(string text)
        {
            if (text.Length < HashSuffixChars + 2 ||
                text[text.Length - HashSuffixChars] != '@') return false;
            for (int i = text.Length - 8; i < text.Length; i++)
            {
                char ch = text[i];
                if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) return false;
            }
            return true;
        }

        static bool EqualHashPrefix(string plaintext, string expected)
        {
            int difference = 0;
            for (int i = 0; i < 8; i++)
                difference |= plaintext[plaintext.Length - 8 + i] ^ expected[i];
            return difference == 0;
        }
    }
}
