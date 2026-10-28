using System;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Runtime.InteropServices;
using TwinKartLauncher;

namespace TwinKartLauncherTests
{
    // Small framework-free runner so the same C# 5/.NET Framework toolchain used
    // by the launcher also checks the real, compiled XInput adapter.
    internal static class InputBridgeTests
    {
        const uint Success = 0;
        const uint DeviceNotConnected = 1167;
        const uint Empty = 4306;
        static int failures;

        [StructLayout(LayoutKind.Sequential)]
        struct XInputGamepad
        {
            public ushort Buttons;
            public byte LeftTrigger;
            public byte RightTrigger;
            public short LeftX;
            public short LeftY;
            public short RightX;
            public short RightY;
        }

        [StructLayout(LayoutKind.Sequential)]
        struct XInputState
        {
            public uint Packet;
            public XInputGamepad Gamepad;
        }

        [StructLayout(LayoutKind.Sequential)]
        struct XInputVibration
        {
            public ushort LeftMotor;
            public ushort RightMotor;
        }

        [StructLayout(LayoutKind.Sequential)]
        struct XInputCapabilities
        {
            public byte Type;
            public byte SubType;
            public ushort Flags;
            public XInputGamepad Gamepad;
            public XInputVibration Vibration;
        }

        [StructLayout(LayoutKind.Sequential)]
        struct XInputBattery
        {
            public byte Type;
            public byte Level;
        }

        [DllImport("KartWheelXInput.dll", EntryPoint = "XInputGetState", CallingConvention = CallingConvention.Winapi)]
        static extern uint GetState(uint index, out XInputState state);

        [DllImport("KartWheelXInput.dll", EntryPoint = "XInputGetStateEx", CallingConvention = CallingConvention.Winapi)]
        static extern uint GetStateEx(uint index, out XInputState state);

        [DllImport("KartWheelXInput.dll", EntryPoint = "XInputGetCapabilities", CallingConvention = CallingConvention.Winapi)]
        static extern uint GetCapabilities(uint index, uint flags, out XInputCapabilities caps);

        [DllImport("KartWheelXInput.dll", EntryPoint = "XInputGetBatteryInformation", CallingConvention = CallingConvention.Winapi)]
        static extern uint GetBattery(uint index, byte type, out XInputBattery battery);

        [DllImport("KartWheelXInput.dll", EntryPoint = "XInputEnable", CallingConvention = CallingConvention.Winapi)]
        static extern void Enable(bool enabled);

        [DllImport("KartWheelXInput.dll", EntryPoint = "XInputGetKeystroke", CallingConvention = CallingConvention.Winapi)]
        static extern uint GetKeystroke(uint index, uint reserved, IntPtr keystroke);

        static int Main()
        {
            Run("calibration and inverted axes", TestCalibration);
            Run("profile persistence and fingerprint", TestPersistence);
            Run("shared memory to compiled XInput exports", TestSharedPadAndNative);
            Console.WriteLine(failures == 0 ? "All input bridge tests passed." : failures + " input bridge test(s) failed.");
            return failures == 0 ? 0 : 1;
        }

        static void Run(string name, Action test)
        {
            try { test(); Console.WriteLine("PASS " + name); }
            catch (Exception error) { failures++; Console.Error.WriteLine("FAIL " + name + ": " + error); }
        }

        static void Check(bool condition, string message)
        {
            if (!condition) throw new Exception(message);
        }

        static void Equal<T>(T expected, T actual, string message)
        {
            if (!object.Equals(expected, actual))
                throw new Exception(message + ": expected " + expected + ", got " + actual);
        }

        static void Near(double expected, double actual, string message)
        {
            if (Math.Abs(expected - actual) > 0.000001)
                throw new Exception(message + ": expected " + expected + ", got " + actual);
        }

        static AxisBinding Binding(int released, int center, int pressed)
        {
            return new AxisBinding {
                DeviceGuid = Guid.NewGuid().ToString("D"), Offset = 4,
                NegativeOrReleased = released, Center = center,
                PositiveOrPressed = pressed, DeadZone = 0.1, Calibrated = true
            };
        }

        static void TestCalibration()
        {
            AxisBinding steering = Binding(-1000, 0, 1000);
            Check(steering.IsValid(true), "normal steering rejected");
            Near(-1, steering.Normalize(-1000, true), "left endpoint");
            Near(0, steering.Normalize(0, true), "center");
            Near(1, steering.Normalize(1000, true), "right endpoint");
            Near(0, steering.Normalize(50, true), "dead zone");
            Near((0.5 - 0.1) / 0.9, steering.Normalize(500, true), "positive response");
            Near(-(0.5 - 0.1) / 0.9, steering.Normalize(-500, true), "negative response");
            Near(1, steering.Normalize(2000, true), "steering clamp");

            AxisBinding inverseSteering = Binding(1000, 0, -1000);
            Check(inverseSteering.IsValid(true), "inverted steering rejected");
            Near(-1, inverseSteering.Normalize(1000, true), "inverted left endpoint");
            Near(1, inverseSteering.Normalize(-1000, true), "inverted right endpoint");
            Near(-(0.5 - 0.1) / 0.9, inverseSteering.Normalize(500, true), "inverted midpoint");

            AxisBinding pedal = Binding(0, 0, 1000);
            Check(pedal.IsValid(false), "pedal rejected");
            Near(0, pedal.Normalize(0, false), "pedal released");
            Near(1, pedal.Normalize(1000, false), "pedal pressed");
            Near((0.5 - 0.1) / 0.9, pedal.Normalize(500, false), "pedal midpoint");
            AxisBinding inversePedal = Binding(1000, 0, 0);
            Check(inversePedal.IsValid(false), "inverted pedal rejected");
            Near(0, inversePedal.Normalize(1000, false), "inverted pedal released");
            Near(1, inversePedal.Normalize(0, false), "inverted pedal pressed");
            Near((0.5 - 0.1) / 0.9, inversePedal.Normalize(500, false), "inverted pedal midpoint");

            AxisBinding invalid = Binding(-1000, 0, 1000);
            invalid.Calibrated = false;
            Check(!invalid.IsValid(true) && invalid.Normalize(1000, true) == 0, "uncalibrated axis accepted");
            invalid.Calibrated = true; invalid.DeviceGuid = "not-a-guid";
            Check(!invalid.IsValid(true), "invalid device GUID accepted");
            invalid.DeviceGuid = Guid.NewGuid().ToString(); invalid.Offset = 5;
            Check(!invalid.IsValid(true), "unaligned axis offset accepted");
            invalid.Offset = 4; invalid.DeadZone = double.NaN;
            Check(!invalid.IsValid(true), "NaN dead zone accepted");
            invalid.DeadZone = 0.5;
            Check(!invalid.IsValid(true), "oversized dead zone accepted");
            invalid.DeadZone = 0.1; invalid.Center = 990;
            Check(!invalid.IsValid(true), "endpoint steering center accepted");
            invalid.Center = 0; invalid.PositiveOrPressed = -950;
            Check(!invalid.IsValid(true), "short calibration span accepted");
        }

        static void TestPersistence()
        {
            string directory = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "profile-test-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(directory);
            try
            {
                ProfileStore store = new ProfileStore(directory);
                LauncherProfile missing = store.Load(GameEdition.Classic);
                Equal(GameEdition.Classic, missing.Edition, "missing profile edition");
                Check(!missing.InputReady, "missing profile claimed calibrated input");
                LauncherProfile classic = new LauncherProfile {
                    Edition = GameEdition.Classic, ClientPath = "C:\\Original\\KartRider.exe",
                    FingerprintSha256 = "example-fingerprint", Steering = Binding(-1000, 0, 1000),
                    Throttle = Binding(1000, 0, 0), Brake = Binding(0, 0, 1000),
                    ButtonDeviceGuid = Guid.NewGuid().ToString("D"), DriftButton = 7, ItemButton = 11
                };
                Check(classic.InputReady, "complete profile not ready");
                store.Save(classic);
                LauncherProfile loaded = store.Load(GameEdition.Classic);
                Check(loaded.InputReady, "loaded profile lost calibration");
                Equal(classic.ClientPath, loaded.ClientPath, "client path not persisted");
                Equal(classic.FingerprintSha256, loaded.FingerprintSha256, "fingerprint not persisted");
                Equal(classic.Throttle.DeviceGuid, loaded.Throttle.DeviceGuid, "axis device not persisted");
                Equal(classic.Throttle.NegativeOrReleased, loaded.Throttle.NegativeOrReleased, "inversion not persisted");
                Equal(classic.DriftButton, loaded.DriftButton, "button not persisted");

                LauncherProfile drift = new LauncherProfile { Edition = GameEdition.Drift, ClientPath = "D:\\Drift\\KartDrift.exe" };
                store.Save(drift);
                Equal(drift.ClientPath, store.Load(GameEdition.Drift).ClientPath, "drift profile not persisted");
                Equal(classic.ClientPath, store.Load(GameEdition.Classic).ClientPath, "editions were mixed");
                classic.ItemButton = 4;
                store.Save(classic);
                Equal(4, store.Load(GameEdition.Classic).ItemButton, "replacement save failed");
                Check(File.Exists(Path.Combine(directory, "classic.json.backup")), "replacement backup missing");

                string sample = Path.Combine(directory, "hash-sample.txt");
                File.WriteAllBytes(sample, new byte[] { 97, 98, 99 });
                Equal("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", ProfileStore.Fingerprint(sample), "SHA-256 fingerprint");

                File.WriteAllText(Path.Combine(directory, "classic.json"), "{\"Edition\":1}");
                bool rejected = false;
                try { store.Load(GameEdition.Classic); }
                catch (InvalidDataException) { rejected = true; }
                Check(rejected, "wrong edition/incomplete profile accepted");
            }
            finally
            {
                // This test owns only this newly created, nonrecursive directory.
                foreach (string file in Directory.GetFiles(directory)) File.Delete(file);
                Directory.Delete(directory);
            }
        }

        static void TestSharedPadAndNative()
        {
            Equal(16, Marshal.SizeOf(typeof(XInputState)), "XINPUT_STATE ABI size");
            Equal(20, Marshal.SizeOf(typeof(XInputCapabilities)), "XINPUT_CAPABILITIES ABI size");
            Equal((short)-32768, SharedPad.SteeringValue(-1), "full left conversion");
            Equal((short)32767, SharedPad.SteeringValue(1), "full right conversion");
            Equal((byte)128, SharedPad.PedalValue(0.5), "half pedal conversion");
            Equal((byte)255, SharedPad.PedalValue(2), "pedal clamp");
            Equal((short)0, SharedPad.SteeringValue(double.NaN), "invalid steering is neutral");
            Equal((byte)0, SharedPad.PedalValue(double.PositiveInfinity), "invalid pedal is released");

            SharedPad pad = new SharedPad();
            try
            {
                Environment.SetEnvironmentVariable("TWIN_KART_INPUT_MAP", pad.Name, EnvironmentVariableTarget.Process);
                using (MemoryMappedFile map = MemoryMappedFile.OpenExisting(pad.Name, MemoryMappedFileRights.ReadWrite))
                using (MemoryMappedViewAccessor view = map.CreateViewAccessor(0, SharedPad.Size, MemoryMappedFileAccess.ReadWrite))
                {
                    Equal(SharedPad.Magic, view.ReadUInt32(0), "shared magic");
                    Equal(SharedPad.Version, view.ReadUInt32(4), "shared version");
                    Equal((uint)0, view.ReadUInt32(12), "initial disabled state");
                    Check((view.ReadUInt32(8) & 1) == 0, "initial sequence odd");

                    XInputState state;
                    Equal(DeviceNotConnected, GetState(0, out state), "disabled virtual slot");
                    pad.Write(true, -1, 0.5, 1, 0x3001);
                    Equal((uint)1, view.ReadUInt32(12), "enabled byte field");
                    Equal((ushort)0x3001, view.ReadUInt16(20), "shared buttons");
                    Equal((short)-32768, view.ReadInt16(22), "shared steering");
                    Equal((byte)255, view.ReadByte(30), "shared brake");
                    Equal((byte)128, view.ReadByte(31), "shared throttle");
                    Equal(view.ReadUInt32(32), ReadConnected(out state), "packet from real XInputGetState");
                    Equal((ushort)0x3001, state.Gamepad.Buttons, "native buttons");
                    Equal((short)-32768, state.Gamepad.LeftX, "native steering");
                    Equal((byte)128, state.Gamepad.RightTrigger, "native throttle");
                    Equal((byte)255, state.Gamepad.LeftTrigger, "native brake");
                    Equal((uint)Success, GetStateEx(0, out state), "XInputGetStateEx export");

                    XInputCapabilities caps;
                    Equal(Success, GetCapabilities(0, 0, out caps), "capabilities connected");
                    Equal((byte)1, caps.Type, "capabilities gamepad type");
                    XInputBattery battery;
                    Equal(Success, GetBattery(0, 0, out battery), "battery connected");
                    Equal((byte)1, battery.Type, "wired battery type");
                    IntPtr stroke = Marshal.AllocHGlobal(8);
                    try { Equal(Empty, GetKeystroke(0, 0, stroke), "virtual controller has no synthetic keystrokes"); }
                    finally { Marshal.FreeHGlobal(stroke); }

                    Enable(false);
                    try
                    {
                        Equal(Success, GetState(0, out state), "XInputEnable(false) disconnected slot");
                        Equal((ushort)0, state.Gamepad.Buttons, "XInputEnable(false) retained buttons");
                        Equal((short)0, state.Gamepad.LeftX, "XInputEnable(false) retained steering");
                        Equal((byte)0, state.Gamepad.RightTrigger, "XInputEnable(false) retained throttle");
                    }
                    finally { Enable(true); }
                    Equal(Success, GetState(0, out state), "XInputEnable(true) did not restore state");
                    Equal((ushort)0x3001, state.Gamepad.Buttons, "buttons not restored");

                    uint originalVersion = view.ReadUInt32(4);
                    view.Write(4, originalVersion + 1);
                    Equal(DeviceNotConnected, GetState(0, out state), "wrong protocol version accepted");
                    view.Write(4, originalVersion);
                    uint sequence = view.ReadUInt32(8);
                    view.Write(8, sequence | 1u);
                    Equal(DeviceNotConnected, GetState(0, out state), "odd write sequence accepted");
                    view.Write(8, sequence);
                    uint heartbeat = view.ReadUInt32(16);
                    view.Write(16, unchecked((uint)(Environment.TickCount - 1000)));
                    Equal(DeviceNotConnected, GetState(0, out state), "stale heartbeat accepted");
                    view.Write(16, heartbeat);
                    pad.Write(true, 1, 1, 0, 0x1000);
                    Equal(Success, GetState(0, out state), "fresh heartbeat failed to reconnect");
                    Equal((short)32767, state.Gamepad.LeftX, "updated steering");
                    Equal((ushort)0x1000, state.Gamepad.Buttons, "updated buttons");

                    pad.Write(false, 0, 0, 0, 0);
                    Equal(DeviceNotConnected, GetState(0, out state), "disabled frame accepted");
                    pad.Write(true, 0, 0, 0, 0);
                    Equal(Success, GetState(0, out state), "re-enabled frame unavailable");
                    pad.Dispose();
                    Equal(DeviceNotConnected, GetState(0, out state), "disposed writer still connected");
                    bool rejected = false;
                    try { pad.Write(true, 0, 0, 0, 0); }
                    catch (ObjectDisposedException) { rejected = true; }
                    Check(rejected, "write after dispose accepted");
                }
            }
            finally
            {
                pad.Dispose();
                Environment.SetEnvironmentVariable("TWIN_KART_INPUT_MAP", null, EnvironmentVariableTarget.Process);
            }
        }

        static uint ReadConnected(out XInputState state)
        {
            Equal(Success, GetState(0, out state), "real XInputGetState connected");
            return state.Packet;
        }
    }
}
