using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

namespace TwinKartLauncher
{
    // Establish graphics hooks before the owned client's primary thread runs.
    // No existing process is suspended or attached. Any startup failure aborts
    // only the process created by this instance, leaving session recovery intact.
    internal sealed class OwnedGameStartup : IDisposable
    {
        [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct StartupInfo {
            public int cb;public string reserved,desktop,title;public int x,y,width,height,charsX,charsY,fill,flags;
            public short show,reservedBytes;public IntPtr reservedData,input,output,error;
        }
        [StructLayout(LayoutKind.Sequential)] struct ProcessInfo {public IntPtr process,thread;public int processId,threadId;}
        [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool CreateProcess(string application,StringBuilder command,IntPtr processAttributes,IntPtr threadAttributes,bool inherit,uint flags,IntPtr environment,string directory,ref StartupInfo startup,out ProcessInfo info);
        [DllImport("kernel32.dll",SetLastError=true)] static extern uint ResumeThread(IntPtr thread);
        [DllImport("kernel32.dll",SetLastError=true)] static extern bool TerminateProcess(IntPtr process,uint code);
        [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetExitCodeProcess(IntPtr process,out uint code);
        [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
        [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr VirtualAllocEx(IntPtr process,IntPtr at,UIntPtr size,uint type,uint protection);
        [DllImport("kernel32.dll")] static extern bool VirtualFreeEx(IntPtr process,IntPtr at,UIntPtr size,uint type);
        [DllImport("kernel32.dll",SetLastError=true)] static extern bool VirtualProtectEx(IntPtr process,IntPtr at,UIntPtr size,uint protection,out uint previous);
        [DllImport("kernel32.dll",SetLastError=true)] static extern bool WriteProcessMemory(IntPtr process,IntPtr at,byte[] data,UIntPtr count,out UIntPtr written);
        [DllImport("kernel32.dll",SetLastError=true)] static extern bool ReadProcessMemory(IntPtr process,IntPtr at,byte[] data,UIntPtr count,out UIntPtr read);
        [DllImport("kernel32.dll")] static extern bool FlushInstructionCache(IntPtr process,IntPtr at,UIntPtr count);
        [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] static extern IntPtr GetModuleHandle(string name);
        [DllImport("kernel32.dll",CharSet=CharSet.Ansi)] static extern IntPtr GetProcAddress(IntPtr module,string name);
        [DllImport("psapi.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern uint GetMappedFileName(IntPtr process,IntPtr address,StringBuilder path,uint capacity);
        [DllImport("kernel32.dll",SetLastError=true)] static extern IntPtr CreateRemoteThread(IntPtr process,IntPtr attributes,UIntPtr stack,IntPtr start,IntPtr parameter,uint flags,out uint id);
        [DllImport("kernel32.dll")] static extern uint WaitForSingleObject(IntPtr handle,uint milliseconds);
        ProcessInfo info;bool resumed,disposed;
        public Process Process {get;private set;}
        public uint ExitCode {get {uint code;if(disposed || !GetExitCodeProcess(info.process,out code))throw new Win32Exception();return code;} }
        static string Quote(string text){if(text.IndexOf('"')>=0 || text.EndsWith("\\",StringComparison.Ordinal))throw new ArgumentException("Invalid executable path.");return "\""+text+"\"";}
        internal static byte[] LoaderCode(ulong loader,ulong unicode,ulong module,ulong status)
        {
            using(var bytes=new MemoryStream())using(var code=new BinaryWriter(bytes)) {
                // Windows x64: shadow space, aligned stack and four arguments
                // to the system LdrLoadDll; return NTSTATUS to owned storage.
                code.Write(new byte[]{0x48,0x83,0xec,0x28,0x31,0xc9,0x31,0xd2,0x49,0xb8});code.Write(unicode);
                code.Write(new byte[]{0x49,0xb9});code.Write(module);code.Write(new byte[]{0x48,0xb8});code.Write(loader);
                code.Write(new byte[]{0xff,0xd0,0x48,0xba});code.Write(status);
                code.Write(new byte[]{0x89,0x02,0x48,0x83,0xc4,0x28,0xc3});return bytes.ToArray();
            }
        }
        public static OwnedGameStartup Create(ProcessStartInfo start)
        {
            if(IntPtr.Size!=8 || start==null || start.UseShellExecute || !Path.IsPathRooted(start.FileName))throw new ArgumentException("Owned x64 process startup required.");
            var variables=new SortedDictionary<string,string>(StringComparer.OrdinalIgnoreCase);
            foreach(string key in start.EnvironmentVariables.Keys) {
                string value=start.EnvironmentVariables[key];
                if(key.Length==0 || key.IndexOfAny(new[]{'=','\0'})>=0 || value==null || value.IndexOf('\0')>=0)throw new ArgumentException("Invalid environment block.");
                variables[key]=value;
            }
            var block=new StringBuilder();foreach(var variable in variables)block.Append(variable.Key).Append('=').Append(variable.Value).Append('\0');block.Append('\0');
            IntPtr environment=Marshal.StringToHGlobalUni(block.ToString());var owned=new OwnedGameStartup();
            try {
                var si=new StartupInfo {cb=Marshal.SizeOf(typeof(StartupInfo))};
                var command=new StringBuilder(Quote(start.FileName)+(String.IsNullOrEmpty(start.Arguments)?"":" "+start.Arguments));
                uint flags=0x404u|(start.CreateNoWindow?0x08000000u:0u);
                if(!CreateProcess(start.FileName,command,IntPtr.Zero,IntPtr.Zero,false,flags,environment,start.WorkingDirectory,ref si,out owned.info))throw new Win32Exception();
                owned.Process=System.Diagnostics.Process.GetProcessById(owned.info.processId);
                // Retain the managed handle before this startup object's
                // native handle is disposed, including for natural exits.
                owned.Process.EnableRaisingEvents=true;return owned;
            }catch{owned.Dispose();throw;}finally{Marshal.FreeHGlobal(environment);}
        }
        public void Load(string path,string hash)
        {
            if(disposed || resumed || Process.HasExited)throw new InvalidOperationException("Startup process is not suspended.");
            path=Path.GetFullPath(path);if(ProfileStore.Fingerprint(path)!=hash)throw new InvalidDataException("Startup module fingerprint mismatch.");
            IntPtr local=GetModuleHandle("ntdll.dll"),loader=GetProcAddress(local,"LdrLoadDll"),remote=IntPtr.Zero;
            try {Process.Refresh();foreach(ProcessModule module in Process.Modules)if(String.Equals(module.ModuleName,"ntdll.dll",StringComparison.OrdinalIgnoreCase))remote=module.BaseAddress;}
            catch(Win32Exception) { }
            if(remote==IntPtr.Zero && local!=IntPtr.Zero) {
                // Before primary-thread loader initialization, the PEB module
                // list can be empty. Accept the shared system mapping only
                // after verifying its mapped filename and actual PE bytes.
                var mapped=new StringBuilder(32768);byte[] remoteHeader=new byte[4096],localHeader=new byte[4096];UIntPtr copied;
                if(GetMappedFileName(info.process,local,mapped,(uint)mapped.Capacity)>0 && String.Equals(Path.GetFileName(mapped.ToString()),"ntdll.dll",StringComparison.OrdinalIgnoreCase) &&
                   ReadProcessMemory(info.process,local,remoteHeader,(UIntPtr)remoteHeader.Length,out copied) && copied.ToUInt64()==(ulong)remoteHeader.Length) {
                    Marshal.Copy(local,localHeader,0,localHeader.Length);bool same=true;
                    for(int i=0;i<localHeader.Length;++i)if(localHeader[i]!=remoteHeader[i]){same=false;break;}
                    if(same)remote=local;
                }
            }
            if(local==IntPtr.Zero || loader==IntPtr.Zero || remote==IntPtr.Zero)throw new InvalidOperationException("Native startup loader unavailable.");
            byte[] name=Encoding.Unicode.GetBytes(path+"\0");if(name.Length>65534)throw new ArgumentException("Module path exceeds native Unicode bounds.");
            IntPtr data=VirtualAllocEx(info.process,IntPtr.Zero,(UIntPtr)(name.Length+32),0x3000,0x04),code=IntPtr.Zero;
            if(data==IntPtr.Zero)throw new Win32Exception();bool complete=false;
            try {
                byte[] parameters=new byte[name.Length+32];Buffer.BlockCopy(BitConverter.GetBytes(unchecked((uint)0xffffffff)),0,parameters,0,4);
                Buffer.BlockCopy(BitConverter.GetBytes((ushort)(name.Length-2)),0,parameters,16,2);Buffer.BlockCopy(BitConverter.GetBytes((ushort)name.Length),0,parameters,18,2);
                Buffer.BlockCopy(BitConverter.GetBytes(data.ToInt64()+32),0,parameters,24,8);Buffer.BlockCopy(name,0,parameters,32,name.Length);
                UIntPtr written;if(!WriteProcessMemory(info.process,data,parameters,(UIntPtr)parameters.Length,out written) || written.ToUInt64()!=(ulong)parameters.Length)throw new Win32Exception();
                byte[] instructions=LoaderCode(checked((ulong)(remote.ToInt64()+loader.ToInt64()-local.ToInt64())),(ulong)data.ToInt64()+16,(ulong)data.ToInt64()+8,(ulong)data.ToInt64());
                code=VirtualAllocEx(info.process,IntPtr.Zero,(UIntPtr)instructions.Length,0x3000,0x04);if(code==IntPtr.Zero)throw new Win32Exception();
                if(!WriteProcessMemory(info.process,code,instructions,(UIntPtr)instructions.Length,out written) || written.ToUInt64()!=(ulong)instructions.Length)throw new Win32Exception();
                uint protection;if(!VirtualProtectEx(info.process,code,(UIntPtr)instructions.Length,0x20,out protection) || !FlushInstructionCache(info.process,code,(UIntPtr)instructions.Length))throw new Win32Exception();
                uint id;IntPtr thread=CreateRemoteThread(info.process,IntPtr.Zero,UIntPtr.Zero,code,IntPtr.Zero,0,out id);if(thread==IntPtr.Zero)throw new Win32Exception();
                try{complete=WaitForSingleObject(thread,15000)==0;}finally{CloseHandle(thread);}
                if(!complete)throw new TimeoutException("Owned startup module loader timed out.");
                byte[] result=new byte[16];if(!ReadProcessMemory(info.process,data,result,(UIntPtr)result.Length,out written) || written.ToUInt64()!=16)throw new Win32Exception();
                if(BitConverter.ToInt32(result,0)<0 || BitConverter.ToInt64(result,8)==0)throw new InvalidOperationException("Owned startup module did not load.");
            }finally{
                // A timed-out instruction stream must not be freed while it
                // could still run. Dispose aborts this owned suspended process.
                if(complete){if(code!=IntPtr.Zero)VirtualFreeEx(info.process,code,UIntPtr.Zero,0x8000);VirtualFreeEx(info.process,data,UIntPtr.Zero,0x8000);}
            }
        }
        public void Resume()
        {
            if(disposed || resumed || ResumeThread(info.thread)!=1)throw new InvalidOperationException("Owned primary thread did not resume from its initial suspension.");
            resumed=true;CloseHandle(info.thread);info.thread=IntPtr.Zero;
        }
        public void Dispose()
        {
            if(disposed)return;disposed=true;
            if(!resumed && info.process!=IntPtr.Zero){TerminateProcess(info.process,1);WaitForSingleObject(info.process,10000);}
            if(info.thread!=IntPtr.Zero)CloseHandle(info.thread);if(info.process!=IntPtr.Zero)CloseHandle(info.process);
        }
    }
}
