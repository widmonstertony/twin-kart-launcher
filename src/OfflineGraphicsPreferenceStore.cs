using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using System.Web.Script.Serialization;

namespace TwinKartLauncher
{
    public sealed class OfflineGraphicsPreference
    {
        public int NeuralStyle { get; set; }
        public int NeuralPasses { get; set; }
        public double Brightness { get; set; }
        public bool SuperResolutionAndFrameGeneration { get; set; }
    }

    // Edits only user-selectable graphics scalars in the private manifest.
    // Paths, fingerprints, modules, race settings and provider pins are kept.
    public static class OfflineGraphicsPreferenceStore
    {
        static readonly JavaScriptSerializer Json=new JavaScriptSerializer { MaxJsonLength=16*1024*1024 };
        static Dictionary<string,object> Object(object value,string name)
        {
            var result=value as Dictionary<string,object>;
            if(result==null)throw new InvalidDataException("Missing or malformed "+name+" object.");
            return result;
        }
        static int Integer(Dictionary<string,object> value,string key)
        { object raw;if(!value.TryGetValue(key,out raw))throw new InvalidDataException("Missing "+key+".");return Convert.ToInt32(raw,CultureInfo.InvariantCulture); }
        static double Number(Dictionary<string,object> value,string key,double fallback)
        { object raw;return value.TryGetValue(key,out raw)?Convert.ToDouble(raw,CultureInfo.InvariantCulture):fallback; }
        static bool Boolean(Dictionary<string,object> value,string key)
        { object raw;if(!value.TryGetValue(key,out raw))throw new InvalidDataException("Missing "+key+".");return Convert.ToBoolean(raw,CultureInfo.InvariantCulture); }
        static void Validate(int style,double brightness)
        {
            if(style < -1 || style > 2)throw new ArgumentException("请选择 Default、Natural、Movie 或关闭。");
            if(Double.IsNaN(brightness) || Double.IsInfinity(brightness) || brightness<0.75 || brightness>1.5)
                throw new ArgumentException("亮度需要在 0.75–1.50 之间。");
        }
        static Dictionary<string,object> Read(string path)
        {
            if(String.IsNullOrWhiteSpace(path))throw new ArgumentException("缺少离线配置路径。");
            string full=Path.GetFullPath(path);
            if(!File.Exists(full))throw new FileNotFoundException("离线配置不存在。",full);
            return Object(Json.DeserializeObject(File.ReadAllText(full,Encoding.UTF8)),"manifest");
        }
        public static OfflineGraphicsPreference Load(string path)
        {
            var manifest=Read(path);object raw;
            if(!manifest.TryGetValue("Graphics",out raw))throw new InvalidDataException("离线配置缺少 Graphics。");
            var graphics=Object(raw,"Graphics");
            int style=Integer(graphics,"NeuralStyle"),passes=Integer(graphics,"NeuralPasses");
            double brightness=Number(graphics,"Brightness",1);
            bool sr=Boolean(graphics,"SuperResolution"),fg=Boolean(graphics,"FrameGeneration"),nr=Boolean(graphics,"NeuralRendering");
            if(sr!=fg)throw new InvalidDataException("超分和插帧必须由同一个独立开关控制。");
            if(!nr){style=-1;brightness=1;}
            Validate(style,brightness);
            if(passes!=2)throw new InvalidDataException("此配置没有启用固定的双 NR pass。");
            return new OfflineGraphicsPreference { NeuralStyle=style,NeuralPasses=passes,Brightness=brightness,SuperResolutionAndFrameGeneration=sr };
        }
        public static void Save(string path,int style,double brightness,bool superResolutionAndFrameGeneration)
        {
            Validate(style,brightness);string full=Path.GetFullPath(path);
            var manifest=Read(full);object raw;
            if(!manifest.TryGetValue("Graphics",out raw))throw new InvalidDataException("离线配置缺少 Graphics。");
            var graphics=Object(raw,"Graphics");
            if(Integer(graphics,"NeuralPasses")!=2)throw new InvalidDataException("此配置没有启用固定的双 NR pass。");
            if(!manifest.TryGetValue("Environment",out raw))throw new InvalidDataException("离线配置缺少 Environment。");
            var environment=Object(raw,"Environment");
            bool nrEnabled=style>=0;int storedStyle=nrEnabled?style:0;double storedBrightness=nrEnabled?brightness:1;
            graphics["SuperResolution"]=superResolutionAndFrameGeneration;graphics["FrameGeneration"]=superResolutionAndFrameGeneration;graphics["NeuralRendering"]=nrEnabled;
            graphics["NeuralStyle"]=storedStyle;graphics["NeuralPasses"]=2;graphics["Brightness"]=storedBrightness;
            environment["TWIN_KART_NR_STYLE"]=storedStyle.ToString(CultureInfo.InvariantCulture);
            environment["TWIN_KART_NR_PASSES"]="2";
            string temporary=full+"."+Guid.NewGuid().ToString("N")+".tmp";
            try {
                File.WriteAllText(temporary,Json.Serialize(manifest),new UTF8Encoding(false));
                try {File.Replace(temporary,full,null,true);}
                catch(IOException) {
                    // Some managed Windows volumes reject Replace without a
                    // backup name. Keep a same-directory rollback point and
                    // restore it if either rename fails.
                    string backup=full+"."+Guid.NewGuid().ToString("N")+".bak";
                    File.Move(full,backup);
                    try {File.Move(temporary,full);File.Delete(backup);}
                    catch {if(!File.Exists(full) && File.Exists(backup))File.Move(backup,full);throw;}
                }
            } finally {if(File.Exists(temporary))File.Delete(temporary);}
        }
    }
}
