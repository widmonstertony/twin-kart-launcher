using System;
using System.Threading;
using TwinKartLauncher;

static class RunSession
{
    static int Main(string[] args)
    {
        if(args.Length!=1) { Console.Error.WriteLine("Usage: TwinKartSession.exe <private.local.json>"); return 2; }
        using(var cancellation=new CancellationTokenSource())
        {
            ConsoleCancelEventHandler stop=(sender,eventArgs)=>{eventArgs.Cancel=true;cancellation.Cancel();};
            Console.CancelKeyPress+=stop;
            try { OfflineLaunchSession.Run(args[0],Console.WriteLine,cancellation.Token); return 0; }
            catch(OperationCanceledException) { return 0; }
            catch(Exception problem) { Console.Error.WriteLine(problem); return 1; }
            finally { Console.CancelKeyPress-=stop; }
        }
    }
}
