using System;
using System.Collections.Generic;
using System.IO;
using System.Net.Sockets;
using System.Net.WebSockets;
using System.Text;
using System.Threading;
using System.Web.Script.Serialization;
using TwinKartLauncher.Gateway;

static class GatewayTests
{
    static readonly JavaScriptSerializer Json = new JavaScriptSerializer();
    static void Check(bool condition, string message)
    { if (!condition) throw new Exception(message); }
    static PreAuthSettings Settings()
    {
        return new PreAuthSettings {
            TransportKey = Convert.ToBase64String(Encoding.ASCII.GetBytes("0123456789abcdef0123456789abcdef")),
            HashSecret = "SYNTHETIC_HASH_SECRET", PreAuthKey = "SYNTHETIC_PREAUTH_KEY",
            ExpectedClientVersion = "TEST;0000_000000;abcdef", AccountPlatform = 2,
            Port = 59876, RunSeconds = 30
        };
    }
    static Dictionary<string, object> Packet(PreAuthSettings s)
    {
        return new Dictionary<string, object> {
            {"_type", "PqPreAuth"}, {"reqId", 17}, {"key", s.PreAuthKey},
            {"clientVer", s.ExpectedClientVersion}, {"accountPlatform", 2},
            {"platform", 1}, {"language", "en"}, {"culture", "en-US"}
        };
    }
    static string Encode(object value, string hash, string key)
    {
        string wire, error;
        Check(WireCodec.TryEncode(Json.Serialize(value), hash, key, out wire, out error), "encode");
        return wire;
    }
    static void CheckReply(string wire, PreAuthSettings s)
    {
        WireDecodeResult decoded; string error;
        Check(WireCodec.TryDecode(wire, s.TransportKey, null, out decoded, out error), "decrypt response");
        Check(decoded.WasEncrypted && decoded.HashStatus == WireHashStatus.NotPresent, "response envelope");
        var packet = (Dictionary<string, object>)Json.DeserializeObject(decoded.Json);
        Check((string)packet["_type"] == "PrPacket" && (int)packet["reqId"] == 17, "reply correlation");
        Check((int)((Dictionary<string, object>)packet["result"])["code"] == 0, "preauth result");
        Check(!packet.ContainsKey("token") && !packet.ContainsKey("accountId"), "must not fabricate account auth");
    }
    static void SessionChecks()
    {
        var s = Settings();
        var session = new PreAuthSession(s);
        string request = Encode(Packet(s), s.HashSecret, s.TransportKey);
        var reply = session.Handle(request);
        Check(reply.CloseCode == 0 && session.PreAuthAccepted, "valid preauth"); CheckReply(reply.Text, s);
        Check(session.Handle(request).CloseCode == 1008, "repeat preauth");
        Check(session.Handle(Encode(new { _type = "PqAuthNxA", reqId = 18, ticket = "SYNTHETIC" }, s.HashSecret, s.TransportKey)).CloseCode == 1003,
            "unimplemented auth must not succeed");
        Check(!new PreAuthSession(s).PreAuthAccepted, "session isolation");
        foreach (string wire in new[] {
            Encode(Packet(s), s.HashSecret, null), Encode(Packet(s), null, s.TransportKey),
            Encode(Packet(s), "WRONG_HASH", s.TransportKey),
            Encode(Packet(s), s.HashSecret, Convert.ToBase64String(new byte[32])), "invalid-envelope" })
        {
            var bad = new PreAuthSession(s);
            Check(bad.Handle(wire).CloseCode == 1008 && !bad.PreAuthAccepted, "invalid envelope accepted");
        }
        foreach (string field in new[] { "key", "clientVer", "accountPlatform", "platform", "reqId", "language", "culture" })
        {
            var packet = Packet(s); packet.Remove(field);
            Check(new PreAuthSession(s).Handle(Encode(packet, s.HashSecret, s.TransportKey)).CloseCode == 1008,
                "missing required field: " + field);
        }
        foreach (string field in new[] { "key", "clientVer", "accountPlatform", "platform", "reqId" })
        {
            var packet = Packet(s); packet[field] = "WRONG_VALUE";
            Check(new PreAuthSession(s).Handle(Encode(packet, s.HashSecret, s.TransportKey)).CloseCode == 1008,
                "wrong field: " + field);
        }
    }

    static void DotNetExchange(int port, string route, PreAuthSettings s)
    {
        using (var socket = new ClientWebSocket())
        using (var deadline = new CancellationTokenSource(5000))
        {
            socket.Options.Proxy = null; socket.Options.AddSubProtocol("ws");
            socket.ConnectAsync(new Uri("ws://127.0.0.1:" + port + route), deadline.Token).GetAwaiter().GetResult();
            Check(socket.SubProtocol == "ws", "subprotocol negotiation");
            byte[] wire = Encoding.UTF8.GetBytes(Encode(Packet(s), s.HashSecret, s.TransportKey));
            int split = 71;
            socket.SendAsync(new ArraySegment<byte>(wire, 0, split), WebSocketMessageType.Text, false, deadline.Token).GetAwaiter().GetResult();
            socket.SendAsync(new ArraySegment<byte>(wire, split, wire.Length-split), WebSocketMessageType.Text, true, deadline.Token).GetAwaiter().GetResult();
            byte[] buffer = new byte[8192];
            var received = socket.ReceiveAsync(new ArraySegment<byte>(buffer), deadline.Token).GetAwaiter().GetResult();
            Check(received.MessageType == WebSocketMessageType.Text && received.EndOfMessage, "response framing");
            CheckReply(Encoding.UTF8.GetString(buffer, 0, received.Count), s);
            byte[] auth = Encoding.UTF8.GetBytes(Encode(new { _type = "PqAuthNxA", reqId = 18 }, s.HashSecret, s.TransportKey));
            socket.SendAsync(new ArraySegment<byte>(auth), WebSocketMessageType.Text, true, deadline.Token).GetAwaiter().GetResult();
            var close = socket.ReceiveAsync(new ArraySegment<byte>(buffer), deadline.Token).GetAwaiter().GetResult();
            Check(close.MessageType == WebSocketMessageType.Close && (int)close.CloseStatus.Value == 1003,
                "account auth should remain explicitly unsupported");
        }
    }

    static TcpClient Raw(int port)
    {
        var client = new TcpClient(); client.Connect("127.0.0.1", port);
        client.ReceiveTimeout = 3000; client.SendTimeout = 3000;
        string request = "GET / HTTP/1.1\r\nHost: 127.0.0.1:" + port +
            "\r\nUpgrade: websocket\r\nConnection: keep-alive, Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n";
        byte[] bytes = Encoding.ASCII.GetBytes(request); var stream = client.GetStream(); stream.Write(bytes, 0, bytes.Length);
        var header = new StringBuilder();
        while (header.Length < 8192 && !header.ToString().EndsWith("\r\n\r\n", StringComparison.Ordinal))
        { int b = stream.ReadByte(); Check(b >= 0, "handshake EOF"); header.Append((char)b); }
        Check(header.ToString().Contains("101 Switching Protocols") &&
              header.ToString().Contains("s3pPLMBiTxaQ9kYGzzhZRbK+xOo="), "RFC6455 handshake vector");
        return client;
    }
    static byte[] Frame(byte first, byte[] payload, bool masked)
    {
        using (var output = new MemoryStream())
        {
            output.WriteByte(first);
            if (payload.Length < 126) output.WriteByte((byte)((masked ? 128 : 0) | payload.Length));
            else { output.WriteByte((byte)((masked ? 128 : 0) | 126)); output.WriteByte((byte)(payload.Length >> 8)); output.WriteByte((byte)payload.Length); }
            byte[] mask = { 3, 7, 11, 19 };
            if (masked) output.Write(mask, 0, 4);
            for (int i = 0; i < payload.Length; i++) output.WriteByte((byte)(payload[i] ^ (masked ? mask[i%4] : 0)));
            return output.ToArray();
        }
    }
    static byte[] Read(Stream stream, int n)
    {
        byte[] bytes = new byte[n]; int offset = 0;
        while (offset < n) { int got = stream.Read(bytes, offset, n-offset); Check(got > 0, "unexpected EOF"); offset += got; }
        return bytes;
    }
    static byte[] ServerFrame(Stream stream, byte opcode)
    {
        byte[] head = Read(stream, 2);
        Check(head[0] == (128 | opcode) && (head[1] & 128) == 0, "server frame flags");
        int length = head[1];
        if (length == 126) { byte[] extended = Read(stream, 2); length = extended[0]*256+extended[1]; }
        Check(length < 65536, "test response too large"); return Read(stream, length);
    }
    static void ExpectClose(int port, byte[] frame, int code)
    {
        try {
        using (var client = Raw(port))
        {
            var stream = client.GetStream(); stream.Write(frame, 0, frame.Length);
            byte[] close = ServerFrame(stream, 8);
            Check(close.Length >= 2 && close[0]*256 + close[1] == code, "protocol close code");
        }
        } catch (Exception error) { throw new Exception("Frame first byte " + frame[0] + ", expected close " + code, error); }
    }
    static void TransportChecks()
    {
        using (var server = new LoopbackWebSocketServer(0, () => message => GatewayReply.Message(message)))
        {
            ExpectClose(server.Port, Frame(0x81, Encoding.UTF8.GetBytes("unmasked"), false), 1002);
            ExpectClose(server.Port, Frame(0xc1, new byte[0], true), 1002);
            ExpectClose(server.Port, Frame(0x80, new byte[0], true), 1002);
            ExpectClose(server.Port, Frame(0x82, new byte[] {1}, true), 1003);
            ExpectClose(server.Port, Frame(0x81, new byte[] {0xff}, true), 1007);
            ExpectClose(server.Port, Frame(0x09, new byte[] {1}, true), 1002);
            ExpectClose(server.Port, Frame(0x88, new byte[] {1}, true), 1002);
            ExpectClose(server.Port, Frame(0x88, new byte[] {3, 237}, true), 1002); // reserved 1005
            ExpectClose(server.Port, new byte[] {0x81,0xfe,0,1}, 1002); // non-minimal length
            ExpectClose(server.Port, new byte[] {0x81,0xff,0,0,0,0,0,0x20,0,1}, 1009); // 2 MiB + 1
            using (var client = Raw(server.Port))
            {
                var stream = client.GetStream();
                // UTF-8 character split over continuation frames; a ping is interleaved.
                foreach (byte[] frame in new[] { Frame(0x01,new byte[]{0xe8},true), Frame(0x89,new byte[]{42},true),
                    Frame(0x80,new byte[]{0xb5,0x9b},true) }) stream.Write(frame,0,frame.Length);
                Check(ServerFrame(stream,10)[0] == 42, "interleaved pong");
                Check(Encoding.UTF8.GetString(ServerFrame(stream,1)) == "赛", "fragmented UTF-8");
                Thread.Sleep(11000);
                byte[] idleEcho = Frame(0x81, Encoding.UTF8.GetBytes("after-idle"), true);
                stream.Write(idleEcho, 0, idleEcho.Length);
                Check(Encoding.UTF8.GetString(ServerFrame(stream,1)) == "after-idle", "HTTP handshake timeout leaked into established WebSocket");
                byte[] close = Frame(0x88,new byte[]{3,232},true); stream.Write(close,0,close.Length);
                Check(ServerFrame(stream,8)[1] == 232, "normal close handshake");
            }
        }
    }
    static void LocalAccountChecks()
    {
        var basic = Settings();
        var s = new LocalAccountSettings { TransportKey = basic.TransportKey, HashSecret = basic.HashSecret,
            PreAuthKey = basic.PreAuthKey, ExpectedClientVersion = basic.ExpectedClientVersion,
            AccountPlatform = 2, Port = basic.Port, RunSeconds = 30,
            LocalAccountId = "100000000000000001", ProtocolHash = new string('a', 64) };
        string firstToken = null;
        var documents = new[] { new DynamicDataDocument { Name = "example.json", Content = "{}" } };
        var catalog = new DynamicDataCatalog(documents);
        Dictionary<string, object> dataReply;
        Check(catalog.TryReply("example.json", 1, out dataReply) &&
            (string)dataReply["hash"] == "44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",
            "known independent document hash");
        Check(!catalog.TryReply("../example.json", 1, out dataReply) && !catalog.TryReply("absent", 1, out dataReply), "no path or fallback lookup");
        bool duplicateRejected = false;
        try { new DynamicDataCatalog(new[] { documents[0], documents[0] }); }
        catch (ArgumentException) { duplicateRejected = true; }
        Check(duplicateRejected, "duplicate catalog key");
        for (int run = 0; run < 2; run++)
        {
            var session = new LocalAccountSession(s, catalog);
            Check(session.Handle(Encode(Packet(s), s.HashSecret, s.TransportKey)).CloseCode == 0, "local preauth");
            Check(session.Handle(Encode(new { _type = "PqAuthComplete", reqId = 18 }, s.HashSecret, s.TransportKey)).CloseCode == 1008,
                "cannot skip local identity");
            Check(session.Handle(Encode(new { _type = "PqAuthNxA", reqId = 18 }, s.HashSecret, s.TransportKey)).CloseCode == 1008,
                "missing ticket must fail without advancing");
            string[] requests = { "PqAuthNxA", "PqAccountLinkCheck", "PqAuthComplete" };
            string[] replies = { "PrAuth", "PrAccountLinkCheck", "PrAuthComplete" };
            for (int step = 0; step < 3; step++)
            {
                string wire = Encode(new { _type = requests[step], reqId = 18 + step, ticket = "SYNTHETIC_LOCAL_TICKET" }, s.HashSecret, s.TransportKey);
                var response = session.Handle(wire);
                Check(response.CloseCode == 0, "local account step " + step);
                WireDecodeResult decoded; string error;
                Check(WireCodec.TryDecode(response.Text, s.TransportKey, null, out decoded, out error), "local reply envelope");
                var packet = (Dictionary<string, object>)Json.DeserializeObject(decoded.Json);
                Check((string)packet["_type"] == replies[step] && (int)packet["reqId"] == 18 + step, "local reply correlation");
                Check(decoded.Json.IndexOf("SYNTHETIC_LOCAL_TICKET", StringComparison.Ordinal) < 0, "ticket must not leak into reply");
                if (step == 0)
                {
                    string token = (string)packet["token"];
                    Check(Convert.FromBase64String(token).Length == 32 && token != firstToken, "per-session random identity token");
                    firstToken = token;
                }
                if (step == 2) Check((string)packet["accountId"] == s.LocalAccountId, "local account mapping");
                Check(session.Handle(wire).CloseCode != 0, "replayed request must fail");
            }
            Check(session.AccountReady, "local account complete");
            Check(session.Handle(Encode(new { _type = "PqGetPublicDynData", reqId = 21, name = "example.json" }, s.HashSecret, s.TransportKey)).Event == "dynamic_data_served",
                "local data after account auth");
            Check(session.Handle(Encode(new { _type = "PqLogin", reqId = 22 }, s.HashSecret, s.TransportKey)).Event == "profile_data_not_implemented",
                "must not claim profile or race readiness");
            Check(new LocalAccountSession(s).Handle(Encode(new { _type = "PqAuthNxA", reqId = 18, ticket = "SYNTHETIC" }, s.HashSecret, s.TransportKey)).CloseCode != 0,
                "another connection needs its own preauth");
        }
    }

    static int Main()
    {
        try
        {
            SessionChecks();
            LocalAccountChecks();
            var s = Settings(); int port;
            using (var server = new LoopbackWebSocketServer(0, () => new PreAuthSession(s).Handle))
            { port = server.Port; DotNetExchange(port,"/",s); DotNetExchange(port,"//",s); }
            var rebinder = new System.Net.Sockets.TcpListener(System.Net.IPAddress.Loopback,port);
            rebinder.Start(); rebinder.Stop();
            TransportChecks();
            Console.WriteLine("PASS encrypted preauth, local identity sequence and isolation, replay rejection, .NET WebSocket interoperability, RFC frame rejection, fragmentation and port cleanup");
            return 0;
        }
        catch (Exception error) { Console.Error.WriteLine(error.ToString()); return 1; }
    }
}
