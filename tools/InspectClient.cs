using System;
using System.Web.Script.Serialization;
using TwinKartLauncher;

static class InspectClient
{
    static int Main(string[] args)
    {
        try {
            string path = args.Length == 1 ? args[0] : ClientDiscovery.FindDrift();
            if (String.IsNullOrWhiteSpace(path)) {
                Console.Error.WriteLine("No local Drift client found. Pass a client EXE path to inspect it.");
                return 2;
            }
            Console.WriteLine(new JavaScriptSerializer().Serialize(ClientDiscovery.Inspect(path)));
            return 0;
        } catch (Exception error) { Console.Error.WriteLine(error.Message); return 1; }
    }
}
