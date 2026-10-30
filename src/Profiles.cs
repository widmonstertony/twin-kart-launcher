using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Web.Script.Serialization;

namespace TwinKartLauncher
{
    public sealed class LocalRaceSettings
    {
        public string Track { get; set; }
        public int Mode { get; set; }
        public int AiCount { get; set; }
        public int Laps { get; set; }
        public LocalRaceSettings() { Track="Pirate_R01"; Mode=1; AiCount=7; Laps=3; }
        public static bool SpitemMode(int mode) { return mode>=105 && mode<=107; }
        public static bool InfiniteBoostMode(int mode) { return mode>=101 && mode<=103; }
        public static bool FlagMode(int mode) { return mode==104; }
        public static int TeamSizeFor(int mode) {
            if(FlagMode(mode)) return 4;
            if((mode<1 || mode>6) && !SpitemMode(mode) && !InfiniteBoostMode(mode)) return 0;
            int group=SpitemMode(mode)?mode-105:InfiniteBoostMode(mode)?mode-101:(mode-1)%3;
            return group==0?1:group==1?2:4;
        }
        public static bool TrackSupports(int mask,int mode) {
            return mask>=0 && mask<=31 && TeamSizeFor(mode)>0 && (mask & (1 << (FlagMode(mode)?4:SpitemMode(mode)?3:mode<=3 || InfiniteBoostMode(mode)?2:1)))!=0;
        }
        public static bool ValidPopulation(int mode,int ai) {
            if(FlagMode(mode)) return ai==7;
            int size=TeamSizeFor(mode),racers=ai+1;
            return size>0 && ai>=1 && ai<=7 && (size==1 || (racers>=size*2 && racers%size==0));
        }
        public bool IsValid { get { return System.Text.RegularExpressions.Regex.IsMatch(Track??"","^[A-Za-z][A-Za-z0-9_]{0,95}$") &&
            ValidPopulation(Mode,AiCount) && Laps>=1 && Laps<=9; } }
        internal void WriteControl(string path) {
            if(!IsValid) throw new ArgumentException("Invalid local race settings.");
            string text=String.Format(System.Globalization.CultureInfo.InvariantCulture,"TKRACE1 {0} {1} {2} {3} {4}\n",Guid.NewGuid().ToString("N"),Track,Mode,AiCount,Laps);
            string temporary=path+"."+Guid.NewGuid().ToString("N")+".tmp";
            try { File.WriteAllText(temporary,text,new System.Text.UTF8Encoding(false)); if(File.Exists(path)) File.Replace(temporary,path,null); else File.Move(temporary,path); }
            finally { if(File.Exists(temporary)) File.Delete(temporary); }
        }
    }
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
        public LocalRaceSettings Race { get; set; }
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
            Race = new LocalRaceSettings();
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
            if(result.Race==null || !result.Race.IsValid) result.Race=new LocalRaceSettings();
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
