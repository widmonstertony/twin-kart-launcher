using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using TwinKartLauncher.Gateway;

namespace TwinKartLauncher
{
    internal sealed class OfflineGarageForm : Form
    {
        readonly JavaScriptSerializer json=new JavaScriptSerializer { MaxJsonLength=32*1024*1024,RecursionLimit=96 };
        readonly ComboBox vehicles=new ComboBox { DropDownStyle=ComboBoxStyle.DropDownList };
        readonly DataGridView grid=new DataGridView { AllowUserToAddRows=false,AllowUserToDeleteRows=false,RowHeadersVisible=false,AutoSizeColumnsMode=DataGridViewAutoSizeColumnsMode.Fill };
        readonly CheckBox all=new CheckBox { Text="对全部已有改装记录的车辆应用",AutoSize=true };
        readonly Label status=new Label();
        readonly string configPath, profileJson, rulesJson;
        readonly Dictionary<string,object> config,profile,rules;
        readonly List<string> fieldNames=new List<string>();
        static Dictionary<string,object> Map(object value) { return (Dictionary<string,object>)value; }
        internal OfflineGarageForm(string manifestPath)
        {
            var manifest=json.Deserialize<OfflineLaunchManifest>(File.ReadAllText(manifestPath));
            configPath=manifest.GatewayConfigPath; config=Map(json.DeserializeObject(File.ReadAllText(configPath)));
            string data=(string)config["ProfileDataDirectory"],save=(string)config["ProfilePath"];
            profileJson=File.ReadAllText(File.Exists(save)?save:Path.Combine(data,"initial-profile.private.json"));
            rulesJson=File.ReadAllText(Path.Combine(data,"gear-rules.private.json"));
            profile=Map(json.DeserializeObject(profileJson)); rules=Map(Map(json.DeserializeObject(rulesJson))["rules"]);
            Text="离线改装方案"; Font=new Font("Microsoft YaHei UI",10); ClientSize=new Size(690,650); StartPosition=FormStartPosition.CenterParent;
            var description=new Label { Left=18,Top=15,Width=650,Height=48,Text="选择车辆和属性档位，保存后由本项目后端在下次启动时应用。\n方案不受共享点数、车型属性上限和固定属性限制。" };
            vehicles.SetBounds(18,74,650,30); grid.SetBounds(18,118,650,414);
            grid.Columns.Add(new DataGridViewTextBoxColumn { Name="attribute",HeaderText="改装属性",ReadOnly=true });
            grid.Columns.Add(new DataGridViewComboBoxColumn { Name="level",HeaderText="档位",FlatStyle=FlatStyle.Flat });
            foreach(var rule in rules)
            {
                object compatible;
                if(Map(rule.Value).TryGetValue("clientCompatible",out compatible) && compatible is bool && !(bool)compatible) continue;
                var levels=(object[])Map(rule.Value)["values"];
                int row=grid.Rows.Add(); fieldNames.Add(rule.Key);
                object label; grid.Rows[row].Cells[0].Value=Map(rule.Value).TryGetValue("label",out label)?label:rule.Key;
                var selector=(DataGridViewComboBoxCell)grid.Rows[row].Cells[1]; foreach(object level in levels) selector.Items.Add(Convert.ToInt32(level));
            }
            foreach(string kart in Map(profile["ownKartsUpgradeInfo"]).Keys.OrderBy(x=>x,StringComparer.Ordinal)) vehicles.Items.Add(kart);
            vehicles.SelectedIndexChanged+=delegate { SelectVehicle(); };
            all.SetBounds(18,542,650,27);
            var savePlan=new Button { Left=18,Top=580,Width=150,Height=34,Text="保存自定义方案" };
            var maximum=new Button { Left=180,Top=580,Width=150,Height=34,Text="应用最高档方案" };
            status.SetBounds(18,620,650,28);
            savePlan.Click+=delegate { StorePlan(false); }; maximum.Click+=delegate { StorePlan(true); };
            Controls.AddRange(new Control[]{description,vehicles,grid,all,savePlan,maximum,status});
            string selected=(string)Map(Map(profile["itemPresetOn"])["itemMap"])["Kart"];
            vehicles.SelectedItem=selected; if(vehicles.SelectedIndex<0 && vehicles.Items.Count>0) vehicles.SelectedIndex=0;
        }
        void SelectVehicle()
        {
            if(vehicles.SelectedItem==null) return;
            var settings=Map(Map(Map(profile["ownKartsUpgradeInfo"])[(string)vehicles.SelectedItem])["gearInfo"]);
            var values=Map(settings["gearSettings"]);
            for(int i=0;i<fieldNames.Count;i++)
            {
                object current; int level=values.TryGetValue(fieldNames[i],out current)?Convert.ToInt32(current):0;
                var cell=(DataGridViewComboBoxCell)grid.Rows[i].Cells[1]; cell.Value=cell.Items.Contains(level)?level:cell.Items[0];
            }
        }
        void Atomic(string path,string text)
        { string temporary=path+".new"; File.WriteAllText(temporary,text); if(File.Exists(path)) File.Replace(temporary,path,path+".previous"); else File.Move(temporary,path); }
        void StorePlan(bool maximum)
        {
            try
            {
                if(vehicles.SelectedItem==null) throw new InvalidOperationException("没有可用车辆改装记录。");
                grid.EndEdit();
                var plan=new Dictionary<string,object> { {"karts",new[]{all.Checked?"*":(string)vehicles.SelectedItem}} };
                if(maximum) plan["maximum"]=true;
                else { var values=new Dictionary<string,int>(); for(int i=0;i<fieldNames.Count;i++) values.Add(fieldNames[i],Convert.ToInt32(grid.Rows[i].Cells[1].Value)); plan["settings"]=values; }
                string text=json.Serialize(plan);
                OfflineGarage.Apply(profileJson,rulesJson,text);
                string target=Path.Combine(Path.GetDirectoryName(configPath),"tuning-plan.private.json");
                Atomic(target,text); config["TuningPlanPath"]=target; Atomic(configPath,json.Serialize(config));
                status.Text="已保存；下次从本启动器启动时应用。";
            }
            catch(Exception problem) { status.Text="保存失败："+problem.Message; }
        }
    }
}
