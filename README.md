# Detecção de Falhas em Hélices de Drone com TinyML

Sistema de detecção de falhas em hélices de drone baseado em sinais de vibração, utilizando uma CNN 1D treinada em Python/Keras e implantada em um microcontrolador Raspberry Pi Pico W via TensorFlow Lite Micro.

## Sobre o projeto

O objetivo é identificar três condições de operação a partir de sinais de vibração capturados por um acelerômetro (ADXL345):

- **Normal** — hélice balanceada
- **Anômalo N1** — desbalanceamento leve (menos fita adesiva)
- **Anômalo N2** — desbalanceamento severo (mais fita adesiva)

O pipeline completo cobre desde a coleta e pré-processamento dos dados até o treinamento do modelo e a inferência embarcada em tempo real no microcontrolador, sem depender de processamento externo ou conexão com a nuvem.

Este projeto foi desenvolvido como parte do TCC e da disciplina de Aprendizado de Máquina Embarcado (AME) na Universidade Federal do Ceará (UFC), Campus Quixadá.

## Estrutura do repositório

```
.
├── dataset/                                                          # arquivos .xlsx com os sinais de vibração
├── notebook/                                                         # notebook com todo o pipeline de ML
├── model/                                                            # modelo treinado (.keras, .tflite)
├── drone_fault_detector/                                             # projeto do firmware (Pico SDK)
│   ├── drone_fault_detector.cpp                                      # código principal do firmware
│   ├── drone_fault_model.cc                                          # modelo convertido em array C
│   ├── drone_fault_model.h                                           # header do modelo
│   ├── CMakeLists.txt                                                # configuração de build
│   ├── pico_sdk_import.cmake
│   └── pico-tflmicro/                                                # biblioteca TensorFlow Lite Micro
├── Detecção_de_Falhas_em_Motores_de_Drone_com_Base_em_Sinais_de_Vibração_Utilizando_TinyML.pdf   # artigo
├── SLIDE_Entregável 3 - Final.pdf                                    # slides de apresentação
└── README.md
```

## Instalação

### Pré-requisitos

- Python 3.12+ (ou conta no Google Colab, sem necessidade de instalação local)
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) configurado
- CMake e Ninja
- VS Code com a extensão oficial do Raspberry Pi Pico (recomendado)
- Git

### Clonando o repositório

```bash
git clone https://github.com/<seu-usuario>/drone-fault-detection-tinyml.git
cd drone-fault-detection-tinyml
```

### Clonando a dependência do firmware

Dentro da pasta `drone_fault_detector/`, clone a biblioteca TFLite Micro:

```bash
cd drone_fault_detector
git clone https://github.com/raspberrypi/pico-tflmicro.git
```

## Como rodar o notebook

1. Abra o notebook localizado em `notebook/` no [Google Colab](https://colab.research.google.com/)
2. Faça o upload dos três arquivos `.xlsx` da pasta `dataset/` (Normal, Anômalo N1, Anômalo N2) para o ambiente do Colab
3. Atualize as variáveis `PATH_NORMAL`, `PATH_ANOMALO_N1` e `PATH_ANOMALO_N2` na seção de configuração com os caminhos dos arquivos enviados
4. Execute as células na ordem, de cima para baixo
5. Ao final, o notebook gera o modelo treinado (salvo em `model/`) e os arquivos `drone_fault_model.cc` / `.tflite` quantizado, prontos para uso no firmware

## Conexões de hardware

O sistema usa duas placas independentes. O Raspberry Pi Pico W lê o acelerômetro ADXL345 e executa a inferência. A WEMOS D1 Mini (ESP8266) lê o potenciômetro e controla o ESC do motor. Nenhum sinal passa de uma placa para a outra, então não é necessário ligar os GNDs entre elas.

```
Bateria -> ESC -> Motor brushless
            |
            |-- 5V (BEC) -> D1 Mini (pino 5V)
            |-- sinal <- D1 Mini (D5) <- Potenciômetro (A0)

Pico W <- I2C -> ADXL345 (fixado na estrutura do motor)
Pico W <- USB -> Computador (alimentação e serial)
```

### Pico W e ADXL345

| ADXL345 | Pico W | Pino físico |
|---------|--------|-------------|
| VCC | 3V3(OUT) | 36 |
| GND | GND | 8 |
| SDA | GP4 (I2C0 SDA) | 6 |
| SCL | GP5 (I2C0 SCL) | 7 |

O Pico é alimentado pelo cabo USB, que também serve para a leitura serial.

### Ligação do display LCD 16x2 (módulo I2C PCF8574)

O LCD, a Pico e a D1 Mini são alimentados pelo 5 V do BEC do ESC e compartilham o mesmo terra.

| Módulo do LCD | Ligação |
|---|---|
| VCC | 5 V do BEC do ESC |
| GND | GND comum |
| SDA | GP6 / I2C1 SDA da Pico (pino 9) |
| SCL | GP7 / I2C1 SCL da Pico (pino 10) |

Observações:

- O módulo I2C tem resistores de pull-up para o seu VCC, de modo que SDA e SCL operam em 5 V, acima do nível de 3,3 V do RP2040. A montagem funciona, mas está fora da especificação do microcontrolador. Uma versão definitiva deve usar um conversor de nível lógico bidirecional entre a Pico e o módulo.
- O firmware detecta automaticamente se o módulo está no endereço `0x27` ou `0x3F`. Se o display acender sem mostrar caracteres, ajuste o contraste no trimpot azul atrás do módulo.

### D1 Mini, potenciômetro e ESC

| Componente | Pino do componente | D1 Mini |
|------------|--------------------|---------|
| Potenciômetro | Pino do meio (cursor) | A0 |
| Potenciômetro | Extremidade 1 | 3V3 |
| Potenciômetro | Extremidade 2 | GND |
| ESC | Sinal (fio laranja) | D5 (GPIO14) |
| ESC | GND (fio marrom) | GND |
| ESC | 5V do BEC (fio vermelho) | 5V |

Os fios grossos do ESC não se ligam a nenhuma das placas. Os dois fios de entrada (vermelho e preto) vão na bateria, e os três fios de saída vão no motor brushless.

### Observações importantes

- A D1 Mini é alimentada pelo 5V do BEC do ESC, sem depender de USB. Nunca ligue o fio de 5V do BEC no Pico enquanto ele estiver conectado ao USB, pois as duas fontes podem conflitar.
- No Arduino IDE, selecione a placa **LOLIN(WEMOS) D1 R2 & mini**. Com a opção "Generic ESP8266 Module" os nomes como `D5` não são reconhecidos.
- Ao ligar, o ESC precisa receber o sinal mínimo por alguns segundos para armar. O sketch aguarda 5 s com o sinal em zero. Espere os bipes de confirmação antes de girar o potenciômetro.
- Mantenha os fios do ADXL345 curtos e afastados dos cabos de potência do ESC, pois o ruído elétrico pode interferir na comunicação I2C.

## Como gravar no Raspberry Pi Pico W

### Pré-requisitos

O projeto usa o Pico SDK 2.2.0 e o compilador ARM 14.2 instalados pela extensão Raspberry Pi Pico do VS Code. Antes de compilar, defina as variáveis de ambiente abaixo. Para não precisar repetir a cada terminal novo, adicione as duas linhas ao `~/.bashrc`.

```bash
export PICO_SDK_PATH=$HOME/.pico-sdk/sdk/2.2.0
export PICO_TOOLCHAIN_PATH=$HOME/.pico-sdk/toolchain/14_2_Rel1
```

### Compilação

1. Copie os arquivos `drone_fault_model.cc` e `drone_fault_model.h` gerados pelo notebook para a pasta `drone_fault_detector/`
2. Configure e compile o projeto:

Ná primeira vez:

```bash
cd drone_fault_detector
mkdir build
cd build
cmake -G Ninja -DPICO_BOARD=pico_w ..
ninja drone_fault_detector
```
Nas próximas:

```bash
cd drone_fault_detector/build
ninja drone_fault_detector
```

A primeira compilação leva alguns minutos, porque compila a biblioteca do TFLite Micro e o picotool. Nas seguintes, só os arquivos alterados são recompilados.

Se a pasta do projeto for movida ou renomeada, apague a pasta `build` e repita os comandos acima, pois o cache do CMake guarda o caminho antigo.

3. Conecte o Pico ao computador segurando o botão **BOOTSEL**. Ele será reconhecido como um dispositivo de armazenamento (`RPI-RP2`)
4. Copie o arquivo `.uf2` gerado para o Pico:

```bash
cp drone_fault_detector.uf2 /media/$USER/RPI-RP2/
```

5. O Pico reinicia automaticamente e começa a executar o firmware
6. Os resultados aparecem no display LCD 16x2. Na inicialização, o display mostra as etapas de carregamento e a frequência de amostragem medida. Em seguida, passa a exibir a cada janela:


## Documentação

- **Artigo:** [`Detecção_de_Falhas_em_Motores_de_Drone_com_Base_em_Sinais_de_Vibração_Utilizando_TinyML.pdf`](./Detecção_de_Falhas_em_Motores_de_Drone_com_Base_em_Sinais_de_Vibração_Utilizando_TinyML.pdf) — documento completo do projeto, com fundamentação teórica, metodologia, implementação e resultados
- **Slides:** [`SLIDE_Entregável 3 - Final.pdf`](<./SLIDE_Entregável 3 - Final.pdf>) — apresentação resumida do projeto

## Pipeline de Machine Learning (Colab)

O notebook cobre as seguintes etapas:

1. **Carregamento dos dados** — leitura dos arquivos `.xlsx` contendo os sinais de vibração brutos (eixos X, Y, Z)
2. **Exploração dos dados** — estatísticas descritivas, visualização temporal e verificação de balanceamento entre classes
3. **Pré-processamento** — segmentação do sinal contínuo em janelas de 26 amostras e normalização Z-score
4. **Divisão temporal** — separação em treino (70%), validação (15%) e teste (15%) respeitando a ordem cronológica das janelas, evitando vazamento de dados entre conjuntos
5. **Modelo** — CNN 1D compacta (TinyCNN1D), com blocos Conv1D → BatchNorm → ReLU → MaxPooling, finalizando em GlobalAveragePooling e uma camada densa com Softmax
6. **Treinamento** — otimizador Adam, *categorical crossentropy*, com EarlyStopping, ReduceLROnPlateau e ModelCheckpoint
7. **Avaliação** — acurácia, precisão, recall, F1-score e matriz de confusão no conjunto de teste
8. **Conversão para TinyML** — exportação do modelo para TensorFlow Lite com quantização int8, seguida da geração do array C (`.cc`) compatível com TensorFlow Lite Micro

### Tecnologias utilizadas

- Python 3.12
- TensorFlow / Keras
- NumPy, Pandas, Scikit-learn
- Matplotlib, Seaborn
- Google Colab

## Firmware embarcado (Raspberry Pi Pico W)

O firmware está na pasta `drone_fault_detector/` e foi desenvolvido em C/C++ utilizando o Raspberry Pi Pico SDK e a biblioteca [pico-tflmicro](https://github.com/raspberrypi/pico-tflmicro), responsável por executar a inferência do modelo TFLite diretamente no microcontrolador.

### Funcionamento

1. Inicializa a comunicação I2C com o acelerômetro ADXL345
2. Coleta uma janela de 26 amostras dos três eixos de aceleração
3. Normaliza os dados coletados
4. Executa a inferência com o TensorFlow Lite Micro
5. Identifica a classe com maior probabilidade e imprime o resultado via USB serial

### Hardware utilizado

- Raspberry Pi Pico W (RP2040)
- Acelerômetro ADXL345
- Display LCD 16x2 com módulo I2C 
- WEMOS D1 Mini (ESP8266), para controle do motor
- Motor brushless A2212/10T 1400KV e ESC
- Potenciômetro para controle de velocidade
- Bateria (cerca de 12 V)

### Dependências

- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
- [pico-tflmicro](https://github.com/raspberrypi/pico-tflmicro)

## Dataset

O modelo foi treinado com um dataset próprio, coletado em bancada com o acelerômetro ADXL345 acoplado a um motor de drone, cobrindo as três condições de operação descritas acima. A metodologia de simulação de falhas (adição de fita adesiva na hélice) foi inspirada no protocolo de Al-Haddad et al. (2025).

## Autor

André Alves de Freitas — Engenharia de Computação, UFC Campus Quixadá