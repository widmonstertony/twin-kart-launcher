using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.Web.Script.Serialization;
using TwinKartLauncher;
static class AiRosterCatalogTests {
    static void Need(bool value){if(!value)throw new Exception("Private AI catalog export failed.");}
    static void Save(string path,object data){File.WriteAllText(path,new JavaScriptSerializer().Serialize(data));}
    public static int Main(string[] args){
        string root=Path.Combine(Path.GetTempPath(),"TwinKartRoster-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);
        try {
            var characters=new Dictionary<string,object>();var vehicles=new Dictionary<string,object>();var items=new List<object>();var bodies=new List<string>();
            for(int i=1;i<=8;++i) {
                string c="Character:"+i,s="Costume:"+i,k="Kart:"+i;
                characters[c]=new Dictionary<string,object>{{"Costume",s}};
                vehicles[k]=new Dictionary<string,object>();bodies.Add(k);
                foreach(var id in new[]{c,s,k})items.Add(new Dictionary<string,object>{{"id",id}});
            }
            Save(Path.Combine(root,"unlock-catalog.private.json"),new {items=items.ToArray(),kartBodies=bodies.ToArray()});
            Save(Path.Combine(root,"vehicle-parts.private.json"),new {characters,vehicles,common=new Dictionary<string,object>()});
            string config=Path.Combine(root,"gateway.json");Save(config,new {ProfileDataDirectory=root});
            string path=AiRosterCatalog.Build(config,root);var lines=File.ReadAllLines(path);
            Need(lines[0]=="TKAI1" && lines.Count(x=>x.StartsWith("C "))==8 && lines.Count(x=>x.StartsWith("K "))==8);
            ((Dictionary<string,object>)characters["Character:1"])["Costume"]="Costume:999";
            Save(Path.Combine(root,"vehicle-parts.private.json"),new {characters,vehicles,common=new Dictionary<string,object>()});
            bool rejected=false;try{AiRosterCatalog.Build(config,root);}catch(InvalidDataException){rejected=true;}Need(rejected);
            Need(File.ReadAllLines(path).SequenceEqual(lines));
            if(args.Length==2) {
                string actual=AiRosterCatalog.Build(args[0],args[1]);var rows=File.ReadAllLines(actual);
                Console.WriteLine("Private definition catalog exported: characters="+rows.Count(x=>x.StartsWith("C "))+", karts="+rows.Count(x=>x.StartsWith("K "))+"; actual model availability still requires native validation.");
            }
            Console.WriteLine("PASS AI catalog export: known IDs, kart-body selection, compatible default outfits and invalid-input preservation. No game started.");return 0;
        }catch(Exception e){Console.Error.WriteLine(e.Message);return 1;}finally{Directory.Delete(root,true);}
    }
}
