using System;
using System.Drawing;
using System.Windows.Forms;

namespace TwinKartLauncher
{
    internal sealed class GraphicsLaunchForm:Form
    {
        readonly string manifestPath;
        readonly ComboBox style=new ComboBox();
        readonly CheckBox acceleration=new CheckBox();
        readonly NumericUpDown brightness=new NumericUpDown();
        public GraphicsLaunchForm(string path,bool launching)
        {
            manifestPath=path;Text="启动画面设置";Font=new Font("Microsoft YaHei UI",10);
            ClientSize=new Size(510,320);FormBorderStyle=FormBorderStyle.FixedDialog;MaximizeBox=false;MinimizeBox=false;
            StartPosition=FormStartPosition.CenterParent;ShowInTaskbar=false;
            var title=new Label {Text="DLSS 5 画面模式",Font=new Font(Font.FontFamily,16,FontStyle.Bold),Left=20,Top=18,Width=460,Height=38};
            var styleLabel=new Label {Text="风格",Left=20,Top=76,Width=80,Height=28};
            style.SetBounds(105,72,365,32);style.DropDownStyle=ComboBoxStyle.DropDownList;
            style.Items.AddRange(new object[] {"Default（DLSS 5 默认风格）","Natural（自然，可能偏暗）","Movie（更鲜明、电影感）","关闭（仅关闭 DLSS 5 NR）"});
            acceleration.SetBounds(105,112,365,30);acceleration.Text="开启超分 + 插帧（独立开关）";
            var brightnessLabel=new Label {Text="原厂亮度",Left=20,Top=160,Width=80,Height=28};
            brightness.SetBounds(105,156,100,32);brightness.DecimalPlaces=2;brightness.Minimum=0.75m;brightness.Maximum=1.50m;brightness.Increment=0.05m;
            var hint=new Label {Text="风格“关闭”只停用 DLSS 5 NR；超分和插帧由上方开关控制。\n设置在下次启动时生效。",Left=220,Top=152,Width=270,Height=70};
            var cancel=new Button {Text="取消",Left=270,Top=258,Width=95,Height=36,DialogResult=DialogResult.Cancel};
            var apply=new Button {Text=launching?"应用并启动":"保存设置",Left=375,Top=258,Width=115,Height=36};
            apply.Click+=delegate {
                try {int[] modes={0,1,2,-1};OfflineGraphicsPreferenceStore.Save(manifestPath,modes[style.SelectedIndex],(double)brightness.Value,acceleration.Checked);DialogResult=DialogResult.OK;Close();}
                catch(Exception error){MessageBox.Show(this,error.Message,"画面设置保存失败",MessageBoxButtons.OK,MessageBoxIcon.Error);}
            };
            Controls.AddRange(new Control[]{title,styleLabel,style,acceleration,brightnessLabel,brightness,hint,cancel,apply});
            AcceptButton=apply;CancelButton=cancel;
            var current=OfflineGraphicsPreferenceStore.Load(path);
            style.SelectedIndex=current.NeuralStyle<0?3:current.NeuralStyle;brightness.Value=(decimal)current.Brightness;acceleration.Checked=current.SuperResolutionAndFrameGeneration;
            style.SelectedIndexChanged+=delegate {bool enabled=style.SelectedIndex!=3;brightness.Enabled=enabled;if(!enabled)brightness.Value=1.00m;};
            brightness.Enabled=style.SelectedIndex!=3;
        }
    }
}
