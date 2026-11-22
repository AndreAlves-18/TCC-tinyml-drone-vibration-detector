# brushless-dataset-picow

Sistema embarcado para **controle de motor brushless via ESC** e **coleta de dados de aceleração** com MPU6050, usando Raspberry Pi Pico W. Os dados são transmitidos via USB Serial e salvos em formato `.xlsx` para montagem de datasets de machine learning.

---

## Hardware necessário

- Raspberry Pi Pico W
- ESC (Electronic Speed Controller) com BEC 5V
- Motor Brushless
- Sensor MPU6050 (acelerômetro/giroscópio)
- Potenciômetro 10kΩ
- Bateria LiPo (compatível com o ESC)
- Cabo USB micro

---

## Conexões

### Potenciômetro → Pico W

| Potenciômetro | Pico W         | Pino físico |
|---------------|----------------|-------------|
| Pino 1 (VCC)  | 3.3V           | 36          |
| Pino 2 (sinal)| GP26 (A0)      | 31          |
| Pino 3 (GND)  | GND            | 38          |

> ⚠️ Use sempre **3.3V** no potenciômetro — a Pico W não tolera 5V nas entradas analógicas.

---

### MPU6050 → Pico W

| MPU6050 | Pico W    | Pino físico |
|---------|-----------|-------------|
| VCC     | 3.3V      | 36          |
| GND     | GND       | 38          |
| SDA     | GP4       | 6           |
| SCL     | GP5       | 7           |

---

### ESC → Pico W

| ESC                      | Pico W    | Pino físico |
|--------------------------|-----------|-------------|
| Sinal (laranja/branco)   | GP15      | 20          |
| GND (preto/marrom)       | GND       | 38          |
| 5V BEC (vermelho)        | VSYS      | 39 — **apenas sem USB** |

> ⚠️ O fio de 5V do ESC (VSYS) deve ser **desconectado quando usar o cabo USB**. O GND e o sinal devem permanecer conectados sempre.

---

### ESC → Bateria e Motor

| ESC         | Destino           |
|-------------|-------------------|
| Fios grossos (vermelho/preto) | Bateria LiPo |
| Fios de fase (3x) | Motor Brushless |

---

## Alimentação — modos de operação

| Modo              | Alimentação da Pico W | Fio 5V BEC |
|-------------------|-----------------------|------------|
| Teste com PC      | Cabo USB              | ❌ Desconectado |
| Standalone (sem PC) | VSYS via ESC BEC    | ✅ Conectado   |

---

## Diagrama de conexão

```
                     ┌─────────────────────────┐
                     │      Raspberry Pi        │
  Potenciômetro ─────► GP26 (A0)    Pico W      │
  ESC sinal ─────────► GP15                     │
  MPU6050 SDA ────────► GP4                     │
  MPU6050 SCL ────────► GP5                     │
  GND (ESC + MPU) ────► GND                     │
  USB ou VSYS ─────────► Alimentação            │
                     └─────────────────────────┘

  Bateria LiPo ──► ESC ──► Motor Brushless
```

---

## Estrutura do projeto

```
brushless-dataset-picow/
├── README.md
├── arduino/
│   └── motor_mpu6050.ino       # Código para a Pico W
└── python/
    └── capturar_dados.py       # Script de coleta para PC
```

---

## Configuração do Arduino IDE

1. Instale o suporte à Pico W:
   - **Arquivo → Preferências → URLs adicionais:**
     ```
     https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
     ```
   - **Ferramentas → Gerenciador de Placas** → instale **Raspberry Pi Pico/RP2040**

2. Selecione a placa: **Ferramentas → Placa → Raspberry Pi Pico W**

3. Instale as bibliotecas:
   - `Adafruit MPU6050`
   - `Adafruit Unified Sensor`
   - `Servo` (já inclusa no pacote da Pico)

---

## Configuração Python

```bash
sudo apt install python3-serial python3-openpyxl
```

---

## Como usar

### Coleta de dados (Pico conectada ao PC)

1. Faça upload do código `motor_mpu6050.ino` para a Pico W
2. Conecte o ESC e o motor à bateria
3. Execute o script Python:
   ```bash
   python3 capturar_dados.py
   ```
4. Gire o potenciômetro para controlar a velocidade do motor
5. Pressione **Ctrl+C** para parar — o arquivo `.xlsx` é salvo automaticamente

---

## Formato do dataset gerado

| Time (s)  | X Acceleration | Y Acceleration | Z Acceleration |
|-----------|----------------|----------------|----------------|
| 0.010000  | 0.063028       | -0.036997      | -0.103550      |
| 0.020000  | 0.131003       | 0.098953       | 0.006932       |
| ...       | ...            | ...            | ...            |

- **Frequência de amostragem:** ~100 Hz (configurável via `INTERVALO_US` no Arduino)
- **Unidade de aceleração:** m/s²
- **Formato de saída:** `.xlsx` com timestamp no nome

---

## Parâmetros configuráveis (Arduino)

```cpp
#define PINO_ESC      15      // Pino PWM do ESC
#define PINO_POT      A0      // Pino do potenciômetro (GP26)
const unsigned int INTERVALO_US = 10000; // 10000 µs = 100 Hz
```

---

## Dependências

| Ferramenta        | Versão testada |
|-------------------|----------------|
| Arduino IDE       | 1.8.x / 2.x    |
| arduino-pico core | earlephilhower |
| Python            | 3.10+          |
| pyserial          | apt package    |
| openpyxl          | apt package    |