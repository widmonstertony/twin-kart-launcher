using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Windows.Forms;
using System.Web.Script.Serialization;

namespace TwinKartLauncher
{
    internal sealed class LocalRaceForm : Form
    {
        sealed class TrackChoice {
            internal string Id,Status;
            internal bool CircuitRequest,FlagRequest;
            internal int? NativeModeMask;
            internal bool Supports(int mode) { return (LocalRaceSettings.FlagMode(mode)?FlagRequest:CircuitRequest) && NativeModeMask.HasValue && LocalRaceSettings.TrackSupports(NativeModeMask.Value,mode); }
            public override string ToString() { return Id+" · "+(CircuitRequest?"原生赛道与 AI 路径":FlagRequest?"原生夺旗地图":Status); }
        }
        public LocalRaceForm(string manifestPath,LocalRaceSettings current,Action<LocalRaceSettings> start)
        {
            Text="开发调试：本地比赛请求"; Font=new Font("Microsoft YaHei UI",10); ClientSize=new Size(640,365);
            var json=new JavaScriptSerializer { MaxJsonLength=16*1024*1024 };
            var manifest=json.Deserialize<OfflineLaunchManifest>(File.ReadAllText(manifestPath));
            if(!manifest.EnableExperimentalRaceHost || String.IsNullOrEmpty(manifest.TrackReportPath)) throw new InvalidDataException("此离线配置没有启用本地主机候选或赛道定义报告。");
            string reportPath=OfflineLaunchSession.ActiveTrackReportPath??manifest.TrackReportPath;
            var report=json.DeserializeObject(File.ReadAllText(reportPath)) as Dictionary<string,object>;
            if(report==null || !Object.Equals(report["ClientSha256"],manifest.GameSha256) || !Object.Equals(report["NativeDefinitionsComplete"],true)) throw new InvalidDataException("赛道定义报告与客户端不匹配。");
            var tracks=new ComboBox { Left=20,Top=55,Width=600,DropDownStyle=ComboBoxStyle.DropDownList };
            int circuits=0,flags=0;
            foreach(var entry in (object[])report["Tracks"]) {
                var row=(Dictionary<string,object>)entry; string status=row["NativeDefinitionStatus"] as string;
                bool defined=status=="native-definition-present";
                bool world=defined && row["WorldMatch"] as string=="exact-base-world-record";
                bool circuit=world && row["AiPathMatch"] as string=="exact-base-ai-record";
                var choice=new TrackChoice { Id=(string)row["TrackId"],Status=circuit?"":defined?"地图或 AI 路径资源待核对":status=="no-native-definition-in-client"?"本客户端无此定义":"变体待适配",CircuitRequest=circuit };
                object mask;
                if(row.TryGetValue("NativeModeMask",out mask) && mask!=null) {
                    if(!(mask is int) || (int)mask<0 || (int)mask>31) throw new InvalidDataException("赛道模式记录无效。");
                    choice.NativeModeMask=(int)mask;
                    choice.FlagRequest=world && (((int)mask&16)!=0);
                }
                tracks.Items.Add(choice); if(circuit) ++circuits; if(choice.FlagRequest)++flags;
                if(choice.Id==current.Track) tracks.SelectedItem=choice;
            }
            if(tracks.SelectedIndex<0 && tracks.Items.Count>0) tracks.SelectedIndex=0;
            Controls.Add(new Label { Text="本机可用："+circuits+" 条原生赛道、"+flags+" 张原生夺旗地图；缺失历史资源仅列出不伪装",Left=20,Top=20,Width=600,Height=28 });
            var mode=new ComboBox { Left=20,Top=115,Width=180,DropDownStyle=ComboBoxStyle.DropDownList };
            int[] modeIds={1,2,3,4,5,6,104,105,106,107,101,102,103};
            mode.Items.AddRange(new object[]{"个人竞速","双人组竞速","四人组竞速","个人道具","双人组道具","四人组道具","夺旗赛（四人组）","个人混合赛（实验）","双人混合赛（实验）","四人混合赛（实验）","个人无限加速（实验）","双人无限加速（实验）","四人无限加速（实验）"});
            mode.SelectedIndex=Math.Max(0,Array.IndexOf(modeIds,current.Mode));
            var ai=new NumericUpDown { Left=290,Top=115,Width=75,Minimum=1,Maximum=7,Value=current.AiCount };
            var laps=new NumericUpDown { Left=495,Top=115,Width=75,Minimum=1,Maximum=9,Value=current.Laps };
            Controls.Add(new Label { Text="AI 数量",Left=220,Top=119,Width=70 }); Controls.Add(new Label { Text="圈数",Left=430,Top=119,Width=60 });
            var statusText=new Label { Text="正常游玩请使用游戏大厅右下角的开始按钮。\n选图、模式、添加 AI 优先使用游戏内的自定义房间。\n此窗口仅用于开发诊断，需要已登录并停在大厅。\n双人组需要 3、5 或 7 个 AI；四人组需要 7 个 AI。F6 切换第一人称。",Left=20,Top=165,Width=600,Height=120 };
            var launch=new Button { Text="在所选赛道开始比赛",Left=20,Top=295,Width=200,Height=38 };
            launch.Click+=delegate {
                var choice=tracks.SelectedItem as TrackChoice;
                int selectedMode=modeIds[mode.SelectedIndex];
                if(choice==null || !choice.Supports(selectedMode)) { statusText.Text="该地图在本客户端不支持所选模式；缺失资源不会被静默替换成别的赛道。"; return; }
                if(!LocalRaceSettings.ValidPopulation(selectedMode,(int)ai.Value)) { statusText.Text="双人组需要 3、5 或 7 个 AI；四人组需要 7 个 AI，保证每队人数完整。"; return; }
                var value=new LocalRaceSettings { Track=choice.Id,Mode=selectedMode,AiCount=(int)ai.Value,Laps=(int)laps.Value };
                try { start(value); statusText.Text="请求已发送。原厂引擎是否成功开赛，以游戏画面和本轮日志为准。"; }
                catch(Exception error) { statusText.Text="请求未发送："+error.Message; }
            };
            Controls.AddRange(new Control[]{tracks,mode,ai,laps,statusText,launch});
        }
    }
}
