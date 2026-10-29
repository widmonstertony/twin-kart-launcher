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
            internal bool CanRequest;
            public override string ToString() { return Id+" · "+(CanRequest?"客户端定义已找到":Status); }
        }
        public LocalRaceForm(string manifestPath,LocalRaceSettings current,Action<LocalRaceSettings> start)
        {
            Text="原厂 AI 比赛 · 本地主机候选"; Font=new Font("Microsoft YaHei UI",10); ClientSize=new Size(640,365);
            var json=new JavaScriptSerializer { MaxJsonLength=16*1024*1024 };
            var manifest=json.Deserialize<OfflineLaunchManifest>(File.ReadAllText(manifestPath));
            if(!manifest.EnableExperimentalRaceHost || String.IsNullOrEmpty(manifest.TrackReportPath)) throw new InvalidDataException("此离线配置没有启用本地主机候选或赛道定义报告。");
            var report=json.DeserializeObject(File.ReadAllText(manifest.TrackReportPath)) as Dictionary<string,object>;
            if(report==null || !Object.Equals(report["ClientSha256"],manifest.GameSha256) || !Object.Equals(report["NativeDefinitionsComplete"],true)) throw new InvalidDataException("赛道定义报告与客户端不匹配。");
            var tracks=new ComboBox { Left=20,Top=55,Width=600,DropDownStyle=ComboBoxStyle.DropDownList };
            int available=0;
            foreach(var entry in (object[])report["Tracks"]) {
                var row=(Dictionary<string,object>)entry; string status=row["NativeDefinitionStatus"] as string;
                bool can=status=="native-definition-present";
                var choice=new TrackChoice { Id=(string)row["TrackId"],Status=can?"":status=="no-native-definition-in-client"?"本客户端无此定义":"变体待适配",CanRequest=can };
                tracks.Items.Add(choice); if(can) ++available;
                if(choice.Id==current.Track) tracks.SelectedItem=choice;
            }
            if(tracks.SelectedIndex<0 && tracks.Items.Count>0) tracks.SelectedIndex=0;
            Controls.Add(new Label { Text="赛道："+available+" 个原生定义可请求；未匹配与变体仍保留列出",Left=20,Top=20,Width=600,Height=28 });
            var mode=new ComboBox { Left=20,Top=115,Width=180,DropDownStyle=ComboBoxStyle.DropDownList };
            mode.Items.AddRange(new object[]{"竞速赛","道具赛"}); mode.SelectedIndex=current.Mode==4?1:0;
            var ai=new NumericUpDown { Left=290,Top=115,Width=75,Minimum=1,Maximum=7,Value=current.AiCount };
            var laps=new NumericUpDown { Left=495,Top=115,Width=75,Minimum=1,Maximum=9,Value=current.Laps };
            Controls.Add(new Label { Text="AI 数量",Left=220,Top=119,Width=70 }); Controls.Add(new Label { Text="圈数",Left=430,Top=119,Width=60 });
            var statusText=new Label { Text="先在游戏里完成登录并停在大厅，再发起请求。\n此版本验证原厂本地主机入口；AI 行驶、道具交互与结算尚未验收。\n第一人称由相机模块提供，F6 切换。",Left=20,Top=165,Width=600,Height=105 };
            var launch=new Button { Text="发送本地比赛请求",Left=20,Top=295,Width=200,Height=38 };
            launch.Click+=delegate {
                var choice=tracks.SelectedItem as TrackChoice;
                if(choice==null || !choice.CanRequest) { statusText.Text="该赛道的本机定义或变体适配尚未完成，未发送请求。"; return; }
                var value=new LocalRaceSettings { Track=choice.Id,Mode=mode.SelectedIndex==1?4:1,AiCount=(int)ai.Value,Laps=(int)laps.Value };
                try { start(value); statusText.Text="请求已发送。原厂引擎是否成功开赛，以游戏画面和本轮日志为准。"; }
                catch(Exception error) { statusText.Text="请求未发送："+error.Message; }
            };
            Controls.AddRange(new Control[]{tracks,mode,ai,laps,statusText,launch});
        }
    }
}
