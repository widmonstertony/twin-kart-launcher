using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using Microsoft.Win32;

namespace TwinKartLauncher
{
    public sealed class ClientInspection
    {
        public string Path { get; set; }
        public string Sha256 { get; set; }
        public string Architecture { get; set; }
        public long Size { get; set; }
        public string KnownBuild { get; set; }
        public int PakCount { get; set; }
        public long PakBytes { get; set; }
        public DateTime CheckedUtc { get; set; }
        public string Describe()
        {
            string packages = PakCount > 0 ? " · " + PakCount + " 个 Pak（目录可读，尚未逐图验证）" : "";
            return Architecture + " · " + KnownBuild + packages + "\nSHA-256：" + Sha256;
        }
    }
    public static class ClientDiscovery
    {
        public static string FindDrift()
        {
            var libraries = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            AddSteam(libraries, RegistryValue(Registry.CurrentUser, @"Software\Valve\Steam", "SteamPath"));
            foreach (var view in new[] { RegistryView.Registry32, RegistryView.Registry64 }) {
                try { using (var machine = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, view)) AddSteam(libraries, RegistryValue(machine, @"SOFTWARE\Valve\Steam", "InstallPath")); }
                catch (System.Security.SecurityException) { }
            }
            foreach (string library in libraries) {
                string candidate = System.IO.Path.Combine(library, "steamapps", "common", "KartRider Drift", "KartDrift", "Binaries", "Win64", "KartDrift-Win64-Shipping.exe");
                if (File.Exists(candidate)) return candidate;
            }
            return "";
        }
        static string RegistryValue(RegistryKey root, string path, string name)
        {
            try { using (var key = root.OpenSubKey(path)) return key == null ? null : key.GetValue(name) as string; }
            catch (System.Security.SecurityException) { return null; }
        }
        static void AddSteam(HashSet<string> libraries, string steam)
        {
            if (String.IsNullOrWhiteSpace(steam) || !Directory.Exists(steam)) return;
            libraries.Add(steam);
            string vdf = System.IO.Path.Combine(steam, "steamapps", "libraryfolders.vdf");
            if (!File.Exists(vdf)) return;
            try {
                foreach (Match match in Regex.Matches(File.ReadAllText(vdf), "\"path\"\\s+\"((?:\\\\.|[^\"\\\\])*)\"", RegexOptions.IgnoreCase)) {
                    string path = match.Groups[1].Value.Replace("\\\\", "\\").Replace("\\\"", "\"");
                    if (Directory.Exists(path)) libraries.Add(path);
                }
            } catch (IOException) { } catch (UnauthorizedAccessException) { }
        }
        public static ClientInspection Inspect(string path)
        {
            path = System.IO.Path.GetFullPath(path);
            ushort machine;
            long size;
            using (var reader = new BinaryReader(File.OpenRead(path))) {
                size = reader.BaseStream.Length;
                if (size < 64 || reader.ReadUInt16() != 0x5A4D) throw new InvalidDataException("不是 Windows 可执行文件");
                reader.BaseStream.Position = 0x3C; int offset = reader.ReadInt32();
                if (offset < 64 || offset > size - 6) throw new InvalidDataException("PE 文件损坏");
                reader.BaseStream.Position = offset;
                if (reader.ReadUInt32() != 0x4550) throw new InvalidDataException("PE 签名无效");
                machine = reader.ReadUInt16();
            }
            string hash = ProfileStore.Fingerprint(path);
            var result = new ClientInspection { Path = path, Sha256 = hash, Size = size, CheckedUtc = DateTime.UtcNow,
                Architecture = machine == 0x8664 ? "x64" : machine == 0x14C ? "x86" : "未知架构 0x" + machine.ToString("X4"),
                KnownBuild = hash == "5041d9f5d45adb4b1c932d8056a89f70db7fa98f93614ac30685cd1adeb9d9c2" ? "Drift Steam build 15490952" : "尚未登记的客户端构建" };
            if (result.KnownBuild.StartsWith("Drift")) {
                var directory = new DirectoryInfo(System.IO.Path.GetDirectoryName(path));
                if (directory.Parent != null && directory.Parent.Parent != null) {
                    string paks = System.IO.Path.Combine(directory.Parent.Parent.FullName, "Content", "Paks");
                    if (Directory.Exists(paks)) {
                        var files = new DirectoryInfo(paks).GetFiles("*.pak", SearchOption.TopDirectoryOnly);
                        result.PakCount = files.Length; result.PakBytes = files.Sum(file => file.Length);
                    }
                }
            }
            return result;
        }
    }
}
