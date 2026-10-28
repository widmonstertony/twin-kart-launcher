using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Web.Script.Serialization;

namespace TwinKartLauncher.Gateway
{
    // Caller-supplied data is external to the source repository. This catalog
    // serves bounded UTF-8 text documents, not executable code or reply templates.
    public sealed class DynamicDataDocument
    {
        public string Name { get; set; }
        public string Content { get; set; }
    }

    public sealed class DynamicDataCatalog
    {
        sealed class Entry { public string Text, Hash; }
        readonly Dictionary<string, Entry> entries = new Dictionary<string, Entry>(StringComparer.Ordinal);
        public static bool ValidName(string name)
        {
            if (string.IsNullOrEmpty(name) || name.Length > 96 || name == "." || name == "..") return false;
            foreach (char c in name)
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                      (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return false;
            return true;
        }

        public static DynamicDataCatalog Load(string path)
        {
            var file = new FileInfo(path);
            if (!file.Exists || file.Length > 4 * 1024 * 1024) throw new ArgumentException("Invalid external data catalog size");
            var json = new JavaScriptSerializer { MaxJsonLength = 4 * 1024 * 1024, RecursionLimit = 8 };
            return new DynamicDataCatalog(json.Deserialize<DynamicDataDocument[]>(File.ReadAllText(file.FullName, new UTF8Encoding(false, true))));
        }

        public DynamicDataCatalog(DynamicDataDocument[] documents)
        {
            if (documents == null || documents.Length > 512) throw new ArgumentException("Invalid document count");
            long total = 0;
            foreach (var document in documents)
            {
                if (document == null || !ValidName(document.Name) || document.Content == null || entries.ContainsKey(document.Name))
                    throw new ArgumentException("Invalid or duplicate dynamic data name");
                byte[] bytes = new UTF8Encoding(false, true).GetBytes(document.Content);
                total += bytes.Length;
                if (bytes.Length > 600 * 1024 || total > 3 * 1024 * 1024)
                    throw new ArgumentException("Dynamic data exceeds limits");
                string hash;
                using (var sha = SHA256.Create()) hash = BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-", "").ToLowerInvariant();
                entries.Add(document.Name, new Entry { Text = document.Content, Hash = hash });
            }
        }

        public bool TryReply(string name, int requestId, out Dictionary<string, object> response)
        {
            Entry entry; response = null;
            if (!ValidName(name) || !entries.TryGetValue(name, out entry)) return false;
            response = new Dictionary<string, object> {
                { "_type", "PrGetPublicDynData" }, { "reqId", requestId },
                { "result", new Dictionary<string, object> { { "code", 0 } } }, { "desc", "" },
                { "name", name }, { "hash", entry.Hash }, { "hasData", true },
                { "isBinary", false }, { "data", entry.Text }, { "isCompressed", false },
                { "uncompressedSize", 0 }, { "compressedData", "" }
            };
            return true;
        }
    }
}
