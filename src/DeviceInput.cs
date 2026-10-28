using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading;

namespace TwinKartLauncher
{
    // The native DLL only reads DirectInput 8 game-controller state in this
    // launcher's process. No input injection, virtual driver, or FFB is used.
    public sealed class AxisInfo
    {
        internal AxisInfo(NativeAxisInfo native)
        {
            Offset = checked((int)native.Offset);
            Name = native.Name ?? String.Empty;
            Minimum = native.Minimum;
            Maximum = native.Maximum;
            HasRange = native.HasRange != 0;
        }

        public int Offset { get; private set; }
        public string Name { get; private set; }
        public int Minimum { get; private set; }
        public int Maximum { get; private set; }
        public bool HasRange { get; private set; }
    }

    public sealed class DeviceInfo
    {
        internal DeviceInfo(NativeDeviceInfo native, IReadOnlyList<AxisInfo> axes)
        {
            InstanceGuid = native.InstanceGuid;
            ProductGuid = native.ProductGuid;
            VendorId = native.VendorId;
            ProductId = native.ProductId;
            DeviceType = native.DeviceType;
            InstanceName = native.InstanceName ?? String.Empty;
            ProductName = native.ProductName ?? String.Empty;
            Axes = axes;
        }

        public Guid InstanceGuid { get; private set; }
        public Guid ProductGuid { get; private set; }
        public uint VendorId { get; private set; }
        public uint ProductId { get; private set; }
        public uint DeviceType { get; private set; }
        public string InstanceName { get; private set; }
        public string ProductName { get; private set; }
        public IReadOnlyList<AxisInfo> Axes { get; private set; }

        public DeviceInput Open(IntPtr hwnd)
        {
            return DeviceInput.Open(InstanceGuid, hwnd);
        }
    }

    public sealed class InputSnapshot
    {
        private readonly int[] axes;
        private readonly byte[] buttons;
        private readonly uint[] pov;

        internal InputSnapshot(NativeInputSample native)
        {
            axes = native.Axes;
            buttons = native.Buttons;
            pov = native.Pov;
            AxisValues = Array.AsReadOnly(axes);
            Buttons = Array.AsReadOnly(buttons);
            Pov = Array.AsReadOnly(pov);
        }

        // X, Y, Z, Rx, Ry, Rz, slider 0, slider 1; use AxisInfo.Offset / 4
        // or GetAxis to read only the axes actually exposed by a device.
        public IReadOnlyList<int> AxisValues { get; private set; }
        // DirectInput sets the high bit of a button byte while pressed.
        public IReadOnlyList<byte> Buttons { get; private set; }
        // Centered POV hats return 0xFFFFFFFF; otherwise values are 1/100°.
        public IReadOnlyList<uint> Pov { get; private set; }

        public int GetAxis(AxisInfo axis)
        {
            if (axis == null) throw new ArgumentNullException("axis");
            if (axis.Offset < 0 || axis.Offset >= axes.Length * sizeof(int) ||
                axis.Offset % sizeof(int) != 0)
                throw new ArgumentOutOfRangeException("axis", "Unsupported axis offset.");
            return axes[axis.Offset / sizeof(int)];
        }

        public bool IsPressed(int index)
        {
            if (index < 0 || index >= buttons.Length)
                throw new ArgumentOutOfRangeException("index");
            return (buttons[index] & 0x80) != 0;
        }
    }

    public sealed class DeviceInput : IDisposable
    {
        private readonly object sync = new object();
        private int handle;

        private DeviceInput(int nativeHandle)
        {
            handle = nativeHandle;
        }

        public static IReadOnlyList<DeviceInfo> Enumerate()
        {
            NativeDeviceInfo[] entries = ReadDevices();
            List<DeviceInfo> result = new List<DeviceInfo>(entries.Length);
            foreach (NativeDeviceInfo entry in entries)
            {
                IReadOnlyList<AxisInfo> axes;
                try { axes = EnumerateAxes(entry.InstanceGuid); }
                catch (COMException) { axes = new List<AxisInfo>().AsReadOnly(); }
                result.Add(new DeviceInfo(entry, axes));
            }
            return result.AsReadOnly();
        }

        public static IReadOnlyList<AxisInfo> EnumerateAxes(Guid instanceGuid)
        {
            for (int attempt = 0; attempt < 3; attempt++)
            {
                int capacity = NativeMethods.EnumerateAxes(ref instanceGuid, null, 0);
                ThrowIfFailed(capacity);
                NativeAxisInfo[] entries = new NativeAxisInfo[capacity];
                int count = NativeMethods.EnumerateAxes(ref instanceGuid, entries, entries.Length);
                ThrowIfFailed(count);
                if (count > entries.Length) continue; // hot-plug changed the list
                List<AxisInfo> result = new List<AxisInfo>(count);
                for (int i = 0; i < count; i++) result.Add(new AxisInfo(entries[i]));
                return result.AsReadOnly();
            }
            throw new InvalidOperationException("Controller axes changed during enumeration.");
        }

        public static DeviceInput Open(Guid instanceGuid, IntPtr hwnd)
        {
            int opened = NativeMethods.Open(ref instanceGuid, hwnd);
            ThrowIfFailed(opened);
            if (opened == 0) throw new InvalidOperationException("Invalid DirectInput handle.");
            return new DeviceInput(opened);
        }

        // Returns false when unplugged or temporarily unavailable. The same
        // instance retries attachment on the next Read; if Windows assigns a
        // different instance GUID, call Enumerate again and open that device.
        public bool Read(out InputSnapshot snapshot)
        {
            lock (sync)
            {
                if (handle == 0) throw new ObjectDisposedException("DeviceInput");
                NativeInputSample native = new NativeInputSample();
                native.Axes = new int[8];
                native.Buttons = new byte[128];
                native.Pov = new uint[4];
                int status = NativeMethods.Read(handle, ref native);
                ThrowIfFailed(status);
                if (status == 1)
                {
                    snapshot = null;
                    return false;
                }
                if (status != 0) throw new InvalidOperationException("Unexpected DirectInput read status.");
                snapshot = new InputSnapshot(native);
                return true;
            }
        }

        public void Dispose()
        {
            lock (sync)
            {
                int oldHandle = Interlocked.Exchange(ref handle, 0);
                if (oldHandle != 0) NativeMethods.Close(oldHandle);
            }
            GC.SuppressFinalize(this);
        }

        ~DeviceInput()
        {
            lock (sync)
            {
                int oldHandle = Interlocked.Exchange(ref handle, 0);
                if (oldHandle != 0) NativeMethods.Close(oldHandle);
            }
        }

        private static NativeDeviceInfo[] ReadDevices()
        {
            for (int attempt = 0; attempt < 3; attempt++)
            {
                int capacity = NativeMethods.EnumerateDevices(null, 0);
                ThrowIfFailed(capacity);
                NativeDeviceInfo[] entries = new NativeDeviceInfo[capacity];
                int count = NativeMethods.EnumerateDevices(entries, entries.Length);
                ThrowIfFailed(count);
                if (count > entries.Length) continue; // hot-plug changed the list
                if (count == entries.Length) return entries;
                NativeDeviceInfo[] trimmed = new NativeDeviceInfo[count];
                Array.Copy(entries, trimmed, count);
                return trimmed;
            }
            throw new InvalidOperationException("Controllers changed during enumeration.");
        }

        private static void ThrowIfFailed(int result)
        {
            if (result < 0) Marshal.ThrowExceptionForHR(result);
        }
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    internal struct NativeDeviceInfo
    {
        public Guid InstanceGuid;
        public Guid ProductGuid;
        public uint VendorId;
        public uint ProductId;
        public uint DeviceType;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
        public string InstanceName;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
        public string ProductName;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    internal struct NativeAxisInfo
    {
        public uint Offset;
        public int Minimum;
        public int Maximum;
        public uint HasRange;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
        public string Name;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct NativeInputSample
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public int[] Axes;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 128)]
        public byte[] Buttons;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
        public uint[] Pov;
    }

    internal static class NativeMethods
    {
        private const string Library = "InputDevice.dll";

        [DllImport(Library, EntryPoint = "KD_EnumerateDevices",
            CallingConvention = CallingConvention.StdCall, ExactSpelling = true)]
        internal static extern int EnumerateDevices(
            [Out] NativeDeviceInfo[] output, int capacity);

        [DllImport(Library, EntryPoint = "KD_EnumerateAxes",
            CallingConvention = CallingConvention.StdCall, ExactSpelling = true)]
        internal static extern int EnumerateAxes(
            ref Guid instanceGuid, [Out] NativeAxisInfo[] output, int capacity);

        [DllImport(Library, EntryPoint = "KD_Open",
            CallingConvention = CallingConvention.StdCall, ExactSpelling = true)]
        internal static extern int Open(ref Guid instanceGuid, IntPtr hwnd);

        [DllImport(Library, EntryPoint = "KD_Read",
            CallingConvention = CallingConvention.StdCall, ExactSpelling = true)]
        internal static extern int Read(int handle, ref NativeInputSample output);

        [DllImport(Library, EntryPoint = "KD_Close",
            CallingConvention = CallingConvention.StdCall, ExactSpelling = true)]
        internal static extern void Close(int handle);
    }
}
