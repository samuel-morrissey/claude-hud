"""Testes do recadeiro, rodados no PC: make test"""

import http.client
import pathlib
import sys
import threading
import unittest
from http.server import ThreadingHTTPServer

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / "relay"))
import server as relay  # noqa: E402

TOKEN = "t" * 40


class Clock:
    def __init__(self):
        self.now = 1_800_000_000.0

    def __call__(self):
        return self.now


def drain(listener):
    events = []
    while not listener.empty():
        events.append(listener.get_nowait())
    return events


class HubTest(unittest.TestCase):
    def setUp(self):
        self.clock = Clock()
        self.hub = relay.Hub(self.clock)

    def test_ouvinte_novo_recebe_reset_e_o_estado_atual(self):
        self.hub.publish("1000 cloud working abc")
        self.hub.publish("1000 pc thinking def")
        self.assertEqual(drain(self.hub.subscribe()), ["reset", "cloud working abc", "pc thinking def"])

    def test_entrega_eventos_novos_a_quem_ja_ouve(self):
        listener = self.hub.subscribe()
        drain(listener)
        self.hub.publish("1000 cloud working abc")
        self.assertEqual(drain(listener), ["cloud working abc"])

    def test_start_vira_idle(self):
        listener = self.hub.subscribe()
        drain(listener)
        self.hub.publish("1000 pc start abc")
        self.assertEqual(drain(listener), ["pc idle abc"])

    def test_recusa_linha_invalida(self):
        for line in ["", "pc working abc", "1000 pc working", "1000 tv working abc", "1000 pc dancing abc", "x pc idle a"]:
            self.assertFalse(self.hub.publish(line), line)
        self.assertEqual(drain(self.hub.subscribe()), ["reset"])

    def test_descarta_evento_que_chega_atrasado(self):
        listener = self.hub.subscribe()
        drain(listener)
        self.hub.publish("2000 pc done abc")
        self.assertTrue(self.hub.publish("1500 pc thinking abc"))
        self.assertEqual(drain(listener), ["pc done abc"])

    def test_evento_atrasado_nao_reabre_sessao_encerrada(self):
        self.hub.publish("1000 pc working abc")
        self.hub.publish("3000 pc end abc")
        self.hub.publish("2000 pc thinking abc")
        self.assertEqual(drain(self.hub.subscribe()), ["reset"])

    def test_sessao_encerrada_pode_voltar_com_evento_mais_novo(self):
        self.hub.publish("3000 pc end abc")
        self.hub.publish("4000 pc working abc")
        self.assertEqual(drain(self.hub.subscribe()), ["reset", "pc working abc"])

    def test_termino_antigo_aparece_como_parado_para_quem_chega(self):
        self.hub.publish("1000 pc done abc")
        self.clock.now += 5
        self.assertEqual(drain(self.hub.subscribe()), ["reset", "pc done abc"])
        self.clock.now += 60
        self.assertEqual(drain(self.hub.subscribe()), ["reset", "pc idle abc"])

    def test_sessao_sem_eventos_expira(self):
        self.hub.publish("1000 pc working abc")
        self.clock.now += 31 * 60
        self.assertEqual(drain(self.hub.subscribe()), ["reset"])

    def test_ouvinte_que_saiu_nao_recebe_mais(self):
        listener = self.hub.subscribe()
        drain(listener)
        self.hub.unsubscribe(listener)
        self.hub.publish("1000 pc working abc")
        self.assertEqual(drain(listener), [])


class HttpTest(unittest.TestCase):
    def setUp(self):
        self.hub = relay.Hub()
        self.server = ThreadingHTTPServer(("127.0.0.1", 0), relay.make_handler(self.hub, TOKEN))
        self.server.daemon_threads = True
        threading.Thread(target=self.server.serve_forever, daemon=True).start()
        self.addCleanup(self.server.server_close)
        self.addCleanup(self.server.shutdown)

    def request(self, method, path, body=None, token=TOKEN):
        connection = http.client.HTTPConnection("127.0.0.1", self.server.server_port, timeout=5)
        headers = {"Authorization": f"Bearer {token}"} if token else {}
        connection.request(method, path, body=body, headers=headers)
        return connection, connection.getresponse()

    def test_healthz_dispensa_token(self):
        _, response = self.request("GET", "/healthz", token=None)
        self.assertEqual(response.status, 200)

    def test_recusa_sem_token_ou_com_token_errado(self):
        for token in [None, "errado"]:
            _, response = self.request("POST", "/events", body="1 pc idle a", token=token)
            self.assertEqual(response.status, 401)
            _, response = self.request("GET", "/stream", token=token)
            self.assertEqual(response.status, 401)

    def test_post_registra_e_stream_entrega(self):
        _, response = self.request("POST", "/events", body="1000 cloud working abc\n")
        self.assertEqual(response.status, 204)

        connection, response = self.request("GET", "/stream")
        self.addCleanup(connection.close)
        self.assertEqual(response.status, 200)
        self.assertEqual(response.getheader("Content-Type"), "text/event-stream")
        self.assertEqual(response.fp.readline(), b"data: reset\n")
        self.assertEqual(response.fp.readline(), b"\n")
        self.assertEqual(response.fp.readline(), b"data: cloud working abc\n")
        self.assertEqual(response.fp.readline(), b"\n")

        self.request("POST", "/events", body="2000 cloud done abc")
        self.assertEqual(response.fp.readline(), b"data: cloud done abc\n")

    def test_post_invalido_devolve_400(self):
        _, response = self.request("POST", "/events", body="lixo")
        self.assertEqual(response.status, 400)

    def test_post_grande_demais_devolve_413(self):
        _, response = self.request("POST", "/events", body="x" * 2000)
        self.assertEqual(response.status, 413)


if __name__ == "__main__":
    unittest.main()
