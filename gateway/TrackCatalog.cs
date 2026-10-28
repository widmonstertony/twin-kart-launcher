using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;

namespace TwinKartLauncher.Gateway
{
    // No license, season, currency or record requirement. IDs are external facts;
    // catalog membership does not prove that the corresponding map is installed.
    public sealed class TrackCatalog
    {
        readonly List<string> tracks = new List<string>();
        readonly HashSet<string> known = new HashSet<string>(StringComparer.Ordinal);
        static readonly Regex Id = new Regex(@"^[A-Za-z][A-Za-z0-9_]{0,95}$", RegexOptions.CultureInvariant);
        public TrackCatalog(string catalogJson)
        {
            var json = new JavaScriptSerializer { MaxJsonLength = 4 * 1024 * 1024 };
            var document = json.DeserializeObject(catalogJson) as Dictionary<string, object>;
            object raw;
            if (document == null || !document.TryGetValue("tracks", out raw) || !(raw is object[])) throw new FormatException("Expected tracks array.");
            var ids = (object[])raw;
            if (ids.Length == 0 || ids.Length > 10000) throw new FormatException("Invalid track count.");
            foreach (object value in ids)
            {
                string id = value as string;
                if (id == null || !Id.IsMatch(id)) throw new FormatException("Invalid track identifier.");
                if (known.Add(id)) tracks.Add(id);
            }
        }
        public string[] All { get { return tracks.ToArray(); } }
        public bool Contains(string id) { return id != null && known.Contains(id); }
        public string TimeAttackRows(ISet<string> personalBestTracks)
        {
            if (personalBestTracks == null) throw new ArgumentNullException("personalBestTracks");
            var rows = new List<object>();
            foreach (string id in tracks)
                rows.Add(new Dictionary<string, object> { { "trackId", id }, { "count", personalBestTracks.Contains(id) ? 1 : 0 } });
            return new JavaScriptSerializer().Serialize(rows);
        }
    }
}
