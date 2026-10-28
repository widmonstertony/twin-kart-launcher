using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using CancellationTokenSource = System.Threading.CancellationTokenSource;
using System.Windows.Forms;

namespace TwinKartLauncher
{
    internal sealed class AxisChoice
    {
        public DeviceInfo Device;
        public AxisInfo Axis;
        public override string ToString() { return Device.InstanceName + " · " + Axis.Name; }
    }

    internal sealed class AxisPanel : GroupBox
    {
        readonly bool steering;
        readonly ComboBox choices = new ComboBox();
        readonly Label live = new Label();
        readonly Label calibration = new Label();
        readonly ProgressBar bar = new ProgressBar();
        readonly Button[] capture;
        readonly int?[] samples;
        AxisBinding binding = new AxisBinding();
        int? raw;
        public AxisBinding Binding { get { return binding; } }
        public event Action Changed;
        public AxisPanel(string name, bool steering)
        {
            this.steering = steering;
            Text = name; Width = 294; Height = 252; Margin = new Padding(5);
            choices.SetBounds(12, 28, 268, 26); choices.DropDownStyle = ComboBoxStyle.DropDownList;
            live.SetBounds(12, 65, 268, 26); bar.SetBounds(12, 95, 268, 20); bar.Maximum = 1000;
            calibration.SetBounds(12, 175, 268, 65);
            samples = new int?[steering ? 3 : 2];
            capture = new Button[samples.Length];
            string[] names = steering ? new[] { "记录左端", "记录正中", "记录右端" } : new[] { "记录松开", "记录踩到底" };
            for (int i = 0; i < capture.Length; i++)
            {
                int slot = i;
                var button = new Button { Text = names[i], Width = steering ? 86 : 130, Height = 32, Left = 12 + i * (steering ? 90 : 138), Top = 130 };
                button.Click += delegate { CaptureSample(slot); }; capture[i] = button; Controls.Add(button);
            }
            choices.SelectedIndexChanged += delegate {
                var choice = choices.SelectedItem as AxisChoice;
                raw = null;
                if (choice != null && (binding.DeviceGuid != choice.Device.InstanceGuid.ToString() || binding.Offset != choice.Axis.Offset))
                {
                    binding = new AxisBinding { DeviceGuid = choice.Device.InstanceGuid.ToString(), Offset = choice.Axis.Offset };
                    for (int i = 0; i < samples.Length; i++) samples[i] = null;
                    Notify();
                }
                UpdateText();
            };
            Controls.AddRange(new Control[] { choices, live, bar, calibration }); UpdateText();
        }
        public void SetBinding(AxisBinding value) {
            binding = value ?? new AxisBinding();
            samples[0] = binding.Calibrated ? (int?)binding.NegativeOrReleased : null;
            if (steering) samples[1] = binding.Calibrated ? (int?)binding.Center : null;
            samples[samples.Length - 1] = binding.Calibrated ? (int?)binding.PositiveOrPressed : null;
            SelectBinding(); UpdateText();
        }
        public void SetChoices(IEnumerable<DeviceInfo> devices)
        {
            choices.Items.Clear();
            foreach (var device in devices) foreach (var axis in device.Axes) choices.Items.Add(new AxisChoice { Device = device, Axis = axis });
            SelectBinding();
        }
        void SelectBinding()
        {
            choices.SelectedIndex = -1;
            for (int i = 0; i < choices.Items.Count; i++) {
                var choice = (AxisChoice)choices.Items[i];
                if (choice.Device.InstanceGuid.ToString() == binding.DeviceGuid && choice.Axis.Offset == binding.Offset) { choices.SelectedIndex = i; break; }
            }
        }
        public bool UpdateSample(IDictionary<string, InputSnapshot> snapshots, out double normalized)
        {
            normalized = 0; raw = null;
            var choice = choices.SelectedItem as AxisChoice;
            InputSnapshot snapshot;
            if (choice != null && snapshots.TryGetValue(choice.Device.InstanceGuid.ToString(), out snapshot)) raw = snapshot.GetAxis(choice.Axis);
            if (raw.HasValue) normalized = binding.Normalize(raw.Value, steering);
            live.Text = raw.HasValue ? "原始值 " + raw.Value + "  ·  " + normalized.ToString("P0") : "等待设备／选择轴";
            bar.Value = raw.HasValue ? (int)Math.Round((steering ? (normalized + 1) / 2 : normalized) * 1000) : 0;
            foreach (var button in capture) button.Enabled = raw.HasValue;
            return raw.HasValue && binding.IsValid(steering);
        }
        void CaptureSample(int index)
        {
            if (!raw.HasValue) return;
            samples[index] = raw;
            binding.NegativeOrReleased = samples[0] ?? 0;
            binding.Center = steering ? samples[1] ?? 0 : 0;
            binding.PositiveOrPressed = samples[samples.Length - 1] ?? 0;
            binding.Calibrated = samples.All(x => x.HasValue);
            Notify(); UpdateText();
        }
        void Notify() { if (Changed != null) Changed(); }
        void UpdateText()
        {
            calibration.Text = String.Join(" / ", samples.Select(x => x.HasValue ? x.Value.ToString() : "未记录")) + "\n" +
                (binding.IsValid(steering) ? "校准有效 · 死区 3%" : "请分别记录各位置，再保存配置");
        }
    }

    public sealed class MainForm : Form
    {
        readonly ProfileStore store = new ProfileStore(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "profiles"));
        readonly ComboBox editions = new ComboBox();
        readonly TextBox client = new TextBox();
        readonly Label clientStatus = new Label();
        readonly Label status = new Label();
        readonly Label devicesText = new Label();
        readonly Label buttonsText = new Label();
        readonly AxisPanel steering = new AxisPanel("方向盘：左端 → 正中 → 右端", true);
        readonly AxisPanel throttle = new AxisPanel("油门：松开 → 踩到底", false);
        readonly AxisPanel brake = new AxisPanel("刹车：松开 → 踩到底", false);
        readonly CheckBox preview = new CheckBox();
        readonly Timer timer = new Timer();
        readonly Dictionary<string, DeviceInput> inputs = new Dictionary<string, DeviceInput>();
        readonly Dictionary<string, InputSnapshot> snapshots = new Dictionary<string, InputSnapshot>();
        readonly SharedPad pad = new SharedPad();
        LauncherProfile profile;
        bool loading;
        bool dirty;
        bool closed;
        CancellationTokenSource gameSession;
        public MainForm()
        {
            Text = "Twin Kart Launcher · 原厂客户端离线适配";
            Font = new Font("Microsoft YaHei UI", 10); ClientSize = new Size(936, 740);
            MinimumSize = new Size(952, 779); StartPosition = FormStartPosition.CenterScreen;
            var title = new Label { Text = "跑跑卡丁车  /  双版本启动器", Font = new Font(Font.FontFamily, 21, FontStyle.Bold), Left = 20, Top = 18, Width = 890, Height = 44 };
            editions.SetBounds(20, 76, 280, 32); editions.DropDownStyle = ComboBoxStyle.DropDownList;
            editions.Items.AddRange(new[] { "2004 原版《跑跑卡丁车》", "《跑跑卡丁车：漂移》" });
            var editionNote = new Label { Text = "独立客户端路径、设备配置及验证记录", Left = 320, Top = 81, Width = 580, Height = 26 };
            client.SetBounds(20, 122, 690, 28);
            var browse = new Button { Text = "选择客户端", Left = 720, Top = 120, Width = 95, Height = 32 };
            var verify = new Button { Text = "核对文件", Left = 821, Top = 120, Width = 95, Height = 32 };
            clientStatus.SetBounds(20, 160, 896, 48);
            var refresh = new Button { Text = "刷新方向盘和踏板", Left = 20, Top = 215, Width = 175, Height = 32 };
            devicesText.SetBounds(210, 218, 706, 48);
            var axes = new FlowLayoutPanel { Left = 15, Top = 270, Width = 910, Height = 265, WrapContents = false };
            axes.Controls.AddRange(new Control[] { steering, throttle, brake });
            var save = new Button { Text = "保存校准", Left = 20, Top = 548, Width = 115, Height = 34 };
            preview.SetBounds(155, 552, 380, 30); preview.Text = "开启会话输入桥（当前仅测试通道）";
            var bindDrift = new Button { Text = "绑定漂移", Left = 550, Top = 548, Width = 110, Height = 34 };
            var bindItem = new Button { Text = "绑定道具", Left = 670, Top = 548, Width = 110, Height = 34 };
            buttonsText.SetBounds(20, 590, 896, 28);
            status.SetBounds(20, 624, 896, 38);
            var configure = new Button { Text = "离线配置", Left = 20, Top = 677, Width = 115, Height = 34 };
            var startGame = new Button { Text = "启动原厂游戏", Left = 145, Top = 677, Width = 155, Height = 34 };
            var tuning = new Button { Text = "改装方案", Left = 310, Top = 677, Width = 115, Height = 34 };
            var pending = new Label { Left = 440, Top = 670, Width = 476, Height = 62, ForeColor = Color.FromArgb(130, 65, 10), Text = "使用本项目后端；外部游戏资源另行提供。\nAI同场、DLSS及全部赛道仍在适配验证中。" };
            Controls.AddRange(new Control[] { title, editions, editionNote, client, browse, verify, clientStatus, refresh, devicesText, axes, save, preview, bindDrift, bindItem, buttonsText, status, configure, startGame, tuning, pending });
            tuning.Click += delegate {
                try { if(String.IsNullOrEmpty(profile.OfflineManifestPath)) { status.Text="请先选择本机离线配置。"; return; } using(var garage=new OfflineGarageForm(profile.OfflineManifestPath)) garage.ShowDialog(this); }
                catch(Exception problem) { status.Text="改装配置读取失败："+problem.Message; }
            };
            configure.Click += delegate {
                using(var picker=new OpenFileDialog { Filter="离线会话配置 (*.local.json)|*.local.json",CheckFileExists=true })
                    if(picker.ShowDialog(this)==DialogResult.OK) { profile.OfflineManifestPath=picker.FileName; dirty=true; SaveProfile(); }
            };
            startGame.Click += async delegate {
                if(gameSession!=null) { gameSession.Cancel(); return; }
                if(profile.Edition!=GameEdition.Drift) { status.Text="经典版尚缺完整客户端，本入口暂未完成。"; return; }
                if(String.IsNullOrEmpty(profile.OfflineManifestPath)) { status.Text="请先选择为本机准备的离线会话配置。"; return; }
                string manifest=profile.OfflineManifestPath;
                gameSession=new CancellationTokenSource(); startGame.Text="结束并恢复配置"; editions.Enabled=false; configure.Enabled=false;
                try {
                    Action<string> update=message=>{ if(!closed && IsHandleCreated) BeginInvoke((Action)(()=>status.Text=message)); };
                    await Task.Run(()=>OfflineLaunchSession.Run(manifest,update,gameSession.Token));
                } catch(OperationCanceledException) { status.Text="会话已结束，配置已恢复。"; }
                catch(Exception problem) { status.Text="启动失败："+problem.Message; }
                finally { gameSession.Dispose(); gameSession=null; startGame.Text="启动原厂游戏"; editions.Enabled=true; configure.Enabled=true; }
            };
            editions.SelectedIndexChanged += delegate { SwitchEdition(); };
            client.TextChanged += delegate { if (!loading) { dirty = true; profile.ClientPath = client.Text; profile.FingerprintSha256 = ""; clientStatus.Text = "路径已变更，尚未核对文件"; } };
            browse.Click += delegate { using (var picker = new OpenFileDialog { Filter = "游戏客户端 (*.exe)|*.exe", CheckFileExists = true }) { if (picker.ShowDialog(this) == DialogResult.OK) client.Text = picker.FileName; } };
            verify.Click += async delegate { await VerifyClient(verify); };
            refresh.Click += delegate { RefreshDevices(); };
            save.Click += delegate { SaveProfile(); };
            bindDrift.Click += delegate { BindButton(true); };
            bindItem.Click += delegate { BindButton(false); };
            steering.Changed += MarkDirty; throttle.Changed += MarkDirty; brake.Changed += MarkDirty;
            preview.CheckedChanged += delegate { if (!preview.Checked) pad.Write(false, 0, 0, 0, 0); };
            timer.Interval = 16; timer.Tick += delegate { Poll(); };
            Shown += delegate { RefreshDevices(); timer.Start(); };
            FormClosing += delegate(object sender, FormClosingEventArgs e) {
                if(gameSession!=null) { gameSession.Cancel(); e.Cancel=true; status.Text="正在退出本轮游戏并恢复配置，完成后可关闭。"; return; }
                if (dirty && !SaveProfile()) { e.Cancel = true; return; }
                closed = true; timer.Stop(); foreach (var input in inputs.Values) input.Dispose(); inputs.Clear(); pad.Dispose();
            };
            editions.SelectedIndex = 1;
        }
        void MarkDirty() { if (!loading) dirty = true; }
        void SwitchEdition()
        {
            if (loading) return;
            if (profile != null && dirty && !SaveProfile()) {
                loading = true;
                try { editions.SelectedIndex = (int)profile.Edition; } finally { loading = false; }
                return;
            }
            preview.Checked = false;
            loading = true;
            try {
                var edition = editions.SelectedIndex == 0 ? GameEdition.Classic : GameEdition.Drift;
                try { profile = store.Load(edition); }
                catch (Exception e) { profile = new LauncherProfile { Edition = edition }; status.Text = "配置读取失败：" + e.Message; }
                if (edition == GameEdition.Drift && String.IsNullOrWhiteSpace(profile.ClientPath)) {
                    try { profile.ClientPath = ClientDiscovery.FindDrift(); }
                    catch (Exception e) { status.Text = "自动查找失败，可手动选择客户端：" + e.Message; }
                }
                client.Text = profile.ClientPath;
                clientStatus.Text = String.IsNullOrEmpty(profile.FingerprintSha256) ? "尚未核对客户端文件" : "上次记录 SHA-256：" + profile.FingerprintSha256 + "\n此记录不代表资源完整或已经可以离线比赛。";
                steering.SetBinding(profile.Steering); throttle.SetBinding(profile.Throttle); brake.SetBinding(profile.Brake);
                UpdateButtons(); dirty = false;
            } finally { loading = false; }
        }
        bool SaveProfile()
        {
            if (profile == null) return true;
            profile.Steering = steering.Binding; profile.Throttle = throttle.Binding; profile.Brake = brake.Binding;
            try { store.Save(profile); dirty = false; status.Text = "配置已保存"; return true; }
            catch (Exception e) { status.Text = "保存失败，保留当前编辑：" + e.Message; return false; }
        }
        async Task VerifyClient(Button button)
        {
            string path = client.Text; var target = profile;
            button.Enabled = false;
            try {
                ClientInspection inspection = await Task.Run(() => ClientDiscovery.Inspect(path));
                if (!closed && profile == target && client.Text == path) {
                    if (target.Edition == GameEdition.Classic && inspection.KnownBuild.StartsWith("Drift"))
                        throw new InvalidDataException("选中了 Drift 客户端，请在《漂移》入口配置此文件。");
                    profile.FingerprintSha256 = inspection.Sha256; dirty = true;
                    clientStatus.Text = inspection.Describe(); SaveProfile();
                }
            } catch (Exception e) { if (!closed && profile == target && client.Text == path) clientStatus.Text = "核对失败：" + e.Message; }
            finally { if (!closed) button.Enabled = true; }
        }
        void RefreshDevices()
        {
            preview.Checked = false;
            foreach (var input in inputs.Values) input.Dispose(); inputs.Clear(); snapshots.Clear();
            try {
                var devices = DeviceInput.Enumerate();
                steering.SetChoices(devices); throttle.SetChoices(devices); brake.SetChoices(devices);
                foreach (var device in devices) {
                    try { inputs.Add(device.InstanceGuid.ToString(), device.Open(Handle)); }
                    catch (Exception e) { status.Text = "打开设备失败：" + e.Message; }
                }
                devicesText.Text = devices.Count == 0 ? "未检测到实体设备。请连接底座／踏板，切至 PC 模式后刷新。" : String.Join("；", devices.Select(x => x.InstanceName));
            } catch (Exception e) { devicesText.Text = "设备枚举失败：" + e.Message; }
        }
        void Poll()
        {
            snapshots.Clear();
            try {
                foreach (var pair in inputs) { InputSnapshot snapshot; if (pair.Value.Read(out snapshot)) snapshots.Add(pair.Key, snapshot); }
                double wheel, gas, stop;
                bool ready = steering.UpdateSample(snapshots, out wheel);
                ready &= throttle.UpdateSample(snapshots, out gas); ready &= brake.UpdateSample(snapshots, out stop);
                ushort buttons = 0;
                InputSnapshot buttonState;
                if (snapshots.TryGetValue(profile.ButtonDeviceGuid ?? "", out buttonState)) {
                    if (profile.DriftButton >= 0 && profile.DriftButton < 128 && buttonState.IsPressed(profile.DriftButton)) buttons |= 0x1000;
                    if (profile.ItemButton >= 0 && profile.ItemButton < 128 && buttonState.IsPressed(profile.ItemButton)) buttons |= 0x2000;
                }
                bool active = preview.Checked && ready;
                pad.Write(active, active ? wheel : 0, active ? gas : 0, active ? stop : 0, active ? buttons : (ushort)0);
                if (preview.Checked) status.Text = ready ? "输入桥运行中 · 仅会话通道，尚未接入原厂比赛" : "输入桥已清零：请完成三个轴的校准，并保持设备连接";
            } catch (Exception e) { pad.Write(false, 0, 0, 0, 0); status.Text = "输入暂停：" + e.Message; }
        }
        void BindButton(bool drift)
        {
            var pressed = snapshots.SelectMany(pair => Enumerable.Range(0, 128).Where(i => pair.Value.IsPressed(i)).Select(i => new { Device = pair.Key, Button = i })).ToArray();
            if (pressed.Length != 1) { status.Text = "请按住设备上一个按钮，再点击绑定。"; return; }
            if (!String.IsNullOrEmpty(profile.ButtonDeviceGuid) && profile.ButtonDeviceGuid != pressed[0].Device) { profile.DriftButton = -1; profile.ItemButton = -1; }
            profile.ButtonDeviceGuid = pressed[0].Device;
            if (drift) profile.DriftButton = pressed[0].Button; else profile.ItemButton = pressed[0].Button;
            dirty = true; UpdateButtons();
        }
        void UpdateButtons() { buttonsText.Text = "漂移 → 手柄 A：" + ButtonName(profile.DriftButton) + "    道具 → 手柄 B：" + ButtonName(profile.ItemButton) + "    游戏内按键含义待验证"; }
        static string ButtonName(int value) { return value < 0 ? "未绑定" : "设备按钮 " + (value + 1); }
    }
    static class Program
    {
        [STAThread] static void Main(string[] args) {
            if(args.Length==2 && args[0]=="--offline-session")
            {
                try { OfflineLaunchSession.Run(args[1],Console.WriteLine,System.Threading.CancellationToken.None); }
                catch(Exception problem) { Console.Error.WriteLine(problem); Environment.ExitCode=1; }
                return;
            }
            Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
            try { Application.Run(new MainForm()); }
            catch (Exception e) { MessageBox.Show(e.Message, "Twin Kart Launcher 启动失败", MessageBoxButtons.OK, MessageBoxIcon.Error); }
        }
    }
}
