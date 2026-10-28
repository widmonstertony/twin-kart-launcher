using System;
using System.Collections.Generic;
using System.Security.Cryptography;
using System.Web.Script.Serialization;

namespace TwinKartLauncher.Gateway
{
    public sealed class LocalAccountSettings : PreAuthSettings
    {
        public string LocalAccountId { get; set; }
        public string ProtocolHash { get; set; }
        public string DynamicDataPath { get; set; }
        public string ProfileDataDirectory { get; set; }
        public string ProfilePath { get; set; }
        public string TuningPlanPath { get; set; }

        public void ValidateLocalAccount()
        {
            Validate();
            ulong account;
            if (!ulong.TryParse(LocalAccountId, out account) || account == 0 ||
                account.ToString(System.Globalization.CultureInfo.InvariantCulture) != LocalAccountId)
                throw new ArgumentException("Explicit local account identifier required");
            if (ProtocolHash == null || ProtocolHash.Length != 64)
                throw new ArgumentException("Explicit client protocol hash required");
            foreach (char c in ProtocolHash)
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
                    throw new ArgumentException("Invalid protocol hash");
            if (AccountPlatform != 2)
                throw new ArgumentException("Only the observed Nexon Arena offline flow is implemented");
        }
    }

    // Local identity emulation, never validation of a live Nexon account.
    // Packet field shapes were observed at the loopback client boundary.
    // No response templates or third-party server implementation are embedded.
    public sealed class LocalAccountSession
    {
        readonly PreAuthSession preauth;
        readonly DynamicDataCatalog dynamicData;
        readonly OfflineProfileStore profile;
        bool profileLoggedIn;
        readonly string key, hashSecret, account, protocolHash, token;
        readonly JavaScriptSerializer json = new JavaScriptSerializer {
            MaxJsonLength = WireCodec.MaxJsonBytes, RecursionLimit = 24 };
        int stage, lastRequest;
        public bool AccountReady { get { return stage == 4; } }

        public LocalAccountSession(LocalAccountSettings settings, DynamicDataCatalog catalog = null, OfflineProfileStore profileStore = null)
        {
            settings.ValidateLocalAccount();
            preauth = new PreAuthSession(settings);
            dynamicData = catalog;
            profile = profileStore;
            key = settings.TransportKey; hashSecret = settings.HashSecret;
            account = settings.LocalAccountId; protocolHash = settings.ProtocolHash;
            byte[] random = new byte[32];
            using (var rng = RandomNumberGenerator.Create()) rng.GetBytes(random);
            token = Convert.ToBase64String(random);
            Array.Clear(random, 0, random.Length);
        }

        static object Field(IDictionary<string, object> packet, string name)
        { object value; return packet.TryGetValue(name, out value) ? value : null; }

        public GatewayReply Handle(string wire)
        {
            WireDecodeResult decoded; string error;
            if (!WireCodec.TryDecode(wire, key, hashSecret, out decoded, out error) ||
                !decoded.WasEncrypted || decoded.HashStatus != WireHashStatus.Verified)
                return GatewayReply.Close(1008, "invalid_envelope");
            IDictionary<string, object> request;
            try { request = json.DeserializeObject(decoded.Json) as IDictionary<string, object>; }
            catch (ArgumentException) { return GatewayReply.Close(1008, "invalid_packet"); }
            catch (InvalidOperationException) { return GatewayReply.Close(1008, "invalid_packet"); }
            if (request == null) return GatewayReply.Close(1008, "invalid_packet");
            object idValue = Field(request, "reqId");
            if (!(idValue is int) || (int)idValue <= lastRequest)
                return GatewayReply.Close(1008, "invalid_request_sequence");
            int id = (int)idValue;
            string type = Field(request, "_type") as string;
            if (stage == 0)
            {
                GatewayReply result = preauth.Handle(wire);
                if (result.CloseCode == 0) { stage = 1; lastRequest = id; }
                return result;
            }
            if (stage == 4)
            {
                if (type != "PqGetPublicDynData")
                {
                    if (profile == null) return GatewayReply.Close(1003, "profile_data_not_implemented");
                    if (!profileLoggedIn && type != "PqLogin") return GatewayReply.Close(1008, "profile_login_required");
                    if (profileLoggedIn && type == "PqLogin") return GatewayReply.Close(1008, "profile_already_logged_in");
                    ProfileResult handled = profile.Handle(request, id);
                    string replyWire;
                    if (!WireCodec.TryEncode(json.Serialize(handled.Response), null, key, out replyWire, out error)) return GatewayReply.Close(1011, "profile_encoding_failed");
                    var pushes = new List<string>();
                    foreach (var notice in handled.Notices)
                    {
                        string encodedNotice;
                        if (!WireCodec.TryEncode(json.Serialize(notice), null, key, out encodedNotice, out error)) return GatewayReply.Close(1011, "notice_encoding_failed");
                        pushes.Add(encodedNotice);
                    }
                    lastRequest = id;
                    if (type == "PqLogin" && (int)((Dictionary<string,object>)handled.Response["result"])["code"] == 0) profileLoggedIn = true;
                    return GatewayReply.Messages(replyWire, pushes.ToArray(), "profile_" + type + "_code_" + ((Dictionary<string,object>)handled.Response["result"])["code"]);
                }
                Dictionary<string, object> document;
                if (dynamicData == null || !dynamicData.TryReply(Field(request, "name") as string, id, out document))
                {
                    if (profile == null) return GatewayReply.Close(1003, "dynamic_data_unavailable");
                    document = new Dictionary<string,object> {
                        {"_type","PrGetPublicDynData"},{"reqId",id},{"result",new Dictionary<string,object>{{"code",10014}}},
                        {"desc",""},{"name",Field(request,"name")??""},{"hash",new string('0',64)},
                        {"hasData",false},{"isBinary",false},{"data",""},{"isCompressed",false},{"uncompressedSize",0},{"compressedData",""}
                    };
                }
                string dataWire;
                if (!WireCodec.TryEncode(json.Serialize(document), null, key, out dataWire, out error))
                    return GatewayReply.Close(1011, "response_encoding_failed");
                lastRequest = id;
                return GatewayReply.Message(dataWire, "dynamic_data_served");
            }
            string expected = stage == 1 ? "PqAuthNxA" : stage == 2 ? "PqAccountLinkCheck" : "PqAuthComplete";
            if (type != expected) return GatewayReply.Close(1008, "unexpected_auth_step");
            var response = new Dictionary<string, object> {
                { "_type", stage == 1 ? "PrAuth" : stage == 2 ? "PrAccountLinkCheck" : "PrAuthComplete" },
                { "reqId", id }, { "result", new Dictionary<string, object> { { "code", 0 } } }, { "desc", "" }
            };
            if (stage == 1)
            {
                if (!PreAuthSettings.ValidText(Field(request, "ticket") as string, 8192))
                    return GatewayReply.Close(1008, "missing_local_sdk_ticket");
                response.Add("addr", ""); response.Add("token", token);
                response.Add("bNew", false); response.Add("protocolHash", protocolHash);
                response.Add("userIp", "127.0.0.1"); response.Add("bChangeName", false);
            }
            else if (stage == 2)
            {
                response.Add("bExistGlobalNpsn", true); response.Add("bAccountLinkGuest", false);
                response.Add("withdrawExpiresSec", 0); response.Add("country", "");
            }
            else
            {
                response.Add("accountId", account); response.Add("bNew", false);
                response.Add("bNeedSet2ndPwd", false); response.Add("bNeed2ndPwd", false);
                response.Add("pwdTryCountLeft", 0); response.Add("bChangeName", false);
            }
            string encoded;
            if (!WireCodec.TryEncode(json.Serialize(response), null, key, out encoded, out error))
                return GatewayReply.Close(1011, "response_encoding_failed");
            stage++; lastRequest = id;
            return GatewayReply.Message(encoded, stage == 2 ? "local_identity_issued" :
                stage == 3 ? "local_link_checked" : "local_account_ready");
        }
    }
}
