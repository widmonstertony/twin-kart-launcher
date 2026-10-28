"""Synthetic socket test: transparent forwarding and value-free JSONL logs."""

from __future__ import annotations

import json
from pathlib import Path
import socket
import sys
import threading


PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / "gateway"))
from protocol_probe import ProbeServer, ShapeLog  # noqa: E402


def receive_exact(sock: socket.socket, size: int) -> bytes:
    result = bytearray()
    while len(result) < size:
        chunk = sock.recv(size - len(result))
        if not chunk:
            raise AssertionError("socket closed before expected bytes arrived")
        result.extend(chunk)
    return bytes(result)


def receive_header(sock: socket.socket) -> bytes:
    result = bytearray()
    while not result.endswith(b"\r\n\r\n"):
        result.extend(receive_exact(sock, 1))
        if len(result) > 4096:
            raise AssertionError("unexpected long test header")
    return bytes(result)


def frame(payload: bytes, opcode: int, *, final: bool = True,
          mask: bool = False) -> bytes:
    first = opcode | (0x80 if final else 0)
    size = len(payload)
    second = 0x80 if mask else 0
    header = bytes([first, second | size]) if size < 126 else bytes([first, second | 126]) + size.to_bytes(2, "big")
    if not mask:
        return header + payload
    key = b"\x12\x34\x56\x78"
    return header + key + bytes(byte ^ key[index & 3] for index, byte in enumerate(payload))


def main() -> int:
    build = PROJECT / "build"
    build.mkdir(exist_ok=True)
    path = build / "protocol-probe-synthetic.jsonl"
    if path.exists():
        path.unlink()
    secrets = ["SECRET_QUERY_42", "SECRET_AUTH_99", "SECRET_CLIENT_TICKET",
               "SECRET_SERVER_KEY", "SECRET_ROOM_ID", "SECRET_SUBPROTOCOL"]
    request = (
        "GET /private?ticket=SECRET_QUERY_42 HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
        "Authorization: Bearer SECRET_AUTH_99\r\n"
        "Sec-WebSocket-Protocol: SECRET_SUBPROTOCOL\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n"
    ).encode("ascii")
    response = (
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Protocol: SECRET_SUBPROTOCOL\r\n"
        "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n"
    ).encode("ascii")
    request_json = json.dumps({"_type": "PqCreateCustomGame", "reqId": 42,
                               "ticket": "SECRET_CLIENT_TICKET",
                               "setting": {"_type": "CsRaceGame", "gameMode": {"raceMode": 4},
                                           "track": "Roulette_A01"}}, separators=(",", ":")).encode()
    midpoint = len(request_json) // 2
    request_frames = frame(request_json[:midpoint], 1, final=False, mask=True) + \
        frame(request_json[midpoint:], 0, mask=True)
    client_binary = frame(b"\x00SECRET_CLIENT_TICKET\xff", 2, mask=True)
    reply = frame(json.dumps({"_type": "PrPacket", "reqId": 42,
                              "result": {"code": 0},
                              "sessionAPIKey": "SECRET_SERVER_KEY"}, separators=(",", ":")).encode(), 1)
    push = frame(json.dumps({"_type": "PcCustomGame",
                             "sessionId": "SECRET_ROOM_ID", "infoValid": True},
                            separators=(",", ":")).encode(), 1)
    binary = frame(b"\x00SECRET_SERVER_KEY\xff", 2)

    upstream_listener = socket.socket()
    upstream_listener.bind(("127.0.0.1", 0))
    upstream_listener.listen(1)
    upstream_port = upstream_listener.getsockname()[1]
    observed: dict[str, bytes] = {}
    server_error: list[BaseException] = []

    def fake_upstream() -> None:
        try:
            with upstream_listener:
                conn, _ = upstream_listener.accept()
                with conn:
                    conn.settimeout(5)
                    observed["http"] = receive_header(conn)
                    conn.sendall(response[:13]); conn.sendall(response[13:])
                    observed["frames"] = receive_exact(conn, len(request_frames + client_binary))
                    conn.sendall(reply + push + binary)
        except BaseException as error:
            server_error.append(error)

    upstream_thread = threading.Thread(target=fake_upstream, daemon=True)
    upstream_thread.start()
    logger = ShapeLog(path)
    proxy = ProbeServer(0, upstream_port, logger)
    proxy_thread = threading.Thread(target=proxy.serve_forever, kwargs={"poll_interval": 0.05}, daemon=True)
    proxy_thread.start()
    try:
        with socket.create_connection(("127.0.0.1", proxy.server_address[1]), timeout=5) as client:
            client.settimeout(5)
            client.sendall(request[:11]); client.sendall(request[11:])
            assert receive_header(client) == response
            client.sendall(request_frames[:5]); client.sendall(request_frames[5:] + client_binary)
            assert receive_exact(client, len(reply + push + binary)) == reply + push + binary
        upstream_thread.join(timeout=5)
        assert not upstream_thread.is_alive() and not server_error, server_error
        assert observed["http"] == request, "HTTP Upgrade was modified"
        assert observed["frames"] == request_frames + client_binary, "masked/fragmented frames were modified"
    finally:
        proxy.shutdown()
        proxy.server_close()
        proxy_thread.join(timeout=5)
        logger.close()

    output = path.read_text(encoding="utf-8")
    for secret in secrets:
        assert secret not in output, "sensitive value leaked: " + secret
    events = [json.loads(line) for line in output.splitlines()]
    handshakes = [event for event in events if event["kind"] == "http_handshake"]
    assert len(handshakes) == 2 and handshakes[1]["status"] == 101
    assert handshakes[0]["subprotocol_present"] is True
    messages = [event for event in events if event["kind"] == "message"]
    create = next(event for event in messages if event.get("packet_type") == "PqCreateCustomGame")
    response_event = next(event for event in messages if event.get("packet_type") == "PrPacket")
    push_event = next(event for event in messages if event.get("packet_type") == "PcCustomGame")
    assert create["request_alias"] == response_event["request_alias"]
    assert push_event["order"] > response_event["order"], "push order was lost"
    assert "ticket:string" in create["shape"] and "setting.gameMode.raceMode:int" in create["shape"]
    assert any(event["frame_type"] == "binary" and not event["decoded_json"] for event in messages)
    path.unlink()
    print("PASS transparent masked/fragmented forwarding and redacted protocol shape log")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
