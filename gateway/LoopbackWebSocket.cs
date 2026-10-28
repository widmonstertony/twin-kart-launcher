using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Text;
using System.Threading;

namespace TwinKartLauncher.Gateway
{
    // Small RFC6455 server transport using system socket/crypto APIs only.
    // No URL ACL, firewall rule, TLS interception or non-loopback listener.
    public sealed class LoopbackWebSocketServer : IDisposable
    {
        readonly TcpListener listener;
        readonly Func<Func<string, GatewayReply>> createSession;
        readonly List<TcpClient> clients = new List<TcpClient>();
        readonly Thread acceptThread;
        volatile bool stopped;
        public int Port { get { return ((IPEndPoint)listener.LocalEndpoint).Port; } }

        public LoopbackWebSocketServer(int port, Func<Func<string, GatewayReply>> factory)
        {
            if (factory == null) throw new ArgumentNullException("factory");
            createSession = factory;
            listener = new TcpListener(IPAddress.Loopback, port);
            listener.Start(8);
            acceptThread = new Thread(Accept) { IsBackground = true };
            acceptThread.Start();
        }

        void Accept()
        {
            while (!stopped)
            {
                TcpClient client;
                try { client = listener.AcceptTcpClient(); }
                catch (SocketException) { if (stopped) return; continue; }
                catch (ObjectDisposedException) { return; }
                lock (clients)
                {
                    if (stopped || clients.Count >= 8) { client.Close(); continue; }
                    clients.Add(client);
                }
                new Thread(() => Serve(client)) { IsBackground = true }.Start();
            }
        }

        void Serve(TcpClient client)
        {
            try
            {
                client.NoDelay = true; client.ReceiveTimeout = 10000; client.SendTimeout = 10000;
                using (NetworkStream stream = client.GetStream())
                {
                    var wire = new WebSocketPeer(stream);
                    if (!wire.Upgrade()) return;
                    // The real game may wait about 50 seconds between echo
                    // requests. The HTTP-handshake deadline must not close an
                    // otherwise valid, idle WebSocket after ten seconds.
                    client.ReceiveTimeout = 0;
                    Func<string, GatewayReply> handle = createSession();
                    while (!stopped)
                    {
                        string message;
                        try { message = wire.ReadText(); }
                        catch (WebSocketProtocolException problem)
                        { FinishClose(client, stream, wire, problem.Code, "invalid_websocket_message"); return; }
                        if (message == null) return;
                        GatewayReply reply = handle(message);
                        if (reply.CloseCode != 0) { FinishClose(client, stream, wire, reply.CloseCode, reply.Event); return; }
                        wire.SendText(reply.Text);
                        if (reply.Notices != null) foreach (string notice in reply.Notices) wire.SendText(notice);
                    }
                }
            }
            catch (IOException) { }
            catch (SocketException) { }
            catch (ObjectDisposedException) { }
            catch (ArgumentException) { }
            finally { client.Close(); lock (clients) clients.Remove(client); }
        }

        static void FinishClose(TcpClient client, NetworkStream stream, WebSocketPeer wire, ushort code, string reason)
        {
            wire.Close(code, reason);
            // Closing a socket with unread invalid bytes can send RST and erase
            // the close frame. Half-close, then bound the remaining input drain.
            client.Client.Shutdown(SocketShutdown.Send);
            client.ReceiveTimeout = 200;
            byte[] drain = new byte[4096]; int total = 0;
            var clock = System.Diagnostics.Stopwatch.StartNew();
            try
            {
                while (total < 65536 && clock.ElapsedMilliseconds < 250)
                {
                    int count = stream.Read(drain, 0, drain.Length);
                    if (count == 0) break;
                    total += count;
                }
            }
            catch (IOException) { }
        }

        public void Dispose()
        {
            stopped = true; listener.Stop();
            lock (clients) foreach (TcpClient client in clients.ToArray()) client.Close();
            if (Thread.CurrentThread != acceptThread) acceptThread.Join(2000);
        }
    }

    internal sealed class WebSocketProtocolException : IOException
    {
        internal readonly ushort Code;
        internal WebSocketProtocolException(ushort code) { Code = code; }
    }

    internal sealed class WebSocketPeer
    {
        readonly Stream stream;
        static readonly UTF8Encoding Utf8 = new UTF8Encoding(false, true);
        const int MaximumMessage = WireCodec.MaxWireChars;
        internal WebSocketPeer(Stream input) { stream = input; }

        byte[] Exact(int count)
        {
            byte[] bytes = new byte[count]; int done = 0;
            while (done < count)
            {
                int read = stream.Read(bytes, done, count - done);
                if (read == 0) throw new EndOfStreamException();
                done += read;
            }
            return bytes;
        }

        internal bool Upgrade()
        {
            var header = new List<byte>();
            while (header.Count < 16384)
            {
                int value = stream.ReadByte();
                if (value < 0) throw new EndOfStreamException();
                if (value > 127 || value == 0) return false;
                header.Add((byte)value);
                int n = header.Count;
                if (n >= 4 && header[n-4] == 13 && header[n-3] == 10 && header[n-2] == 13 && header[n-1] == 10) break;
            }
            string text = Encoding.ASCII.GetString(header.ToArray());
            if (!text.EndsWith("\r\n\r\n", StringComparison.Ordinal)) return false;
            string[] lines = text.Split(new[] { "\r\n" }, StringSplitOptions.None);
            string[] request = lines[0].Split(' ');
            if (request.Length != 3 || request[0] != "GET" || request[2] != "HTTP/1.1") return false;
            var fields = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            for (int i = 1; i < lines.Length && lines[i].Length != 0; i++)
            {
                int colon = lines[i].IndexOf(':');
                if (colon <= 0 || char.IsWhiteSpace(lines[i][0])) return false;
                string name = lines[i].Substring(0, colon);
                if (fields.ContainsKey(name)) return false;
                fields.Add(name, lines[i].Substring(colon + 1).Trim());
            }
            string host;
            if (!fields.TryGetValue("Host", out host) || !IsLocalHost(host)) return false;
            if (request[1] == "/health")
            {
                byte[] body = Encoding.ASCII.GetBytes("{\"phase\":\"development-gateway\",\"playable\":false}");
                WriteAscii("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: " + body.Length + "\r\n\r\n");
                stream.Write(body, 0, body.Length); return false;
            }
            if (request[1] != "/" && request[1] != "//") return false;
            string upgrade, connection, version, key;
            if (!fields.TryGetValue("Upgrade", out upgrade) || !upgrade.Equals("websocket", StringComparison.OrdinalIgnoreCase) ||
                !fields.TryGetValue("Connection", out connection) || !ContainsToken(connection, "Upgrade") ||
                !fields.TryGetValue("Sec-WebSocket-Version", out version) || version != "13" ||
                !fields.TryGetValue("Sec-WebSocket-Key", out key)) return false;
            byte[] nonce;
            try { nonce = Convert.FromBase64String(key); } catch (FormatException) { return false; }
            if (nonce.Length != 16 || Convert.ToBase64String(nonce) != key) return false;
            string protocols;
            bool ws = fields.TryGetValue("Sec-WebSocket-Protocol", out protocols) && ContainsToken(protocols, "ws");
            string accept;
            using (SHA1 sha = SHA1.Create())
                accept = Convert.ToBase64String(sha.ComputeHash(Encoding.ASCII.GetBytes(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11")));
            WriteAscii("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + accept +
                "\r\n" + (ws ? "Sec-WebSocket-Protocol: ws\r\n" : "") + "\r\n");
            return true;
        }

        static bool IsLocalHost(string host)
        {
            Uri uri;
            return Uri.TryCreate("http://" + host + "/", UriKind.Absolute, out uri) &&
                uri.UserInfo.Length == 0 && uri.AbsolutePath == "/" && uri.Query.Length == 0 && uri.Fragment.Length == 0 &&
                (uri.Host == "127.0.0.1" || uri.Host.Equals("localhost", StringComparison.OrdinalIgnoreCase));
        }
        static bool ContainsToken(string value, string token)
        {
            foreach (string part in value.Split(','))
                if (part.Trim().Equals(token, StringComparison.OrdinalIgnoreCase)) return true;
            return false;
        }
        void WriteAscii(string value)
        { byte[] bytes = Encoding.ASCII.GetBytes(value); stream.Write(bytes, 0, bytes.Length); }

        internal string ReadText()
        {
            using (var message = new MemoryStream())
            {
                bool fragmented = false;
                while (true)
                {
                    byte[] head = Exact(2);
                    bool final = (head[0] & 128) != 0;
                    int opcode = head[0] & 15;
                    if ((head[0] & 112) != 0 || (head[1] & 128) == 0) throw new WebSocketProtocolException(1002);
                    int marker = head[1] & 127;
                    ulong length = (ulong)marker;
                    if (marker >= 126)
                    {
                        byte[] extended = Exact(marker == 126 ? 2 : 8); length = 0;
                        foreach (byte part in extended) length = (length << 8) | part;
                        if ((marker == 127 && (extended[0] & 128) != 0) ||
                            (marker == 126 && length < 126) || (marker == 127 && length <= 65535))
                            throw new WebSocketProtocolException(1002);
                    }
                    bool control = opcode >= 8;
                    if (control && (!final || length > 125)) throw new WebSocketProtocolException(1002);
                    if (length > MaximumMessage || (!control && (ulong)message.Length + length > MaximumMessage))
                        throw new WebSocketProtocolException(1009);
                    byte[] mask = Exact(4), payload = Exact((int)length);
                    for (int i = 0; i < payload.Length; i++) payload[i] ^= mask[i % 4];
                    if (opcode == 9) { Send(10, payload); continue; }
                    if (opcode == 10) continue;
                    if (opcode == 8)
                    {
                        if (payload.Length == 1) throw new WebSocketProtocolException(1002);
                        if (payload.Length >= 2)
                        {
                            int code = payload[0] * 256 + payload[1];
                            if (!((code >= 1000 && code <= 1014 && code != 1004 && code != 1005 && code != 1006) ||
                                  (code >= 3000 && code <= 4999))) throw new WebSocketProtocolException(1002);
                            try { Utf8.GetString(payload, 2, payload.Length - 2); }
                            catch (DecoderFallbackException) { throw new WebSocketProtocolException(1007); }
                        }
                        Send(8, payload); return null;
                    }
                    if (opcode == 2) throw new WebSocketProtocolException(1003);
                    if ((opcode != 0 && opcode != 1) || (opcode == 0 && !fragmented) || (opcode == 1 && fragmented))
                        throw new WebSocketProtocolException(1002);
                    message.Write(payload, 0, payload.Length);
                    if (final)
                    {
                        try { return Utf8.GetString(message.ToArray()); }
                        catch (DecoderFallbackException) { throw new WebSocketProtocolException(1007); }
                    }
                    fragmented = true;
                }
            }
        }

        internal void SendText(string value) { Send(1, Utf8.GetBytes(value)); }
        internal void Close(ushort code, string reason)
        {
            byte[] text = Utf8.GetBytes(reason);
            if (text.Length > 123) throw new ArgumentException("Oversized close reason");
            byte[] body = new byte[text.Length + 2];
            body[0] = (byte)(code >> 8); body[1] = (byte)code;
            Buffer.BlockCopy(text, 0, body, 2, text.Length); Send(8, body);
        }
        void Send(int opcode, byte[] payload)
        {
            if (payload.Length > MaximumMessage) throw new IOException("Oversized outgoing message");
            using (var header = new MemoryStream())
            {
                header.WriteByte((byte)(128 | opcode));
                if (payload.Length < 126) header.WriteByte((byte)payload.Length);
                else
                {
                    int bytes = payload.Length <= 65535 ? 2 : 8;
                    header.WriteByte((byte)(bytes == 2 ? 126 : 127));
                    ulong length = (ulong)payload.Length;
                    for (int i = bytes - 1; i >= 0; i--) header.WriteByte((byte)(length >> (8 * i)));
                }
                byte[] data = header.ToArray(); stream.Write(data, 0, data.Length);
                stream.Write(payload, 0, payload.Length);
            }
        }
    }
}
