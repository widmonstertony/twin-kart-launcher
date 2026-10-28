using System;
using System.Runtime.InteropServices;

static class SdkCopyTests
{
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr LoadLibraryEx(string path,IntPtr file,uint flags);
    [DllImport("kernel32.dll",CharSet=CharSet.Ansi)] static extern IntPtr GetProcAddress(IntPtr module,string name);
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] static extern IntPtr GetModuleHandle(string name);
    [DllImport("kernel32.dll")] static extern bool FreeLibrary(IntPtr module);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] [return:MarshalAs(UnmanagedType.I1)] delegate bool Initialize([MarshalAs(UnmanagedType.LPStr)] string product);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate IntPtr Ticket();
    static int Main(string[] args)
    {
        if(args.Length!=1) return 2;
        IntPtr module=LoadLibraryEx(System.IO.Path.GetFullPath(args[0]),IntPtr.Zero,0x1100);
        if(module==IntPtr.Zero) { Console.Error.WriteLine("Load failed: "+Marshal.GetLastWin32Error()); return 1; }
        try
        {
            var init=(Initialize)Marshal.GetDelegateForFunctionPointer(GetProcAddress(module,"nxapi_init"),typeof(Initialize));
            if(!init("synthetic-local-test")) throw new Exception("Local initialization failed.");
            foreach(string name in new[]{"nxapi_get_product_ticket","nxcapi_get_product_ticket"})
            {
                var ticket=(Ticket)Marshal.GetDelegateForFunctionPointer(GetProcAddress(module,name),typeof(Ticket));
                if(Marshal.PtrToStringAnsi(ticket())!="TwinKartOriginalOfflineIdentity01") throw new Exception("Local identity mismatch.");
            }
            if(GetModuleHandle("nexon_x64.dll")!=IntPtr.Zero) throw new Exception("Unexpected online runtime loaded.");
            Console.WriteLine("PASS own SDK preparation: original local identity, both ABI ticket functions and no online runtime."); return 0;
        }
        catch(Exception problem) { Console.Error.WriteLine(problem); return 1; }
        finally { FreeLibrary(module); }
    }
}
