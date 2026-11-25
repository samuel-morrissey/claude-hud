# claude-hud

Um visor de mesa que mostra o que o Claude Code está fazendo. Um bonequinho
laranja dorme, pensa, corre, pede atenção e comemora conforme o estado das
sessões abertas.

![Quadros de cada estado](docs/preview.png)

De cima para baixo: dormindo, parado, pensando, trabalhando, esperando você e
terminou. O ícone à direita diz de onde vem a sessão (PC, nuvem ou celular) e o
número, quantas sessões estão abertas.

## Hardware

- ESP32-C3 com USB nativo (USB-Serial/JTAG)
- Visor ST7735 de 96x54, SPI de 4 fios, sem CS nem RST ligados

| Sinal | GPIO |
| ----- | ---- |
| SCL   | 6    |
| SDA   | 5    |
| D/C   | 7    |

Pinos e calibração do recorte visível ficam em `src/config.h`.

Sem CS, o visor não tem como retomar o alinhamento dos bytes. Se a imagem
congelar, desligue a placa da energia por alguns segundos e ligue de novo.

## Como funciona

```
hooks do Claude Code ──POST /events──▶ recadeiro ──GET /stream──▶ placa (Wi-Fi)
   (PC, VPS, ...)                    (relay/server.py)
```

O Claude Code dispara hooks a cada mudança de estado. Cada hook chama
`host/claude-hud-hook`, que manda um evento ao recadeiro. A placa mantém uma
conexão aberta com o recadeiro e recebe os eventos conforme chegam. Assim a
placa só precisa de energia e Wi-Fi, e qualquer máquina com acesso à internet
pode aparecer no visor.

Um evento é uma linha:

```
<origem> <estado> [id]
```

- `origem`: `pc`, `cloud` ou `phone`
- `estado`: `start`, `idle`, `thinking`, `working`, `waiting`, `done` ou `end`
- `id`: identificador da sessão, até 11 caracteres

A placa guarda até 8 sessões e mostra a mais urgente: esperando você, depois
trabalhando, pensando, terminou e parado. Uma sessão sem eventos há 30 minutos
é descartada. Sem conexão com o recadeiro, aparecem barrinhas de sinal
riscadas no canto.

| Hook do Claude Code              | Estado     |
| -------------------------------- | ---------- |
| `SessionStart`                   | `start`    |
| `UserPromptSubmit`               | `thinking` |
| `PreToolUse`                     | `working`  |
| `PreToolUse` de pergunta ao usuário | `waiting` |
| `PostToolUse`                    | `thinking` |
| `Notification` de permissão      | `waiting`  |
| `Stop`                           | `done`     |
| `SessionEnd`                     | `end`      |

## Instalação

### 1. Recadeiro

Um servidor HTTP pequeno, sem dependências, em `relay/`. O `docker-compose.yml`
supõe um Traefik que publica serviços por labels e emite certificados Let's
Encrypt. A placa só confia nas raízes da Let's Encrypt (`src/roots.h`).

```sh
cd relay
cp .env.example .env    # RELAY_HOST e RELAY_TOKEN (openssl rand -hex 32)
docker compose up -d --build
```

### 2. Placa

Requer [PlatformIO](https://platformio.org/). Grave o firmware e depois a
configuração de rede, com a placa ligada por USB:

```sh
make flash
host/claude-hud-setup
```

O `claude-hud-setup` pergunta o nome e a senha do Wi-Fi (só 2,4 GHz) e lê o
endereço e o segredo do recadeiro de `~/.config/claude-hud/env`. Tudo fica
gravado na placa; depois disso ela pode ir para qualquer fonte USB.

No Linux, o usuário precisa de acesso à porta serial. Uma regra de udev resolve
para qualquer placa Espressif:

```sh
echo 'SUBSYSTEM=="tty", ATTRS{idVendor}=="303a", MODE="0666"' | sudo tee /etc/udev/rules.d/99-esp32.rules
sudo udevadm control --reload && sudo udevadm trigger --subsystem-match=tty
```

### 3. Cada máquina com Claude Code

Crie `~/.config/claude-hud/env`, com permissão 600:

```sh
CLAUDE_HUD_SOURCE=pc    # ou cloud, em um servidor
CLAUDE_HUD_URL=https://hud.example.com
CLAUDE_HUD_TOKEN=<o RELAY_TOKEN>
```

Copie os hooks de `host/hooks.example.json` para `~/.claude/settings.json`,
trocando o caminho pelo deste repositório. O script depende de `jq` e `curl`.

Para testar sem o Claude Code:

```sh
echo '{"session_id":"teste"}' | host/claude-hud-hook working
echo '{"session_id":"teste"}' | host/claude-hud-hook end
```

Sem `CLAUDE_HUD_URL`, o hook escreve direto na serial USB da placa, o que
dispensa recadeiro e Wi-Fi quando placa e Claude Code estão na mesma máquina.

## Serial

Com a placa no USB, a serial aceita os mesmos eventos e mais:

| Comando              | Efeito                                         |
| -------------------- | ---------------------------------------------- |
| `ping`               | responde `pong claude-hud`                     |
| `demo`               | liga/desliga um passeio por todos os estados   |
| `shot`               | devolve o quadro atual em hexadecimal          |
| `set <item> <valor>` | grava `ssid`, `pass`, `host` ou `token`        |
| `apply`              | reconecta com o que foi gravado                |
| `status`             | estado da rede, sem revelar senha nem segredo  |

## Desenvolvimento

O desenho (`src/render.cpp`) e o controle das sessões (`src/tracker.cpp`) não
dependem de Arduino e compilam no PC.

```sh
make test      # testes do controle de sessões e do recadeiro
make preview   # gera out/preview.png com quadros de cada estado
make firmware  # compila o firmware
```
