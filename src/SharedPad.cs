using System;
using System.IO.MemoryMappedFiles;
using System.Threading;

namespace TwinKartLauncher
{
    // This channel is private to the processes started for one local game session.
    // It never installs a driver or issues a force-feedback command.
    public sealed class SharedPad : IDisposable
    {
        public const uint Magic = 0x4B415254;
        public const uint Version = 1;
        public const int Size = 64;
        readonly MemoryMappedFile mapping;
        readonly MemoryMappedViewAccessor view;
        readonly object sync = new object();
        uint sequence;
        uint packet;
        bool disposed;
        public string Name { get; private set; }
        public SharedPad() : this("Local\\KartLocalWheel-" + Guid.NewGuid().ToString("N")) { }
        public SharedPad(string name)
        {
            Name = name;
            mapping = MemoryMappedFile.CreateNew(name, Size, MemoryMappedFileAccess.ReadWrite);
            view = mapping.CreateViewAccessor(0, Size, MemoryMappedFileAccess.ReadWrite);
            view.Write(0, Magic); view.Write(4, Version);
            Write(false, 0, 0, 0, 0);
        }
        public static short SteeringValue(double value)
        {
            if (double.IsNaN(value) || double.IsInfinity(value)) return 0;
            value = Math.Max(-1, Math.Min(1, value));
            return (short)Math.Round(value * (value < 0 ? 32768 : 32767));
        }
        public static byte PedalValue(double value) {
            if (double.IsNaN(value) || double.IsInfinity(value)) return 0;
            return (byte)Math.Round(Math.Max(0, Math.Min(1, value)) * 255);
        }
        public void Write(bool enabled, double steering, double throttle, double brake, ushort buttons)
        {
            lock (sync) { WriteCore(enabled, steering, throttle, brake, buttons); }
        }
        void WriteCore(bool enabled, double steering, double throttle, double brake, ushort buttons)
        {
            if (disposed) throw new ObjectDisposedException("SharedPad");
            sequence += 2;
            view.Write(8, sequence - 1);
            Thread.MemoryBarrier();
            view.Write(12, enabled ? 1u : 0u);
            view.Write(16, unchecked((uint)Environment.TickCount));
            view.Write(20, buttons);
            view.Write(22, SteeringValue(steering));
            view.Write(24, (short)0); view.Write(26, (short)0); view.Write(28, (short)0);
            view.Write(30, PedalValue(brake)); view.Write(31, PedalValue(throttle));
            view.Write(32, ++packet);
            Thread.MemoryBarrier();
            view.Write(8, sequence);
        }
        public void Dispose()
        {
            lock (sync) {
                if (disposed) return;
                WriteCore(false, 0, 0, 0, 0);
                disposed = true;
                view.Dispose(); mapping.Dispose();
            }
        }
    }
}
