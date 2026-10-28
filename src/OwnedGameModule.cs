using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

namespace TwinKartLauncher
{
    internal static class OwnedGameModule
    {
        [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr VirtualAllocEx(IntPtr process, IntPtr at, UIntPtr size, uint allocation, uint protection);
        [DllImport("kernel32.dll", SetLastError=true)] static extern bool VirtualFreeEx(IntPtr process, IntPtr at, UIntPtr size, uint operation);
        [DllImport("kernel32.dll", SetLastError=true)] static extern bool WriteProcessMemory(IntPtr process, IntPtr at, byte[] bytes, UIntPtr count, out UIntPtr written);
        [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] static extern IntPtr GetModuleHandle(string name);
        [DllImport("kernel32.dll", CharSet=CharSet.Ansi)] static extern IntPtr GetProcAddress(IntPtr module, string name);
        [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr CreateRemoteThread(IntPtr process, IntPtr attributes, UIntPtr stack, IntPtr start, IntPtr parameter, uint flags, out uint threadId);
        [DllImport("kernel32.dll")] static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);
        [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
        internal static void Load(Process ownedProcess, string expectedGame, string modulePath, string moduleHash)
        {
            if (ownedProcess.HasExited || !String.Equals(ownedProcess.MainModule.FileName, expectedGame, StringComparison.OrdinalIgnoreCase)) throw new InvalidOperationException("Owned game process changed.");
            modulePath=Path.GetFullPath(modulePath);
            if (ProfileStore.Fingerprint(modulePath)!=moduleHash) throw new InvalidDataException("Native module fingerprint mismatch.");
            IntPtr local=GetModuleHandle("kernel32.dll"), entry=GetProcAddress(local,"LoadLibraryW"), remote=IntPtr.Zero;
            foreach(ProcessModule loaded in ownedProcess.Modules)
                if(String.Equals(loaded.ModuleName,"kernel32.dll",StringComparison.OrdinalIgnoreCase)) remote=loaded.BaseAddress;
            if(local==IntPtr.Zero || entry==IntPtr.Zero || remote==IntPtr.Zero) throw new InvalidOperationException("Loader module unavailable.");
            IntPtr start=new IntPtr(checked(remote.ToInt64()+entry.ToInt64()-local.ToInt64()));
            byte[] path=Encoding.Unicode.GetBytes(modulePath+"\0");
            IntPtr allocation=VirtualAllocEx(ownedProcess.Handle,IntPtr.Zero,(UIntPtr)path.Length,0x3000,0x04);
            if(allocation==IntPtr.Zero) throw new System.ComponentModel.Win32Exception();
            bool completed=false;
            try
            {
                UIntPtr written;
                if(!WriteProcessMemory(ownedProcess.Handle,allocation,path,(UIntPtr)path.Length,out written) || written.ToUInt64()!=(ulong)path.Length) throw new System.ComponentModel.Win32Exception();
                uint id; IntPtr thread=CreateRemoteThread(ownedProcess.Handle,IntPtr.Zero,UIntPtr.Zero,start,allocation,0,out id);
                if(thread==IntPtr.Zero) throw new System.ComponentModel.Win32Exception();
                try { completed=WaitForSingleObject(thread,15000)==0; } finally { CloseHandle(thread); }
                if(!completed) throw new TimeoutException("Game module load timed out.");
                ownedProcess.Refresh();
                foreach(ProcessModule loaded in ownedProcess.Modules)
                    if(String.Equals(loaded.FileName,modulePath,StringComparison.OrdinalIgnoreCase)) return;
                throw new InvalidOperationException("Game module was not loaded.");
            }
            finally { if(completed || ownedProcess.HasExited) VirtualFreeEx(ownedProcess.Handle,allocation,UIntPtr.Zero,0x8000); }
        }
    }
}
