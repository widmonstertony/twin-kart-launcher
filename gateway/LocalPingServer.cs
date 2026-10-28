using System;
using System.Net;
using System.Net.Sockets;
using System.Threading;

namespace TwinKartLauncher.Gateway
{
    // The observed client latency probe is a 92-byte UDP datagram. This local
    // endpoint echoes only loopback probes and owns no race simulation state.
    public sealed class LocalPingServer : IDisposable
    {
        readonly UdpClient socket = new UdpClient(new IPEndPoint(IPAddress.Loopback, 0));
        readonly Thread worker;
        volatile bool stopped;
        public int Port { get { return ((IPEndPoint)socket.Client.LocalEndPoint).Port; } }
        public LocalPingServer()
        {
            worker = new Thread(() => {
                while (!stopped) try {
                    var source = new IPEndPoint(IPAddress.Loopback, 0);
                    byte[] bytes = socket.Receive(ref source);
                    if (IPAddress.IsLoopback(source.Address) && bytes.Length == 92) socket.Send(bytes, bytes.Length, source);
                } catch (SocketException) { if (stopped) return; }
                  catch (ObjectDisposedException) { return; }
            }) { IsBackground = true };
            worker.Start();
        }
        public void Dispose() { stopped = true; socket.Close(); worker.Join(2000); }
    }
}
