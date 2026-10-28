using System;
using System.Drawing;
using System.Windows.Forms;

namespace TwinKartLauncher
{
    internal sealed class CockpitForm : Form
    {
        readonly CheckBox enabled=new CheckBox(), head=new CheckBox();
        readonly NumericUpDown seatForward,seatHeight,eyeForward,eyeHeight,pitch,fov;
        readonly Label feedback=new Label();
        NumericUpDown Number(string text,int top,int value,int minimum,int maximum)
        {
            var label=new Label { Text=text,Left=20,Top=top+4,Width=265,Height=27 };
            var number=new NumericUpDown { Left=300,Top=top,Width=130,Minimum=minimum,Maximum=maximum,Value=value };
            Controls.Add(label); Controls.Add(number); return number;
        }
        public CockpitForm(CockpitSettings current,Action<CockpitSettings> apply)
        {
            Text="第一人称 · 原厂双手与方向盘"; Font=new Font("Microsoft YaHei UI",10);
            ClientSize=new Size(460,465); FormBorderStyle=FormBorderStyle.FixedDialog;
            StartPosition=FormStartPosition.CenterParent; MinimizeBox=MaximizeBox=false;
            enabled.SetBounds(20,18,410,28); enabled.Text="使用第一人称（F6 可切换第三人称）"; enabled.Checked=current.Enabled;
            head.SetBounds(20,52,410,28); head.Text="跟随驾驶员头部，适应角色与车辆"; head.Checked=current.FollowHead;
            Controls.Add(enabled); Controls.Add(head);
            eyeForward=Number("眼睛前后位置（厘米）",94,current.EyeForward,-50,80);
            eyeHeight=Number("眼睛高度偏移（厘米）",133,current.EyeHeight,-30,80);
            pitch=Number("视线俯仰（度）",172,current.Pitch,-40,30);
            fov=Number("视野 FOV（度）",211,current.FieldOfView,60,120);
            seatForward=Number("备用座位前后位置（厘米）",250,current.SeatForward,-200,300);
            seatHeight=Number("备用座位高度（厘米）",289,current.SeatHeight,20,250);
            feedback.SetBounds(20,335,410,65); feedback.Text="在赛道内生效；可以调整位置和视野后应用。\n更换角色后会重新识别驾驶员头部。"; Controls.Add(feedback);
            var save=new Button { Text="保存并应用",Left=180,Top=415,Width=125,Height=32 };
            var close=new Button { Text="关闭",Left=315,Top=415,Width=115,Height=32 };
            save.Click+=delegate {
                try {
                    apply(new CockpitSettings { Enabled=enabled.Checked,FollowHead=head.Checked,EyeForward=(int)eyeForward.Value,EyeHeight=(int)eyeHeight.Value,
                        Pitch=(int)pitch.Value,FieldOfView=(int)fov.Value,SeatForward=(int)seatForward.Value,SeatHeight=(int)seatHeight.Value });
                    feedback.Text="设置已保存；本项目正在运行的相机会读取这些设置。\n未启动游戏时，下次启动生效。";
                } catch(Exception error) { feedback.Text="保存失败："+error.Message; }
            };
            close.Click+=delegate { Close(); }; Controls.Add(save); Controls.Add(close);
        }
    }
}
