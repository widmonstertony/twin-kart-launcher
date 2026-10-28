# Loopback protocol probe

`protocol_probe.py` is an original, byte-transparent recorder for a later controlled original-client test. It accepts connections only on `127.0.0.1` and forwards them unchanged to an explicitly chosen port on `127.0.0.1`. It never starts the game or upstream server.

For the prepared Drift route, first start the local reference server on port 59874, then start the probe:

```powershell
python gateway/protocol_probe.py --listen-port 59875 --upstream-port 59874 --log probe-run.jsonl
```

Route the original client to `ws://127.0.0.1:59875` through the existing, reversible `Game.ini` setting for `[/Script/KartDrift.KdServerConfig]`, `ServerAddrs=(Name="kd-live",HttpUrl="...")`. The `PreAuthKey` is a sensitive input and must not be copied into source, command-line arguments or probe logs. The actual local SDK auth shim remains outside this project's current original implementation. Use a unique log filename for each run; the probe appends JSON lines.

The recorder observes HTTP method/status, Upgrade presence, numeric Content-Length, and only the presence of subprotocol/extensions. It does **not** log URLs, paths, query strings, header values or WebSocket keys. For WebSocket frames, it records direction, order, text/binary/control kind, mask/compression flags and payload length. Uncompressed text frames that decode as JSON also get a known packet `_type`, type-only field shape and an anonymous request-ID alias (`R0001`, etc.) for request/reply correlation. The actual `reqId` and all field values are omitted. Unknown packet names and field names are not written verbatim. Binary or compressed messages stay opaque; a missing JSON shape on those frames is not a protocol failure.

The parser caps each inspected HTTP header at 16 KiB and each inspected message at 1 MiB. Larger packets still forward unchanged, but their contents are not inspected. The forwarder does not decrypt TLS or a game's own encrypted binary payload, decompress WebSocket extensions, synthesize responses, or change frame timing intentionally. Cross-direction log order is thread scheduling order and should be treated as approximate; order within each direction is preserved.

Synthetic socket verification is in `tests/test_protocol_probe.py`. It checks unchanged forwarding of a fragmented masked client message, a masked binary message, a server reply/push/binary sequence, anonymous `reqId` correlation, and absence of secret values from the log. It does **not** count as a live game handshake. On the current Codex sandbox, even `127.0.0.1` socket tests require the local sandbox escalation; no Internet access is used.

The purpose of this capture is to establish the actual client wire envelope and authentication/room message shape. The probe alone cannot authenticate, open a room or enter a race.
