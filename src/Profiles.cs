using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Web.Script.Serialization;

namespace TwinKartLauncher
{
    public sealed class CockpitSettings
    {
        public bool Enabled { get; set; }
        public bool FollowHead { get; set; }
        public int SeatForward { get; set; }
        public int SeatHeight { get; set; }
        public int EyeForward { get; set; }
        public int EyeHeight { get; set; }
        public int Pitch { get; set; }
        public int FieldOfView { get; set; }
        public CockpitSettings() { Enabled=true; FollowHead=true; SeatForward=-30; SeatHeight=140; EyeForward=12; EyeHeight=8; Pitch=-10; FieldOfView=90; }
        public CockpitSettings Copy() { return (CockpitSettings)MemberwiseClone(); }
        public bool IsValid { get { return SeatForward>=-200 && SeatForward<=300 && SeatHeight>=20 && SeatHeight<=250 &&
            EyeForward>=-50 && EyeForward<=80 && EyeHeight>=-30 && EyeHeight<=80 && Pitch>=-40 && Pitch<=30 && FieldOfView>=60 && FieldOfView<=120; } }
        public string NativeControl()
        {
            if(!IsValid) throw new ArgumentOutOfRangeException("CockpitSettings");
            return String.Join(" ",new[]{Enabled?1:0,SeatForward,SeatHeight,Pitch,FieldOfView,FollowHead?1:0,EyeForward,EyeHeight}.Select(value=>value.ToString(System.Globalization.CultureInfo.InvariantCulture)))+"\n";
        }
        internal void WriteNativeControl(string path)
        {
            string text=NativeControl(),temporary=path+"."+Guid.NewGuid().ToString("N")+".tmp";
            try {
                File.WriteAllText(temporary,text,new System.Text.UTF8Encoding(false));
                if(File.Exists(path)) File.Replace(temporary,path,null); else File.Move(temporary,path);
            } finally { if(File.Exists(temporary)) File.Delete(temporary); }
        }
    }
    public enum GameEdition { Classic, Drift }

    public sealed class AxisBinding
    {
        public string DeviceGuid { get; set; }
        public int Offset { get; set; }
        public int NegativeOrReleased { get; set; }
        public int Center { get; set; }
        public int PositiveOrPressed { get; set; }
        public double DeadZone { get; set; }
        public bool Calibrated { get; set; }

        public AxisBinding() { DeviceGuid = ""; Offset = 0; DeadZone = 0.03; }
        public bool IsValid(bool steering)
        {
            Guid parsed;
            if (!Calibrated || !Guid.TryParse(DeviceGuid, out parsed) || Offset < 0 || Offset > 28 || Offset % 4 != 0) return false;
            if (double.IsNaN(DeadZone) || DeadZone < 0 || DeadZone >= 0.5) return false;
            long span = (long)PositiveOrPressed - NegativeOrReleased;
            if (Math.Abs(span) < 100) return false;
            if (!steering) return true;
            return Center > Math.Min(NegativeOrReleased, PositiveOrPressed) + 20 && Center < Math.Max(NegativeOrReleased, PositiveOrPressed) - 20;
        }
        public double Normalize(int raw, bool steering)
        {
            if (!IsValid(steering)) return 0;
            double value;
            if (steering)
            {
                double delta = (double)raw - Center;
                bool positive = delta * ((double)PositiveOrPressed - Center) >= 0;
                value = positive ? delta / ((double)PositiveOrPressed - Center) : -delta / ((double)NegativeOrReleased - Center);
                value = Clamp(value, -1, 1);
                double absolute = Math.Abs(value);
                return absolute <= DeadZone ? 0 : Math.Sign(value) * (absolute - DeadZone) / (1 - DeadZone);
            }
            value = Clamp(((double)raw - NegativeOrReleased) / ((double)PositiveOrPressed - NegativeOrReleased), 0, 1);
            return value <= DeadZone ? 0 : (value - DeadZone) / (1 - DeadZone);
        }
        static double Clamp(double value, double min, double max) { return Math.Max(min, Math.Min(max, value)); }
    }

    public sealed class LauncherProfile
    {
        public GameEdition Edition { get; set; }
        public string ClientPath { get; set; }
        // A file fingerprint only; never a claim of game compatibility.
        public string FingerprintSha256 { get; set; }
        public string OfflineManifestPath { get; set; }
        public CockpitSettings Cockpit { get; set; }
        public AxisBinding Steering { get; set; }
        public AxisBinding Throttle { get; set; }
        public AxisBinding Brake { get; set; }
        public string ButtonDeviceGuid { get; set; }
        public int DriftButton { get; set; }
        public int ItemButton { get; set; }
        public LauncherProfile()
        {
            ClientPath = ""; FingerprintSha256 = ""; ButtonDeviceGuid = "";
            Steering = new AxisBinding(); Throttle = new AxisBinding(); Brake = new AxisBinding();
            DriftButton = -1; ItemButton = -1;
            Cockpit = new CockpitSettings();
        }
        public bool InputReady { get { return Steering.IsValid(true) && Throttle.IsValid(false) && Brake.IsValid(false); } }
    }

    public sealed class ProfileStore
    {
        readonly string directory;
        public ProfileStore(string directory) { this.directory = Path.GetFullPath(directory); }
        public LauncherProfile Load(GameEdition edition)
        {
            string path = Path.Combine(directory, edition.ToString().ToLowerInvariant() + ".json");
            if (!File.Exists(path)) return new LauncherProfile { Edition = edition };
            var result = new JavaScriptSerializer().Deserialize<LauncherProfile>(File.ReadAllText(path));
            if (result == null || result.Edition != edition || result.Steering == null || result.Throttle == null || result.Brake == null)
                throw new InvalidDataException("设备配置不完整，请重新校准。");
            if(result.Cockpit==null || !result.Cockpit.IsValid) result.Cockpit=new CockpitSettings();
            return result;
        }
        public void Save(LauncherProfile profile)
        {
            Directory.CreateDirectory(directory);
            string path = Path.Combine(directory, profile.Edition.ToString().ToLowerInvariant() + ".json");
            string temporary = path + ".new";
            File.WriteAllText(temporary, new JavaScriptSerializer().Serialize(profile));
            if (File.Exists(path)) File.Replace(temporary, path, path + ".backup");
            else File.Move(temporary, path);
        }
        public static string Fingerprint(string path)
        {
            using (var stream = File.OpenRead(path))
            using (var sha = SHA256.Create())
                return BitConverter.ToString(sha.ComputeHash(stream)).Replace("-", "").ToLowerInvariant();
        }
    }
}
