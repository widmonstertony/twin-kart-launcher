using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using System.Security.Cryptography;

namespace TwinKartLauncher
{
    // Requested behavior only. Enabling these options never sets an acceptance
    // or capability flag: the native adapter checks actual resources/results.
    public sealed class OfflineGraphicsOptions
    {
        public bool SuperResolution { get; set; }
        public bool FrameGeneration { get; set; }
        public bool NeuralRendering { get; set; }
        public int NeuralPasses { get; set; }
        public int NeuralStyle { get; set; }
        public double RenderPercentage { get; set; }
        public double Brightness { get; set; }
        public OfflineGraphicsOptions() { RenderPercentage=50; NeuralPasses=2; NeuralStyle=1; Brightness=1; }
        public bool NeedsRenderer { get { return SuperResolution || FrameGeneration || NeuralRendering; } }
        public void Validate(IDictionary<string,string> environment)
        {
            if(NeuralPasses<1 || NeuralPasses>2 || NeuralStyle<0 || NeuralStyle>2)
                throw new ArgumentException("NR requires 1 or 2 passes and style 0 (default), 1 (natural), or 2 (cinematic/Movie).");
            if(Double.IsNaN(RenderPercentage) || Double.IsInfinity(RenderPercentage) || RenderPercentage<25 || RenderPercentage>100)
                throw new ArgumentException("Render percentage must be between 25 and 100.");
            if(Double.IsNaN(Brightness) || Double.IsInfinity(Brightness) || Brightness<0.75 || Brightness>1.5)
                throw new ArgumentException("Native brightness must be between 0.75 and 1.50.");
            if(SuperResolution && RenderPercentage>=100)throw new ArgumentException("Super resolution requires a lower native rendering percentage.");
            // Temporal SR and frame generation have their own native guide
            // adapters. DLSS 5 NR can therefore be disabled independently.
            if(NeuralRendering) {
                Required(environment,"TWIN_KART_NEURAL_MODULE");Required(environment,"TWIN_KART_NEURAL_PROVIDER");
                Required(environment,"TWIN_KART_NEURAL_MODULE_SHA256");
                if(!Regex.IsMatch(environment["TWIN_KART_NEURAL_MODULE_SHA256"],"\\A[0-9a-f]{64}\\z",RegexOptions.CultureInvariant))
                    throw new ArgumentException("Expected the SHA-256 of the independently built NR consumer.");
            }
            if(FrameGeneration)Required(environment,"TWIN_KART_FG_PROVIDER");
            if(SuperResolution) {
                Required(environment,"TWIN_KART_DLSS_MODULE");Required(environment,"TWIN_KART_DLSS_PROVIDER");
                Required(environment,"TWIN_KART_DLSS_MODULE_SHA256");
                if(!Regex.IsMatch(environment["TWIN_KART_DLSS_MODULE_SHA256"],"\\A[0-9a-f]{64}\\z",RegexOptions.CultureInvariant))
                    throw new ArgumentException("Expected the SHA-256 of the independently built temporal SR module.");
            }
        }
        static void Required(IDictionary<string,string> environment,string key)
        {
            string value;
            if(environment==null || !environment.TryGetValue(key,out value) || String.IsNullOrWhiteSpace(value))
                throw new ArgumentException("Missing external graphics dependency: "+key);
        }
        public void ValidateFiles(IDictionary<string,string> environment)
        {
            Validate(environment);
            if(SuperResolution) {
                string actual;
                using(var file=File.OpenRead(environment["TWIN_KART_DLSS_MODULE"]))
                using(var hash=SHA256.Create())actual=BitConverter.ToString(hash.ComputeHash(file)).Replace("-","").ToLowerInvariant();
                if(actual!=environment["TWIN_KART_DLSS_MODULE_SHA256"])throw new InvalidDataException("Temporal SR module differs from the private manifest.");
                if(!Directory.Exists(environment["TWIN_KART_DLSS_PROVIDER"]))throw new DirectoryNotFoundException("External SR provider directory unavailable.");
            }
            if(NeuralRendering) {
                if(!File.Exists(environment["TWIN_KART_NEURAL_MODULE"]))throw new FileNotFoundException("Original NR consumer unavailable.");
                string actual;
                using(var file=File.OpenRead(environment["TWIN_KART_NEURAL_MODULE"]))
                using(var hash=SHA256.Create())actual=BitConverter.ToString(hash.ComputeHash(file)).Replace("-","").ToLowerInvariant();
                if(actual!=environment["TWIN_KART_NEURAL_MODULE_SHA256"])throw new InvalidDataException("NR consumer differs from the private manifest.");
                if(!Directory.Exists(environment["TWIN_KART_NEURAL_PROVIDER"]))throw new DirectoryNotFoundException("External NR provider directory unavailable.");
            }
            if(FrameGeneration)foreach(string file in new[]{"sl.interposer.dll","sl.dlss_g.dll","sl.common.dll","sl.reflex.dll","sl.pcl.dll"})
                if(!File.Exists(Path.Combine(environment["TWIN_KART_FG_PROVIDER"],file)))throw new FileNotFoundException("External frame generation dependency unavailable: "+file);
        }
        public void ApplyEnvironment(IDictionary<string,string> environment,string runDirectory)
        {
            Validate(environment);
            environment["TWIN_KART_SUPER_RESOLUTION"]=SuperResolution?"1":"0";
            environment["TWIN_KART_RENDER_PERCENTAGE"]=SuperResolution?RenderPercentage.ToString("0.######",CultureInfo.InvariantCulture):"";
            environment["TWIN_KART_FRAME_GENERATION"]=FrameGeneration?"1":"0";
            environment["TWIN_KART_NR_PASSES"]=NeuralPasses.ToString(CultureInfo.InvariantCulture);
            environment["TWIN_KART_NR_STYLE"]=NeuralStyle.ToString(CultureInfo.InvariantCulture);
            if(!NeuralRendering)environment["TWIN_KART_NEURAL_MODULE"]="";
            if(FrameGeneration) {
                string logs=Path.Combine(runDirectory,"streamline-log");Directory.CreateDirectory(logs);
                environment["TWIN_KART_FG_LOG_DIRECTORY"]=logs;
            }
        }
        static string Field(string value,string key,string data)
        {
            if(value.Length<2 || value[0]!='(' || value[value.Length-1]!=')')throw new InvalidDataException("Malformed native graphics settings.");
            var matches=Regex.Matches(value,"(?<=\\(|,)"+Regex.Escape(key)+"=[^,)]*",RegexOptions.CultureInvariant);
            if(matches.Count>1)throw new InvalidDataException("Ambiguous native rendering percentage.");
            string field=key+"="+data;
            return matches.Count==1?value.Substring(0,matches[0].Index)+field+value.Substring(matches[0].Index+matches[0].Length):
                "("+field+(value.Length==2?"":",")+value.Substring(1);
        }
        string Settings(string value)
        {
            if(SuperResolution)value=Field(value,"ScreenPercentage",RenderPercentage.ToString("0.######",CultureInfo.InvariantCulture));
            if(Math.Abs(Brightness-1)>0.000001)value=Field(value,"Brightness",Brightness.ToString("0.######",CultureInfo.InvariantCulture));
            // Observe original engine frame IDs before the native NVAPI
            // enable checks, while Streamline owns pacing and PCL delivery.
            // Keep native NVAPI pacing disabled to avoid two sleep providers.
            if(FrameGeneration)value=Field(value,"ReflexMode","0");
            return value;
        }
        public byte[] RouteUserSettings(byte[] original)
        {
            if(!SuperResolution && !FrameGeneration && Math.Abs(Brightness-1)<=0.000001)return original;
            if(SuperResolution && (Double.IsNaN(RenderPercentage) || Double.IsInfinity(RenderPercentage) || RenderPercentage<25 || RenderPercentage>=100))
                throw new ArgumentException("Invalid lower rendering percentage.");
            string text=new UTF8Encoding(false,true).GetString(original??new byte[0]).TrimStart('\ufeff');
            const string section="[/Script/KartDrift.KartGameUserSettings]";
            var output=new List<string>();bool inside=false,found=false,graphic=false,last=false,dynamic=false;
            Action append=()=>{
                if(!graphic)output.Add("GraphicSetting="+Settings("()"));
                if(!last)output.Add("LastGraphicSetting="+Settings("()"));
                if(!dynamic && SuperResolution)output.Add("bUseDynamicResolution=False");
            };
            foreach(string line in text.Replace("\r\n","\n").Split('\n')) {
                string trimmed=line.Trim();
                if(trimmed.StartsWith("[",StringComparison.Ordinal) && trimmed.EndsWith("]",StringComparison.Ordinal)) {
                    if(inside)append();inside=String.Equals(trimmed,section,StringComparison.OrdinalIgnoreCase);
                    if(inside) { if(found)throw new InvalidDataException("Duplicate native user settings section.");found=true; }
                }
                if(inside) {
                    int equal=trimmed.IndexOf('=');string key=equal<0?trimmed:trimmed.Substring(0,equal);
                    if(key=="GraphicSetting" || key=="LastGraphicSetting") {
                        bool duplicate=key=="GraphicSetting"?graphic:last;
                        if(duplicate || equal<0)throw new InvalidDataException("Duplicate native graphics structure.");
                        if(key=="GraphicSetting")graphic=true;else last=true;
                        output.Add(key+"="+Settings(trimmed.Substring(equal+1)));continue;
                    }
                    if(key=="bUseDynamicResolution") {
                        if(dynamic)throw new InvalidDataException("Duplicate dynamic resolution setting.");
                        dynamic=true;output.Add(SuperResolution?"bUseDynamicResolution=False":line);continue;
                    }
                }
                output.Add(line);
            }
            if(!found){output.Add(section);inside=true;}
            if(inside)append();
            return new UTF8Encoding(false).GetBytes(String.Join("\r\n",output));
        }
        public byte[] RouteEngineSettings(byte[] original)
        {
            if(!SuperResolution)return original;
            if(Double.IsNaN(RenderPercentage) || Double.IsInfinity(RenderPercentage) || RenderPercentage<25 || RenderPercentage>=100)
                throw new ArgumentException("Invalid lower rendering percentage.");
            // The native user-settings structure can be replaced when the
            // account's graphics options arrive. Use the engine's own cvar
            // configuration as well; the render adapter still checks actual
            // input/output dimensions and never infers SR from this request.
            string text=new UTF8Encoding(false,true).GetString(original??new byte[0]).TrimStart('\ufeff');
            var output=new List<string>();bool inside=false,found=false,written=false;
            string value="r.ScreenPercentage="+RenderPercentage.ToString("0.######",CultureInfo.InvariantCulture);
            foreach(string line in text.Replace("\r\n","\n").Split('\n')) {
                string trimmed=line.Trim();
                if(trimmed.StartsWith("[",StringComparison.Ordinal) && trimmed.EndsWith("]",StringComparison.Ordinal)) {
                    if(inside && !written){output.Add(value);written=true;}
                    inside=String.Equals(trimmed,"[SystemSettings]",StringComparison.OrdinalIgnoreCase);
                    if(inside){if(found)throw new InvalidDataException("Duplicate engine system settings section.");found=true;}
                }
                int equal=trimmed.IndexOf('=');
                if(inside && equal>=0 && String.Equals(trimmed.Substring(0,equal).Trim(),"r.ScreenPercentage",StringComparison.OrdinalIgnoreCase)) {
                    if(written)throw new InvalidDataException("Duplicate engine rendering percentage.");
                    output.Add(value);written=true;
                } else output.Add(line);
            }
            if(!found)output.Add("[SystemSettings]");
            if(!written)output.Add(value);
            return new UTF8Encoding(false).GetBytes(String.Join("\r\n",output));
        }
    }
}
