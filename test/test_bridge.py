"""Testes da ponte, rodados no PC: make test"""

import importlib.machinery
import importlib.util
import pathlib
import unittest

path = pathlib.Path(__file__).resolve().parent.parent / "host" / "claude-hud-bridge"
loader = importlib.machinery.SourceFileLoader("bridge", str(path))
spec = importlib.util.spec_from_loader("bridge", loader)
bridge = importlib.util.module_from_spec(spec)
loader.exec_module(bridge)

NOW = 1_800_000_000


class TranslatorTest(unittest.TestCase):
    def setUp(self):
        self.translator = bridge.Translator()

    def test_repassa_evento_recente_sem_a_hora(self):
        self.assertEqual(self.translator.translate(f"{NOW} cloud working abc\n", NOW), "cloud working abc")

    def test_descarta_linha_malformada(self):
        for line in ["", "cloud working abc", f"{NOW} cloud working", f"{NOW} tv working abc", f"{NOW} cloud dancing abc"]:
            self.assertIsNone(self.translator.translate(line, NOW), line)

    def test_descarta_evento_mais_velho_que_o_prazo_da_placa(self):
        self.assertIsNone(self.translator.translate(f"{NOW - 31 * 60} cloud working abc", NOW))
        self.assertIsNotNone(self.translator.translate(f"{NOW - 29 * 60} cloud working abc", NOW))

    def test_termino_antigo_vira_parado(self):
        self.assertEqual(self.translator.translate(f"{NOW - 60} cloud done abc", NOW), "cloud idle abc")
        self.assertEqual(self.translator.translate(f"{NOW - 5} cloud done abc", NOW), "cloud done abc")

    def test_descarta_linha_anterior_a_ultima_vista(self):
        self.translator.translate(f"{NOW} cloud working abc", NOW)
        self.assertIsNone(self.translator.translate(f"{NOW - 10} cloud thinking abc", NOW))

    def test_aceita_eventos_no_mesmo_segundo(self):
        self.translator.translate(f"{NOW} cloud working abc", NOW)
        self.assertEqual(self.translator.translate(f"{NOW} cloud thinking abc", NOW), "cloud thinking abc")

    def test_encerra_as_sessoes_abertas_quando_a_conexao_cai(self):
        self.translator.translate(f"{NOW} cloud working abc", NOW)
        self.translator.translate(f"{NOW} cloud working def", NOW)
        self.translator.translate(f"{NOW} cloud end abc", NOW)
        self.assertEqual(self.translator.close_all(), ["cloud end def"])
        self.assertEqual(self.translator.close_all(), [])


if __name__ == "__main__":
    unittest.main()
