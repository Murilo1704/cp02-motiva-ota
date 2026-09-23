# CP02 – Projeto Motiva | Atualização Remota de Firmware (OTA)

Nó IoT de monitoramento de vegetação em rodovias, simulado com **ESP32 no Wokwi**, que é atualizado da versão **1.0** para a **2.0** pela Internet (OTA), sem intervenção local no equipamento.

**Turma 2CCR · Edge Computing · Prof. Marcelo Fernando Morgantini**

| Integrante | RM |
|---|---|
| Luiz Miguel | 562796 |
| Murilo Justino | 565470 |
| Stefanny Brum | 566216 |
| Enzo Hideki | 565052 |

- **Projeto Wokwi:** https://wokwi.com/projects/475999244753427457
- **Repositório OTA:** https://github.com/Murilo1704/cp02-motiva-ota

---

## 1. Arquitetura

```
┌─────────────────────┐   Wi-Fi     ┌──────────┐   HTTPS   ┌────────────────────────────┐
│ ESP32 (Wokwi)       │ Wokwi-GUEST │ Internet │ ───────▶ │ GitHub (repositório público)│
│ Firmware 1.0        │ ──────────▶ │          │           │  ├── version.json           │
│ LED RGB: azul       │             │          │ ◀─────── │  └── firmware_v2.bin        │
└─────────────────────┘             └──────────┘           └────────────────────────────┘

Fluxo: consultar version.json → comparar versões → baixar .bin → gravar OTA → reiniciar → FW 2.0
```

| Camada | O que faz |
|---|---|
| **Medição** | 5 leituras simuladas (10 a 20 cm) a cada 2 s, guardadas em um vetor. Uma sessão a cada 48 s, contados do **início** da sessão anterior e controlados com `millis()`, sem `delay()` longo. |
| **Processamento (1.0)** | Média aritmética. |
| **Processamento (2.0)** | Média, cópia ordenada (bubble sort próprio), mediana (3º elemento ordenado) e histerese NORMAL/ALERTA. |
| **Indicação** | LED RGB: **azul** = FW 1.0; **verde** = FW 2.0/NORMAL; **vermelho** = FW 2.0/ALERTA; azul piscando = gravação OTA. |
| **Atualização** | A cada 3 sessões: `GET version.json`, depois `ArduinoJson` extrai `version` e `url`, compara as versões e, se a remota for maior, `HTTPUpdate` baixa e grava o `.bin` na partição OTA livre, com reinício em seguida. |

### Mapa do tempo de uma sessão
```
00 s → leitura 1 | 02 s → leitura 2 | 04 s → leitura 3 | 06 s → leitura 4 | 08 s →
