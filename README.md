# Detecção de Falhas em Hélices de Drone com TinyML

Sistema de detecção de falhas em hélices de drone baseado em sinais de vibração, utilizando uma CNN 1D treinada em Python/Keras e implantada em um microcontrolador Raspberry Pi Pico W via TensorFlow Lite Micro.

## Sobre o projeto

O objetivo é identificar cinco condições de operação da hélice a partir dos sinais de vibração capturados por um acelerômetro ADXL345, amostrado a cerca de 790 Hz:

- **Normal**: hélice em bom estado
- **Desbalanceamento N1**: fita adesiva na hélice, em menor quantidade (desequilíbrio leve)
- **Desbalanceamento N2**: fita adesiva na hélice, em maior quantidade (desequilíbrio severo)
- **Danificada N1**: parte da pá removida, primeiro nível de dano
- **Danificada N2**: parte da pá removida, segundo nível de dano

O pipeline completo cobre desde a coleta dos dados em bancada até o treinamento do modelo e a inferência embarcada em tempo real no microcontrolador, com o resultado exibido em um display LCD, sem depender de processamento externo ou conexão com a nuvem.

Este projeto foi desenvolvido como Trabalho de Conclusão de Curso (TCC) do curso de Engenharia de Computação da Universidade Federal do Ceará (UFC), Campus Quixadá.

## Estrutura do repositório

```
.
├── Criar-Dataset/                    # coleta do dataset (tem README próprio)
│   ├── Criar-Dataset.ino             # firmware de coleta (Pico W + ADXL345)
│   ├── capturar_dados/
│   │   └── capturar_dados.py         # grava os dados recebidos pela USB
│   └── dataset/                      # um arquivo .csv por classe
├── Executar-Motor/                   # códigos da D1 Mini (controle do motor)
│   ├── Executar-Motor/               # controle manual pelo potenciômetro
│   └── Executar-Motor-Automatico/    # perfil automático de rotação (coleta)
├── notebook/                         # pipeline de ML (.ipynb e versão .html)
├── model/                            # modelos gerados (.keras, .tflite, .cc)
├── drone_fault_detector/             # firmware de inferência (Pico SDK)
│   ├── drone_fault_detector.cpp      # código principal do firmware
│   ├── drone_fault_model.cc          # modelo convertido em array C
│   ├── drone_fault_model.h           # declaração do modelo
│   ├── CMakeLists.txt                # configuração de build
│   ├── pico_sdk_import.cmake
│   └── pico-tflmicro/                # biblioteca TensorFlow Lite Micro
├── PicoW-A4-Pinout.pdf               # pinagem da Raspberry Pi Pico W
└── README.md
```

## Instalação

### Pré-requisitos

- Python 3.12+ (ou conta no Google Colab, sem necessidade de instalação local)
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) configurado
- CMake e Ninja
- VS Code com a extensão oficial do Raspberry Pi Pico (recomendado)
- Git
- Arduino IDE com os cores Arduino-Pico (Pico W, usada na coleta) e ESP8266 (D1 Mini)

### Clonando o repositório

```bash
git clone https://github.com/AndreAlves-18/TCC-tinyml-drone-vibration-detector.git
cd TCC-tinyml-drone-vibration-detector
```

### Clonando a dependência do firmware

Dentro da pasta `drone_fault_detector/`, clone a biblioteca TFLite Micro:

```bash
cd drone_fault_detector
git clone https://github.com/raspberrypi/pico-tflmicro.git
```

## Como rodar o notebook

1. Abra o notebook localizado em `notebook/` no [Google Colab](https://colab.research.google.com/)
2. Faça o upload dos cinco arquivos `.csv` da pasta `Criar-Dataset/dataset/` para o ambiente do Colab
3. Na célula de configuração, confira o dicionário `ARQUIVOS`: os caminhos dos arquivos e a ordem das classes, que precisa ser a mesma do firmware, com a classe Normal em primeiro
4. Execute as células na ordem, de cima para baixo. Para treinar do zero novamente, execute de novo a célula que cria o modelo antes da célula de treinamento
5. Ao final, o notebook gera o modelo quantizado (`drone_fault_quant.tflite`), o arquivo `drone_fault_model.cc` pronto para o firmware e, na última célula, os valores de `SCALER_MEAN` e `SCALER_SCALE` usados na normalização

## Conexões de hardware

O sistema usa duas placas. A Raspberry Pi Pico W lê o acelerômetro ADXL345, executa a inferência e mostra o resultado no display LCD. A WEMOS D1 Mini lê o potenciômetro e controla o ESC do motor. Nenhum sinal passa de uma placa para a outra.

```
Bateria -> ESC -> Motor brushless
            |
            |-- 5V (BEC) -> D1 Mini (5V), Pico W (VSYS)*, LCD (VCC)
            |-- sinal <- D1 Mini (D5) <- Potenciômetro (A0)

Pico W <- I2C0 -> ADXL345 (fixado na estrutura do motor)
Pico W <- I2C1 -> LCD 16x2
Pico W <- USB  -> Computador (coleta do dataset)*

* VSYS e USB nunca ligados ao mesmo tempo
```

### Alimentação da Pico W

A Pico W tem dois modos de alimentação. Na coleta do dataset, ela é alimentada pelo cabo USB, que também transmite os dados para o computador. Na demonstração do detector, ela é alimentada pelo 5 V do BEC do ESC no pino VSYS (39), junto com o LCD e a D1 Mini, e todos compartilham o mesmo terra. As duas fontes nunca devem ser ligadas ao mesmo tempo: desconecte o fio do BEC do VSYS antes de conectar o USB.

### Pico W e ADXL345

| ADXL345 | Pico W | Pino físico |
|---------|--------|-------------|
| VCC | 3V3(OUT) | 36 |
| GND | GND | 8 |
| SDA | GP4 (I2C0 SDA) | 6 |
| SCL | GP5 (I2C0 SCL) | 7 |

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

A D1 Mini pode receber dois códigos, ambos na pasta `Executar-Motor/`. O `Executar-Motor.ino` faz o controle manual: o potenciômetro ajusta a velocidade, limitada a 15% da aceleração. O `Executar-Motor-Automatico.ino` executa o perfil automático de rotação usado na coleta do dataset; nele, o potenciômetro apenas inicia e para o perfil. Os detalhes estão no README da pasta `Criar-Dataset/`.

### Observações importantes

- A D1 Mini é alimentada pelo 5V do BEC do ESC, sem depender de USB. Nunca ligue o fio de 5V do BEC no Pico enquanto ele estiver conectado ao USB, pois as duas fontes podem conflitar.
- No Arduino IDE, selecione a placa **LOLIN(WEMOS) D1 R2 & mini**. Com a opção "Generic ESP8266 Module" os nomes como `D5` não são reconhecidos.
- Ao ligar, o ESC precisa receber o sinal mínimo por alguns segundos para armar. Os dois códigos da D1 Mini enviam esse sinal na inicialização. Deixe o potenciômetro no mínimo ao ligar e espere os bipes de confirmação antes de girá-lo.
- Mantenha os fios do ADXL345 curtos e afastados dos cabos de potência do ESC, pois o ruído elétrico pode interferir na comunicação I2C.

## Como gravar no Raspberry Pi Pico W

### Pré-requisitos

O projeto usa o Pico SDK 2.2.0 e o compilador ARM 14.2 instalados pela extensão Raspberry Pi Pico do VS Code. Antes de compilar, defina as variáveis de ambiente abaixo. Para não precisar repetir a cada terminal novo, adicione as duas linhas ao `~/.bashrc`.

```bash
export PICO_SDK_PATH=$HOME/.pico-sdk/sdk/2.2.0
export PICO_TOOLCHAIN_PATH=$HOME/.pico-sdk/toolchain/14_2_Rel1
```

### Compilação

1. Copie o arquivo `drone_fault_model.cc` gerado pelo notebook para a pasta `drone_fault_detector/` e, no `drone_fault_detector.cpp`, atualize os vetores `SCALER_MEAN` e `SCALER_SCALE` com os valores impressos na última célula do notebook. Se o tamanho da janela ou as classes mudarem no notebook, ajuste também `WINDOW_SIZE`, `N_CLASSES` e `CLASS_NAMES`, mantendo a mesma ordem das classes.
2. Configure e compile o projeto:

Na primeira vez:

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
6. Os resultados aparecem no display LCD 16x2. Na inicialização, o display mostra as etapas de carregamento e, por 3 segundos, a frequência de amostragem medida e o tempo de uma inferência. Em seguida, passa a exibir o resultado a cada janela de 512 amostras (cerca de 0,65 s):

```
+----------------+     +----------------+     +----------------+
|     NORMAL     |     | *** ALERTA *** |     |  MOTOR PARADO  |
|    conf 98%    |     |Danif. N1    95%|     |                |
+----------------+     +----------------+     +----------------+
```

Com o motor parado, o modelo não é consultado, pois as condições estão fora das que ele viu no treino. Se ocorrer uma falha na inicialização (sensor não encontrado, modelo incompatível, memória insuficiente ou tamanho de entrada diferente do esperado), a mensagem de erro fica fixa no display.


## Pipeline de Machine Learning (Colab)

O notebook cobre as seguintes etapas:

1. **Carregamento dos dados**: leitura dos cinco arquivos `.csv` (um por classe) gerados na coleta
2. **Exploração dos dados**: estatísticas descritivas, trechos do sinal, espectro médio e espectrograma de cada classe
3. **Divisão em blocos**: o sinal de cada classe é dividido em 21 blocos de cerca de 30 s, distribuídos no padrão de 5 blocos de treino, 1 de validação e 1 de teste (cerca de 71%, 14% e 14%), para que os três conjuntos cubram todas as condições de rotação e de fixação
4. **Janelamento**: dentro de cada bloco, o sinal é segmentado em janelas de 512 amostras (cerca de 0,65 s) com 50% de sobreposição. Como as janelas não atravessam a fronteira entre blocos, nenhuma amostra aparece em mais de um conjunto. Janelas que contêm perda de amostras (coluna `Gap`) ou com o motor parado (RMS abaixo de 0,5 m/s²) são descartadas
5. **Normalização**: Z-score por eixo, com médias e desvios calculados apenas no conjunto de treino e reutilizados no firmware
6. **Modelo**: CNN 1D com quatro blocos Conv1D, BatchNorm e ReLU (os três primeiros com MaxPooling), seguidos de GlobalAveragePooling, Dropout e uma camada densa com Softmax, totalizando cerca de 20 mil parâmetros
7. **Treinamento**: otimizador Adam e *categorical crossentropy*, com EarlyStopping, ReduceLROnPlateau e ModelCheckpoint
8. **Avaliação**: acurácia, precisão, recall, F1-score, matrizes de confusão e distribuição dos erros ao longo da coleta. A acurácia no conjunto de teste ficou entre 96,6% e 97,2% em execuções repetidas
9. **Conversão para TinyML**: quantização int8 completa com conjunto de dados representativo, mantendo entrada e saída em float (cerca de 37,5 KB), e geração do array C (`.cc`) com o modelo armazenado na memória flash

### Tecnologias utilizadas

- Python 3.12
- TensorFlow / Keras
- NumPy, Pandas, SciPy, Scikit-learn
- Matplotlib, Seaborn
- Google Colab

## Firmware embarcado (Raspberry Pi Pico W)

O firmware está na pasta `drone_fault_detector/` e foi desenvolvido em C/C++ utilizando o Raspberry Pi Pico SDK e a biblioteca [pico-tflmicro](https://github.com/raspberrypi/pico-tflmicro), responsável por executar a inferência do modelo TFLite diretamente no microcontrolador.

### Funcionamento

1. Inicializa o display LCD (I2C1, 100 kHz) e o acelerômetro ADXL345 (I2C0, 400 kHz), configurado em 800 Hz, faixa de ±16g e FIFO interna em modo stream
2. Carrega o modelo, confere se o tamanho da entrada corresponde à janela configurada e mostra no display a frequência de amostragem medida e o tempo de uma inferência
3. Coleta uma janela de 512 amostras dos três eixos a partir da FIFO do sensor, com intervalo uniforme entre amostras
4. Calcula a intensidade da vibração (RMS) da janela e, se ela estiver abaixo de 0,5 m/s², mostra "MOTOR PARADO" sem consultar o modelo
5. Normaliza os dados com os mesmos parâmetros Z-score do treinamento
6. Executa a inferência com o TensorFlow Lite Micro e mostra no display a classe de maior probabilidade e a confiança

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

O modelo foi treinado com um dataset próprio, coletado em bancada com o acelerômetro ADXL345 fixado na estrutura do motor e amostrado a cerca de 790 Hz. São cerca de 477 mil amostras por classe (pouco mais de 10 minutos), gravadas com um perfil automático de rotação idêntico para todas as classes e com o protótipo em três condições diferentes de fixação. Os arquivos, os códigos de coleta e o protocolo completo estão na pasta `Criar-Dataset/`, que tem um README próprio.

A metodologia de simulação de desbalanceamento (adição de fita adesiva na hélice) foi inspirada no protocolo de Al-Haddad et al. (2025).

## Autor

André Alves de Freitas — Engenharia de Computação, UFC Campus Quixadá