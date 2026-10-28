"""Loopback-only, byte-transparent HTTP/WebSocket protocol shape recorder.

This is an original diagnostic transport adapter. It forwards bytes unchanged
to an explicitly selected 127.0.0.1 upstream. It never writes frame bodies,
tokens, tickets, header values, paths or query strings to its JSONL log.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import secrets
import socket
import socketserver
import threading
import time


MAX_HTTP_HEADER = 16 * 1024
MAX_INSPECT_MESSAGE = 1024 * 1024
MAX_SHAPE_FIELDS = 48

# Names already established by this machine's reflected client metadata or
# sanitized local traffic. Unknown names are counted, never written verbatim.
KNOWN_TYPES = frozenset(
    "PqPreAuth PrPacket PqAuthNxA PrAuth PqAccountLinkCheck PrAccountLinkCheck "
    "PqAuthComplete PrAuthComplete PqGetPublicDynData PrGetPublicDynData "
    "PqLogin PrLogin PqInventory PrInventory PqQuest PrQuest PqLoginComplete "
    "PqCreateCustomGame PqJoinCustomGame PcPartyCreateJoinCustomGame PcCustomGame "
    "PqAddCustomGameAiMember PqPlayCustomGame PcCustomGamePrepareSession PcHostGame "
    "PqTimeAttackPrepare PqTimeAttackStart PqSetClientStage PrSetClientStage "
    "PqGetServerEndpoint PrGetServerEndpoint PqGfsTicket PrGfsTicket PqEcho PrEcho "
    "PqTimeAttackGetRecords PrTimeAttackGetRecords PqTimeAttackGetRankers "
    "PrTimeAttackGetRankers PqServerTime PrServerTime PqLogout".split()
)
SAFE_FIELDS = frozenset(
    "_type reqId result code desc setting observerMode hideGameKey sessionId "
    "infoValid info slot hostMode gameMode raceMode profile track lap levelOption "
    "customGameOption gameId players members ai accountId bossAccountId gameKey "
    "maxMemberCount session sessionReqId abortedSessionReqId gameNum joinId "
    "observerMode observers gameOption gameData trackSet trackSetDone bfTrack "
    "clientVer accountPlatform language culture ticket token packetId addr bNew "
    "protocolHash userIp bChangeName InRiderName RemainReserveTime "
    "sessionAPIEndpoint sessionAPIKey sessionAPIAuthToken redisChannelName etc "
    "isSpectator name hasData data".split()
)


def value_kind(value: object) -> str:
    if value is None:
        return "null"
    if isinstance(value, bool):
        return "bool"
    if isinstance(value, int):
        return "int"
    if isinstance(value, float):
        return "float"
    if isinstance(value, str):
        return "string"
    if isinstance(value, list):
        return "array"
    if isinstance(value, dict):
        return "object"
    return "other"


class ShapeLog:
    def __init__(self, path: Path):
        path.parent.mkdir(parents=True, exist_ok=True)
        self._file = path.open("a", encoding="utf-8", newline="\n")
        self._lock = threading.Lock()
        self._salt = secrets.token_bytes(32)
        self._request_aliases: dict[bytes, str] = {}
        self._order = 0

    def close(self) -> None:
        with self._lock:
            self._file.close()

    def event(self, connection: str, direction: str, kind: str, **safe_fields: object) -> None:
        # Callers pass only generated enums/counts/booleans/known names.
        with self._lock:
            self._order += 1
            event = {"order": self._order, "connection": connection,
                     "direction": direction, "kind": kind}
            event.update(safe_fields)
            self._file.write(json.dumps(event, ensure_ascii=True, separators=(",", ":")) + "\n")
            self._file.flush()

    def alias(self, request_id: object) -> str:
        canonical = json.dumps(request_id, ensure_ascii=True, sort_keys=True,
                               separators=(",", ":")).encode("ascii")
        digest = hashlib.sha256(self._salt + canonical).digest()
        with self._lock:
            if digest not in self._request_aliases:
                self._request_aliases[digest] = "R%04d" % (len(self._request_aliases) + 1)
            return self._request_aliases[digest]

    def packet_shape(self, connection: str, direction: str, opcode: str,
                     payload: bytes, compressed: bool, oversized: bool,
                     payload_bytes: int, masked: bool) -> None:
        fields: dict[str, object] = {"frame_type": opcode,
                                     "payload_bytes": payload_bytes,
                                     "compressed": compressed,
                                     "oversized": oversized,
                                     "masked": masked}
        if opcode != "text" or compressed or oversized:
            fields["decoded_json"] = False
            self.event(connection, direction, "message", **fields)
            return
        try:
            root = json.loads(payload.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            fields["decoded_json"] = False
            self.event(connection, direction, "message", **fields)
            return
        fields["decoded_json"] = True
        fields["json_kind"] = value_kind(root)
        if isinstance(root, dict):
            packet_type = root.get("_type")
            fields["packet_type"] = packet_type if isinstance(packet_type, str) and packet_type in KNOWN_TYPES else "unrecognized"
            if "reqId" in root:
                fields["request_alias"] = self.alias(root["reqId"])
                fields["request_id_kind"] = value_kind(root["reqId"])
            shape: list[str] = []
            unknown = [0]
            self._shape(root, "", 0, shape, unknown)
            fields["shape"] = shape
            fields["unrecognized_field_count"] = unknown[0]
        self.event(connection, direction, "message", **fields)

    @staticmethod
    def _shape(value: dict[str, object], prefix: str, depth: int,
               shape: list[str], unknown: list[int]) -> None:
        if depth > 2:
            return
        for name, child in value.items():
            if len(shape) >= MAX_SHAPE_FIELDS:
                break
            if name not in SAFE_FIELDS:
                unknown[0] += 1
                continue
            path = prefix + name
            shape.append(path + ":" + value_kind(child))
            if isinstance(child, dict) and name in {"result", "setting", "gameMode", "info", "gameOption"}:
                ShapeLog._shape(child, path + ".", depth + 1, shape, unknown)


class WebSocketInspector:
    def __init__(self, log: ShapeLog, connection: str, direction: str):
        self.log, self.connection, self.direction = log, connection, direction
        self.buffer = bytearray()
        self.skip_remaining = 0
        self.skip_frame: tuple[int, bool, bool, bool, int] | None = None
        self.message_opcode: int | None = None
        self.message_payload = bytearray()
        self.message_size = 0
        self.message_compressed = False
        self.message_masked = False
        self.message_oversized = False

    def feed(self, data: bytes) -> None:
        self.buffer.extend(data)
        while True:
            if self.skip_remaining:
                amount = min(self.skip_remaining, len(self.buffer))
                del self.buffer[:amount]
                self.skip_remaining -= amount
                if self.skip_remaining:
                    return
                if self.skip_frame is not None:
                    opcode, fin, compressed, masked, size = self.skip_frame
                    self._frame(opcode, fin, compressed, masked, b"", size, True)
                    self.skip_frame = None
                continue
            if len(self.buffer) < 2:
                return
            first, second = self.buffer[0], self.buffer[1]
            fin, compressed = bool(first & 0x80), bool(first & 0x40)
            opcode, masked = first & 0x0F, bool(second & 0x80)
            size, header = second & 0x7F, 2
            if size == 126:
                if len(self.buffer) < 4:
                    return
                size, header = int.from_bytes(self.buffer[2:4], "big"), 4
            elif size == 127:
                if len(self.buffer) < 10:
                    return
                size, header = int.from_bytes(self.buffer[2:10], "big"), 10
            if masked:
                header += 4
            if len(self.buffer) < header:
                return
            mask = bytes(self.buffer[header - 4:header]) if masked else None
            if size > MAX_INSPECT_MESSAGE:
                del self.buffer[:header]
                self.skip_remaining = size
                self.skip_frame = (opcode, fin, compressed, masked, size)
                continue
            if len(self.buffer) < header + size:
                return
            payload = bytes(self.buffer[header:header + size])
            del self.buffer[:header + size]
            if mask is not None:
                payload = bytes(byte ^ mask[index & 3] for index, byte in enumerate(payload))
            self._frame(opcode, fin, compressed, masked, payload, size, False)

    def _frame(self, opcode: int, fin: bool, compressed: bool, masked: bool,
               payload: bytes, size: int, oversized: bool) -> None:
        if opcode in (8, 9, 10):
            kind = {8: "close", 9: "ping", 10: "pong"}[opcode]
            self.log.event(self.connection, self.direction, "control",
                           frame_type=kind, payload_bytes=size, masked=masked)
            return
        if opcode in (1, 2):
            if self.message_opcode is not None:
                self.log.event(self.connection, self.direction, "malformed_fragment",
                               payload_bytes=self.message_size)
            self.message_opcode = opcode
            self.message_payload.clear()
            self.message_size = 0
            self.message_compressed = compressed
            self.message_masked = masked
            self.message_oversized = False
        elif opcode != 0 or self.message_opcode is None:
            self.log.event(self.connection, self.direction, "unrecognized_frame",
                           opcode=opcode, payload_bytes=size, masked=masked)
            return
        self.message_size += size
        self.message_masked = self.message_masked or masked
        self.message_compressed = self.message_compressed or compressed
        self.message_oversized = self.message_oversized or oversized or self.message_size > MAX_INSPECT_MESSAGE
        if not self.message_oversized:
            self.message_payload.extend(payload)
        else:
            self.message_payload.clear()
        if fin:
            self.log.packet_shape(self.connection, self.direction,
                                  "text" if self.message_opcode == 1 else "binary",
                                  bytes(self.message_payload), self.message_compressed,
                                  self.message_oversized, self.message_size,
                                  self.message_masked)
            self.message_opcode = None
            self.message_payload.clear()
            self.message_size = 0


class HttpThenWebSocketInspector:
    def __init__(self, log: ShapeLog, connection: str, direction: str):
        self.log, self.connection, self.direction = log, connection, direction
        self.header = bytearray()
        self.disabled = False
        self.http_body_remaining = 0
        self.websocket = False
        self.ws = WebSocketInspector(log, connection, direction)

    def feed(self, data: bytes) -> None:
        if self.websocket:
            self.ws.feed(data)
            return
        if self.disabled:
            return
        self.header.extend(data)
        while True:
            if self.http_body_remaining:
                amount = min(self.http_body_remaining, len(self.header))
                del self.header[:amount]
                self.http_body_remaining -= amount
                if self.http_body_remaining:
                    return
            end = self.header.find(b"\r\n\r\n")
            if end < 0:
                if len(self.header) > MAX_HTTP_HEADER:
                    self.disabled = True
                    self.log.event(self.connection, self.direction, "http_header_too_large")
                    self.header.clear()
                return
            if end + 4 > MAX_HTTP_HEADER:
                self.disabled = True
                self.log.event(self.connection, self.direction, "http_header_too_large")
                self.header.clear()
                return
            header = bytes(self.header[:end + 4])
            del self.header[:end + 4]
            lines = header.decode("iso-8859-1", errors="replace").split("\r\n")
            first = lines[0].split(" ")
            names: dict[str, str] = {}
            for line in lines[1:]:
                if ":" in line:
                    name, value = line.split(":", 1)
                    names[name.strip().lower()] = value.strip()
            upgrade = "websocket" in names.get("upgrade", "").lower()
            length_raw = names.get("content-length", "")
            content_length = int(length_raw) if length_raw.isdecimal() and len(length_raw) < 12 else None
            if self.direction == "client_to_upstream":
                method = first[0] if first and first[0] in {"GET", "POST", "CONNECT"} else "other"
                self.websocket = upgrade and method == "GET"
                self.log.event(self.connection, self.direction, "http_handshake",
                               method=method,
                               upgrade_websocket=upgrade,
                               subprotocol_present="sec-websocket-protocol" in names,
                               extensions_present="sec-websocket-extensions" in names,
                               permessage_deflate="permessage-deflate" in names.get("sec-websocket-extensions", "").lower(),
                               content_length=content_length)
            else:
                status = int(first[1]) if len(first) >= 2 and first[1].isdigit() else None
                self.websocket = status == 101 and upgrade
                self.log.event(self.connection, self.direction, "http_handshake",
                               status=status,
                               upgrade_websocket=upgrade,
                               subprotocol_present="sec-websocket-protocol" in names,
                               extensions_present="sec-websocket-extensions" in names,
                               permessage_deflate="permessage-deflate" in names.get("sec-websocket-extensions", "").lower(),
                               content_length=content_length)
            if self.websocket:
                if self.header:
                    self.ws.feed(bytes(self.header))
                    self.header.clear()
                return
            if "chunked" in names.get("transfer-encoding", "").lower():
                self.disabled = True  # forward untouched; body boundaries are not inferred
                self.header.clear()
                return
            if content_length is not None:
                self.http_body_remaining = content_length
            elif self.direction == "upstream_to_client" and status not in {101, 204, 304}:
                self.disabled = True  # unknown response body length
                self.header.clear()
                return
            if not self.header:
                return


class ProbeServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True

    def __init__(self, listen_port: int, upstream_port: int, log: ShapeLog):
        if listen_port != 0 and listen_port == upstream_port:
            raise ValueError("Listen and upstream ports must differ")
        if not 1 <= upstream_port <= 65535 or not 0 <= listen_port <= 65535:
            raise ValueError("Invalid loopback port")
        self.upstream_port = upstream_port
        self.shape_log = log
        super().__init__(("127.0.0.1", listen_port), ProbeHandler)


class ProbeHandler(socketserver.BaseRequestHandler):
    def handle(self) -> None:
        connection = secrets.token_hex(4)
        log = self.server.shape_log
        try:
            upstream = socket.create_connection(("127.0.0.1", self.server.upstream_port), timeout=5)
        except OSError:
            log.event(connection, "local", "upstream_unavailable")
            return
        log.event(connection, "local", "connected")
        with upstream:
            client = self.request
            client.settimeout(1)
            upstream.settimeout(1)
            first = HttpThenWebSocketInspector(log, connection, "client_to_upstream")
            second = HttpThenWebSocketInspector(log, connection, "upstream_to_client")
            worker = threading.Thread(target=self._pipe,
                                      args=(client, upstream, first), daemon=True)
            worker.start()
            self._pipe(upstream, client, second)
            worker.join(timeout=2)
        log.event(connection, "local", "closed")

    @staticmethod
    def _pipe(source: socket.socket, destination: socket.socket,
              inspector: HttpThenWebSocketInspector) -> None:
        idle_deadline = time.monotonic() + 300
        try:
            while time.monotonic() < idle_deadline:
                try:
                    data = source.recv(16384)
                except socket.timeout:
                    continue
                if not data:
                    break
                destination.sendall(data)  # the only forwarding step; never transformed
                idle_deadline = time.monotonic() + 300
                try:
                    inspector.feed(data)
                except Exception:
                    inspector.log.event(inspector.connection, inspector.direction, "inspection_error")
        except OSError:
            pass
        try:
            destination.shutdown(socket.SHUT_WR)
        except OSError:
            pass


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--listen-port", type=int, default=59875)
    parser.add_argument("--upstream-port", type=int, required=True,
                        help="Explicit upstream port on 127.0.0.1")
    parser.add_argument("--log", type=Path, required=True,
                        help="JSONL output; contains only sanitized metadata")
    args = parser.parse_args()
    logger = ShapeLog(args.log)
    try:
        with ProbeServer(args.listen_port, args.upstream_port, logger) as server:
            print("Protocol probe listening on 127.0.0.1:%d and forwarding to 127.0.0.1:%d" %
                  (server.server_address[1], args.upstream_port), flush=True)
            server.serve_forever(poll_interval=0.2)
    except KeyboardInterrupt:
        pass
    finally:
        logger.close()


if __name__ == "__main__":
    main()
