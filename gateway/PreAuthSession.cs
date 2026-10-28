using System;
using System.Collections.Generic;
using System.Web.Script.Serialization;

namespace TwinKartLauncher.Gateway
{
    // Runtime configuration stays outside the published source. TransportKey,
    // HashSecret and PreAuthKey have different roles and must not be conflated.
    public class PreAuthSettings
    {
        public string TransportKey { get; set; }
        public string HashSecret { get; set; }
        public string PreAuthKey { get; set; }
        public string ExpectedClientVersion { get; set; }
        public int AccountPlatform { get; set; }
        public int Port { get; set; }
        public int RunSeconds { get; set; }

        public void Validate()
        {
            byte[] key;
            try { key = Convert.FromBase64String(TransportKey ?? ""); }
            catch (FormatException) { throw new ArgumentException("Invalid transport key format"); }
            if (key.Length != 32) throw new ArgumentException("Transport key must decode to 32 bytes");
            Array.Clear(key, 0, key.Length);
            if (!ValidText(HashSecret, 4096) || !ValidText(PreAuthKey, 4096))
                throw new ArgumentException("Explicit hash and pre-auth configuration required");
            if (!ValidText(ExpectedClientVersion, 256))
                throw new ArgumentException("Explicit client version required");
            if (AccountPlatform < 1 || AccountPlatform > 7)
                throw new ArgumentException("Explicit supported account platform required");
            if (Port < 1024 || Port > 65535 || RunSeconds < 1 || RunSeconds > 3600)
                throw new ArgumentException("Invalid listener port or run duration");
        }

        internal static bool ValidText(string text, int maximum)
        {
            return !string.IsNullOrEmpty(text) && text.Length <= maximum && text.IndexOf('\0') < 0;
        }
    }

    public sealed class GatewayReply
    {
        public string Text { get; private set; }
        public ushort CloseCode { get; private set; }
        public string Event { get; private set; }
        public string[] Notices { get; private set; }
        public static GatewayReply Message(string text, string eventName = "preauth_accepted")
        { return new GatewayReply { Text = text, Event = eventName }; }
        public static GatewayReply Messages(string text, string[] notices, string eventName)
        { return new GatewayReply { Text = text, Notices = notices, Event = eventName }; }
        public static GatewayReply Close(ushort code, string reason)
        { return new GatewayReply { CloseCode = code, Event = reason }; }
    }

    // This intentionally stops before account authentication. A PrPacket result
    // of zero acknowledges pre-auth only; it does not create an account or race.
    public sealed class PreAuthSession
    {
        readonly string transportKey, hashSecret, preAuthKey, version;
        readonly int accountPlatform;
        bool accepted;
        public bool PreAuthAccepted { get { return accepted; } }

        public PreAuthSession(PreAuthSettings settings)
        {
            settings.Validate();
            transportKey = settings.TransportKey; hashSecret = settings.HashSecret;
            preAuthKey = settings.PreAuthKey; version = settings.ExpectedClientVersion;
            accountPlatform = settings.AccountPlatform;
        }

        static bool EqualSecret(string left, string right)
        {
            if (left == null || right == null || left.Length != right.Length) return false;
            int difference = 0;
            for (int i = 0; i < left.Length; i++) difference |= left[i] ^ right[i];
            return difference == 0;
        }

        static object Get(IDictionary<string, object> packet, string key)
        { object value; return packet.TryGetValue(key, out value) ? value : null; }

        public GatewayReply Handle(string wire)
        {
            WireDecodeResult decoded; string error;
            if (!WireCodec.TryDecode(wire, transportKey, hashSecret, out decoded, out error) ||
                !decoded.WasEncrypted || decoded.HashStatus != WireHashStatus.Verified)
                return GatewayReply.Close(1008, "invalid_envelope");
            IDictionary<string, object> packet;
            var json = new JavaScriptSerializer { MaxJsonLength = WireCodec.MaxJsonBytes, RecursionLimit = 24 };
            try { packet = json.DeserializeObject(decoded.Json) as IDictionary<string, object>; }
            catch (ArgumentException) { return GatewayReply.Close(1008, "invalid_packet"); }
            catch (InvalidOperationException) { return GatewayReply.Close(1008, "invalid_packet"); }
            if (packet == null) return GatewayReply.Close(1008, "invalid_packet");
            string type = Get(packet, "_type") as string;
            if (type != "PqPreAuth")
                return GatewayReply.Close(1003, accepted ? "account_auth_not_implemented" : "preauth_required");
            if (accepted) return GatewayReply.Close(1008, "preauth_already_accepted");
            object id = Get(packet, "reqId");
            object account = Get(packet, "accountPlatform"), platform = Get(packet, "platform");
            if (!(id is int) || (int)id < 1 || !(account is int) || (int)account != accountPlatform ||
                !(platform is int) || (int)platform != 1 ||
                !EqualSecret(Get(packet, "key") as string, preAuthKey) ||
                !string.Equals(Get(packet, "clientVer") as string, version, StringComparison.Ordinal) ||
                !PreAuthSettings.ValidText(Get(packet, "language") as string, 32) ||
                !PreAuthSettings.ValidText(Get(packet, "culture") as string, 64))
                return GatewayReply.Close(1008, "preauth_configuration_mismatch");
            var response = new Dictionary<string, object> {
                { "_type", "PrPacket" }, { "reqId", id },
                { "result", new Dictionary<string, object> { { "code", 0 } } }, { "desc", "" }
            };
            string reply;
            if (!WireCodec.TryEncode(json.Serialize(response), null, transportKey, out reply, out error))
                return GatewayReply.Close(1011, "response_encoding_failed");
            accepted = true;
            return GatewayReply.Message(reply);
        }
    }
}
