#!/usr/bin/env python3
"""Recadeiro do claude-hud.

Recebe os eventos dos hooks do Claude Code de qualquer máquina e os entrega à
placa, que fica ligada só ao Wi-Fi.

  POST /events   uma linha por evento: "<ms> <origem> <estado> <id>"
  GET  /stream   eventos em Server-Sent Events: "data: <origem> <estado> <id>"
  GET  /healthz  sem autenticação

As duas primeiras rotas pedem "Authorization: Bearer <RELAY_TOKEN>".

Quem se conecta em /stream recebe primeiro "data: reset" e o estado atual de
cada sessão, e depois os eventos conforme chegam.
"""

import hmac
import os
import queue
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

STATES = {"start", "idle", "thinking", "working", "waiting", "done", "end"}
SOURCES = {"pc", "cloud", "phone"}

# Mesmo prazo em que a placa dá uma sessão como morta.
STALE_SECONDS = 30 * 60
# A comemoração só vale para quem acabou de terminar.
DONE_SECONDS = 20
# Por quanto tempo um evento atrasado de uma sessão encerrada ainda é barrado.
TOMBSTONE_SECONDS = 60
PING_SECONDS = 15
MAX_BODY = 1024
MAX_ID = 11


class Hub:
    """Estado das sessões e entrega dos eventos a quem está ouvindo."""

    def __init__(self, clock=time.time):
        self._clock = clock
        self._lock = threading.Lock()
        self._sessions = {}  # (origem, id) -> [estado, ms do emissor, entrou no estado, último evento]
        self._ended = {}  # (origem, id) -> (ms do emissor, quando encerrou)
        self._listeners = set()

    def publish(self, line):
        """Registra um evento. Devolve False quando a linha é inválida."""
        parts = line.split()
        if len(parts) != 4 or not parts[0].isdigit():
            return False
        stamp, source, state, session = int(parts[0]), parts[1], parts[2], parts[3][:MAX_ID]
        if source not in SOURCES or state not in STATES:
            return False

        now = self._clock()
        key = (source, session)
        with self._lock:
            self._prune(now)
            # Os hooks rodam em paralelo e podem chegar fora de ordem. Vale a
            # hora de quem emitiu; o que chega atrasado é descartado.
            current = self._sessions.get(key)
            if current and stamp < current[1]:
                return True
            if key in self._ended and stamp <= self._ended[key][0]:
                return True

            if state == "end":
                self._sessions.pop(key, None)
                self._ended[key] = (stamp, now)
            else:
                if state == "start":
                    state = "idle"
                changed = current[2] if current and current[0] == state else now
                self._sessions[key] = [state, stamp, changed, now]

            event = f"{source} {state} {session}"
            for listener in list(self._listeners):
                try:
                    listener.put_nowait(event)
                except queue.Full:
                    self._listeners.discard(listener)
        return True

    def subscribe(self):
        """Devolve a fila de um novo ouvinte, já com o estado atual."""
        listener = queue.Queue(maxsize=256)
        now = self._clock()
        with self._lock:
            self._prune(now)
            listener.put_nowait("reset")
            for (source, session), (state, _stamp, changed, _updated) in sorted(self._sessions.items()):
                if state == "done" and now - changed > DONE_SECONDS:
                    state = "idle"
                listener.put_nowait(f"{source} {state} {session}")
            self._listeners.add(listener)
        return listener

    def unsubscribe(self, listener):
        with self._lock:
            self._listeners.discard(listener)

    def _prune(self, now):
        for key in [k for k, v in self._sessions.items() if now - v[3] > STALE_SECONDS]:
            del self._sessions[key]
        for key in [k for k, v in self._ended.items() if now - v[1] > TOMBSTONE_SECONDS]:
            del self._ended[key]


def make_handler(hub, token):
    class Handler(BaseHTTPRequestHandler):
        server_version = "claude-hud-relay"

        def log_message(self, *_args):
            pass

        def _authorized(self):
            given = self.headers.get("Authorization", "")
            return hmac.compare_digest(given.encode(), f"Bearer {token}".encode())

        def _reply(self, status, body=b""):
            self.send_response(status)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            if self.path == "/healthz":
                self._reply(200, b"ok\n")
            elif self.path != "/stream":
                self._reply(404)
            elif not self._authorized():
                self._reply(401)
            else:
                self._stream()

        def do_POST(self):
            if self.path != "/events":
                self._reply(404)
                return
            if not self._authorized():
                self._reply(401)
                return
            try:
                length = int(self.headers.get("Content-Length", ""))
            except ValueError:
                self._reply(411)
                return
            if length > MAX_BODY:
                self._reply(413)
                return
            lines = self.rfile.read(length).decode("utf-8", "replace").splitlines()
            lines = [line for line in lines if line.strip()]
            accepted = [hub.publish(line) for line in lines]
            self._reply(204 if accepted and all(accepted) else 400)

        def _stream(self):
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-cache")
            self.end_headers()
            listener = hub.subscribe()
            try:
                while True:
                    try:
                        chunk = f"data: {listener.get(timeout=PING_SECONDS)}\n\n"
                    except queue.Empty:
                        chunk = ": ping\n\n"
                    self.wfile.write(chunk.encode())
                    self.wfile.flush()
            except OSError:
                pass
            finally:
                hub.unsubscribe(listener)

    return Handler


def main():
    token = os.environ.get("RELAY_TOKEN", "")
    if len(token) < 32:
        raise SystemExit("RELAY_TOKEN precisa ter ao menos 32 caracteres")
    port = int(os.environ.get("PORT", "8787"))
    server = ThreadingHTTPServer(("0.0.0.0", port), make_handler(Hub(), token))
    server.daemon_threads = True
    server.serve_forever()


if __name__ == "__main__":
    main()
