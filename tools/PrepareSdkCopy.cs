using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;

// Prepares a private vendor SDK copy for the independently implemented local
// identity flow. It distributes no SDK bytes and never edits the installed file.
static class PrepareSdkCopy
{
    const string Supported="456233bf2e7ef57f40663d4f808bb44c8dee3eab7b5437cf55b605577b5dfc04";
    sealed class Section { public int Header,VirtualSize,Rva,Size,File; public uint Flags; public string Name; }
    static string Hash(byte[] bytes) { using(var hash=SHA256.Create()) return BitConverter.ToString(hash.ComputeHash(bytes)).Replace("-","").ToLowerInvariant(); }
    static int Main(string[] args)
    {
        if(args.Length!=2) { Console.Error.WriteLine("Usage: TwinKartSdkCopy.exe <installed-sdk.dll> <new-private-copy.dll>"); return 2; }
        try
        {
            string source=Path.GetFullPath(args[0]),destination=Path.GetFullPath(args[1]);
            if(String.Equals(source,destination,StringComparison.OrdinalIgnoreCase) || File.Exists(destination)) throw new ArgumentException("A new destination is required.");
            byte[] bytes=File.ReadAllBytes(source);
            if(Hash(bytes)!=Supported) throw new InvalidDataException("Unsupported installed SDK hash.");
            Func<int,int> i32=at=>BitConverter.ToInt32(bytes,at);
            int nt=i32(0x3c),optional=nt+24;
            if(i32(nt)!=0x4550 || BitConverter.ToUInt16(bytes,optional)!=0x20b) throw new InvalidDataException("Expected x64 PE.");
            int count=BitConverter.ToUInt16(bytes,nt+6),headers=optional+BitConverter.ToUInt16(bytes,nt+20);
            var sections=new List<Section>();
            for(int i=0;i<count;i++)
            {
                int at=headers+40*i;
                sections.Add(new Section {Header=at,Name=Encoding.ASCII.GetString(bytes,at,8).TrimEnd('\0'),VirtualSize=i32(at+8),Rva=i32(at+12),Size=i32(at+16),File=i32(at+20),Flags=BitConverter.ToUInt32(bytes,at+36)});
            }
            Func<int,int> file=rva=>{
                foreach(var section in sections) if(rva>=section.Rva && rva-section.Rva<section.Size) return checked(section.File+rva-section.Rva);
                throw new InvalidDataException("RVA outside backed sections.");
            };
            Func<int,string> name=rva=>{int at=file(rva),end=Array.IndexOf(bytes,(byte)0,at); if(end<at || end-at>256) throw new InvalidDataException("Invalid export name."); return Encoding.ASCII.GetString(bytes,at,end-at);};
            int exportRva=i32(optional+112),exports=file(exportRva);
            int named=i32(exports+24),functions=file(i32(exports+28)),names=file(i32(exports+32)),ordinals=file(i32(exports+36));
            var entries=new Dictionary<string,int>(StringComparer.Ordinal);
            for(int i=0;i<named;i++) entries.Add(name(i32(names+4*i)),i32(functions+4*BitConverter.ToUInt16(bytes,ordinals+2*i)));
            // Only mapped, read-only section padding is used for our local text.
            byte[] ticket=Encoding.ASCII.GetBytes("TwinKartOriginalOfflineIdentity01\0");
            var storage=sections.FirstOrDefault(section=>section.Name==".rdata" && (section.Flags&0xa0000000)==0 && section.Size-section.VirtualSize>=ticket.Length);
            if(storage==null) throw new InvalidDataException("No verified read-only padding.");
            int offset=checked(storage.File+storage.VirtualSize),ticketRva=checked(storage.Rva+storage.VirtualSize);
            if(bytes.Skip(offset).Take(ticket.Length).Any(value=>value!=0)) throw new InvalidDataException("Padding already in use.");
            Buffer.BlockCopy(ticket,0,bytes,offset,ticket.Length);
            Buffer.BlockCopy(BitConverter.GetBytes(storage.VirtualSize+ticket.Length),0,bytes,storage.Header+8,4);
            Action<string,string,byte[]> patch=(symbol,signature,replacement)=>{
                int rva=entries[symbol],at=file(rva);
                var expected=Enumerable.Range(0,signature.Length/2).Select(index=>Convert.ToByte(signature.Substring(2*index,2),16)).ToArray();
                if(!bytes.Skip(at).Take(expected.Length).SequenceEqual(expected)) throw new InvalidDataException("SDK entry signature differs.");
                Buffer.BlockCopy(replacement,0,bytes,at,replacement.Length);
            };
            patch("nxapi_init","40574883ec20488bf94885c9750833c0",new byte[]{0xb8,1,0,0,0,0xc3});
            foreach(var symbol in new[]{"nxapi_get_product_ticket","nxcapi_get_product_ticket"})
            {
                byte[] code={0x48,0x8d,0x05,0,0,0,0,0xc3};
                Buffer.BlockCopy(BitConverter.GetBytes(checked(ticketRva-(entries[symbol]+7))),0,code,3,4);
                patch(symbol,symbol=="nxapi_get_product_ticket"?"4883ec28488b0d6d1303004885c97507":"4883ec28488b0dcd1203004885c97504",code);
            }
            Directory.CreateDirectory(Path.GetDirectoryName(destination));
            using(var output=new FileStream(destination,FileMode.CreateNew,FileAccess.Write,FileShare.None)) { output.Write(bytes,0,bytes.Length); output.Flush(true); }
            if(Hash(File.ReadAllBytes(source))!=Supported) throw new IOException("Installed SDK changed unexpectedly.");
            Console.WriteLine("Prepared local SDK copy: "+Hash(bytes));
            return 0;
        }
        catch(Exception problem) { Console.Error.WriteLine(problem); return 1; }
    }
}
