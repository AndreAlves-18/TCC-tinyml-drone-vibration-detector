# Criar-Dataset: coleta do dataset de vibração

Esta pasta reúne os códigos e os dados usados para montar o dataset do projeto. A coleta é feita em bancada, com o acelerômetro ADXL345 fixado na estrutura do motor, e cada classe de falha da hélice é gravada em um arquivo `.csv` próprio.

Durante a coleta trabalham duas placas ao mesmo tempo. A Raspberry Pi Pico W lê o acelerômetro e envia as amostras pela USB para o computador, onde um script em Python grava os dados em arquivo. A WEMOS D1 Mini controla o motor através do ESC, variando a rotação automaticamente segundo um perfil pré-definido, que é o mesmo para todas as classes.

## Estrutura da pasta

```
Criar-Dataset/
├── Criar-Dataset.ino              # firmware de coleta (Pico W + ADXL345)
├── capturar_dados/
│   └── capturar_dados.py          # recebe as amostras pela USB e grava o arquivo
├── dataset/                       # dataset coletado (um .csv por classe)
│   ├── normal.csv
│   ├── desbalanceado_n1.csv
│   ├── desbalanceado_n2.csv
│   ├── danificada_n1.csv
│   └── danificada_n2.csv
└── README.md
```

O perfil automático de rotação do motor fica fora desta pasta, em `../Executar-Motor/Executar-Motor-Automatico/`, pois é gravado em outra placa.

## Os códigos

### Criar-Dataset.ino (Pico W)

Firmware de coleta, escrito para o core Arduino-Pico. Ele configura o ADXL345 e transmite as amostras para o computador.

O sensor é configurado com taxa de saída de 800 Hz (a taxa real medida no sensor usado é de 790,7 Hz), faixa de ±16g e resolução total, o que dá uma resolução de 3,9 mg por unidade (cerca de 0,038 m/s²) em qualquer faixa. A comunicação I2C roda a 400 kHz.

As amostras não são lidas uma a uma no tempo do microcontrolador. O sensor guarda cada amostra na sua memória FIFO interna, no ritmo do seu próprio relógio, e a Pico apenas esvazia essa FIFO a cada repetição do laço principal. Assim, o intervalo entre amostras é sempre uniforme, mesmo que a Pico atrase um pouco em alguma leitura.

Cada amostra é enviada pela USB como uma linha de texto no formato `n,x,y,z`, em que `n` é um contador sequencial e `x`, `y` e `z` são os valores brutos do sensor, em números inteiros. A conversão para m/s² é feita no computador, o que poupa processamento na Pico. Linhas que começam com `#` são mensagens de estado. Se a FIFO do sensor chegar a encher, indicando que alguma amostra pode ter sido perdida, o firmware envia a linha `# OVERRUN`.

O código ainda contém um trecho de controle do ESC pelo pino GP15, herdado de uma versão anterior da bancada. Na montagem atual esse pino não é ligado, pois quem controla o motor é a D1 Mini.

### capturar_dados.py (computador)

Script em Python que recebe as amostras pela porta serial e grava o arquivo da coleta. Ele converte os valores brutos para m/s², calcula a coluna de tempo a partir do número da amostra e da taxa real do sensor (790,7 Hz) e grava cada amostra no arquivo `.csv` no momento em que ela chega, salvando em disco duas vezes por segundo. Dessa forma, se a conexão cair ou o computador travar, os dados recebidos até ali ficam preservados.

O script também verifica a integridade da coleta. Ele conta as mensagens de `OVERRUN` vindas do firmware e confere se o contador `n` avança sem pular números. Qualquer perda é marcada com o valor 1 na coluna `Gap`, na primeira amostra depois da falha, para que o notebook possa descartar as janelas que contêm uma emenda. Se nenhuma amostra chegar por 3 segundos, o script encerra a coleta sozinho e salva o que recebeu. Ao terminar, ele informa quantas amostras foram gravadas e se houve alguma perda.

A leitura da porta é feita em blocos, e não linha a linha, porque a leitura linha a linha do pyserial não acompanhava com folga os 790 Hz do sensor e fazia a FIFO encher.

Opções de uso:

| Comando | Efeito |
|---|---|
| `python3 capturar_dados.py normal --sem-xlsx` | grava `normal.csv` (forma usada no dataset) |
| `python3 capturar_dados.py normal` | grava `normal.csv` e, ao final, gera também `normal.xlsx` |
| `python3 capturar_dados.py normal -p /dev/ttyACM1` | usa outra porta serial |
| `python3 capturar_dados.py` | sem nome, grava `dataset_<data_hora>.csv` |

Para não apagar uma coleta por engano, o script se recusa a gravar se já existir um arquivo com o mesmo nome. O arquivo `.xlsx` é opcional e só existe por compatibilidade com versões antigas do notebook, que hoje lê os arquivos `.csv` diretamente.

### Executar-Motor-Automatico.ino (D1 Mini)

Fica em `../Executar-Motor/Executar-Motor-Automatico/`. Controla o motor durante a coleta, gerando um perfil de rotação de 10 minutos que varia desde o início, sem longos períodos em rotação fixa.

O perfil vai de 5% a 15% da aceleração do ESC e é formado por ciclos de cerca de 2 minutos. Cada ciclo tem uma rampa lenta subindo e descendo, uma escada de degraus em quatro níveis e uma sequência de variações aleatórias, com transições e patamares de duração variável. As variações aleatórias usam uma semente fixa, então o perfil é exatamente o mesmo em todas as execuções. Com isso, todas as classes são coletadas com a mesma sequência de rotações, e o modelo não tem como associar uma classe a uma faixa de rotação específica.

O potenciômetro deixa de controlar a velocidade e passa a servir para iniciar e parar o perfil. Girar até o máximo inicia o perfil e acende o LED da placa. Baixar até a metade interrompe o motor imediatamente. Ao fim dos 10 minutos o motor para sozinho e o LED passa a piscar. Para repetir, basta levar o potenciômetro ao mínimo e depois ao máximo de novo.

## Protocolo de coleta

### Classes

| Arquivo | Classe | Condição da hélice |
|---|---|---|
| `normal.csv` | Normal | hélice em bom estado |
| `desbalanceado_n1.csv` | Desbalanceamento N1 | fita adesiva na hélice, em menor quantidade (desequilíbrio leve) |
| `desbalanceado_n2.csv` | Desbalanceamento N2 | fita adesiva na hélice, em maior quantidade (desequilíbrio severo) |
| `danificada_n1.csv` | Hélice danificada N1 | parte da pá removida, primeiro nível de dano |
| `danificada_n2.csv` | Hélice danificada N2 | parte da pá removida, segundo nível de dano |

### Variação das condições

Para que o dataset tenha variabilidade, cada classe foi gravada combinando duas fontes de variação.

A primeira é a rotação do motor, controlada pelo perfil automático da D1 Mini descrito acima. Ela é idêntica em todas as classes.

A segunda é a forma como o protótipo estava apoiado. Cada coleta foi dividida em três trechos, sempre nos mesmos pontos do arquivo:

| Trecho (amostras) | Tempo aproximado | Condição do protótipo |
|---|---|---|
| 0 a 155.000 | 0 a 196 s | protótipo segurado e balançado com a mão, de forma contínua |
| 155.000 a 310.000 | 196 a 392 s | protótipo preso, com a mão apoiada sobre ele para limitar o balanço |
| 310.000 até o fim | 392 s até o fim | protótipo solto, apenas evitando que ele se deslocasse pela bancada por causa da vibração |

Assim, o modelo é treinado com a vibração da hélice somada a diferentes perturbações externas e formas de fixação, e não apenas na condição ideal de bancada.

### Resultado da coleta

Cada arquivo tem cerca de 477.000 amostras, o equivalente a pouco mais de 10 minutos de gravação. Nenhuma das coletas teve perda de amostras: a coluna `Gap` está zerada nos cinco arquivos.

| Arquivo | Amostras | Duração |
|---|---|---|
| `normal.csv` | 476.367 | 602,5 s |
| `desbalanceado_n1.csv` | 477.582 | 604,0 s |
| `desbalanceado_n2.csv` | 477.354 | 603,7 s |
| `danificada_n1.csv` | 477.637 | 604,1 s |
| `danificada_n2.csv` | 476.679 | 602,9 s |

O começo e o fim de cada arquivo incluem alguns segundos com o motor parado, antes do início e depois do fim do perfil. O notebook descarta esses trechos automaticamente, removendo as janelas cuja intensidade de vibração (RMS) fica abaixo de 0,5 m/s².

## Formato dos dados

| Coluna | Descrição |
|---|---|
| `Time (s)` | tempo desde o início da gravação, calculado como número da amostra ÷ 790,7 |
| `X Acceleration` | aceleração no eixo X, em m/s² |
| `Y Acceleration` | aceleração no eixo Y, em m/s² |
| `Z Acceleration` | aceleração no eixo Z, em m/s² (inclui a gravidade, cerca de 9 m/s² em repouso) |
| `Gap` | 1 na primeira amostra depois de uma perda; 0 nas demais |

Os valores de aceleração aparecem com seis casas decimais, mas a resolução real do sensor é de 0,038 m/s². Todos os valores são múltiplos desse passo, e por isso valores exatamente iguais a zero são comuns no eixo que fica próximo de zero em repouso.

## Como usar

### 1. Ligações do ADXL345 na Pico W

| ADXL345 | Pico W | Pino físico |
|---|---|---|
| VCC | 3V3(OUT) | 36 |
| GND | GND | 8 |
| SDA | GP4 (I2C0 SDA) | 6 |
| SCL | GP5 (I2C0 SCL) | 7 |

Durante a coleta a Pico é alimentada pelo cabo USB, que também transmite os dados. O 5 V do BEC do ESC não deve estar ligado na Pico enquanto o USB estiver conectado. As ligações da D1 Mini, do potenciômetro e do ESC estão no README principal do projeto.

### 2. Preparar o ambiente

Na IDE do Arduino, instale o core Arduino-Pico. Em Arquivo > Preferências, adicione a URL abaixo e depois instale "Raspberry Pi Pico/RP2040" no Gerenciador de Placas:

```
https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
```

Não é preciso instalar bibliotecas extras: `Wire` e `Servo` já vêm com o core.

No computador, instale as dependências do script:

```bash
pip install pyserial openpyxl
```

O `openpyxl` só é necessário se você quiser gerar também o arquivo `.xlsx`.

### 3. Gravar as placas

Grave o `Criar-Dataset.ino` na Pico W, com a placa "Raspberry Pi Pico W" selecionada na IDE. Grave o `Executar-Motor-Automatico.ino` na D1 Mini, com a placa "LOLIN(WEMOS) D1 R2 & mini" selecionada. Antes de cada gravação, confira a placa e a porta no rodapé da IDE, já que as duas placas são programadas pela mesma ferramenta.

### 4. Gravar uma classe

1. Monte a hélice da classe que será coletada.
2. Ligue a bateria com o potenciômetro no mínimo e aguarde os bipes do ESC.
3. Conecte a Pico ao computador e inicie a gravação com o nome da classe:

   ```bash
   cd capturar_dados
   python3 capturar_dados.py normal --sem-xlsx
   ```

   As amostras começam a chegar cerca de 5 segundos depois, quando o firmware termina a inicialização.
4. Gire o potenciômetro até o máximo para iniciar o perfil de rotação.
5. Siga os três trechos do protocolo, acompanhando o contador de amostras na tela: balance o protótipo com a mão até 155.000 amostras, mantenha a mão apoiada sobre ele até 310.000 e depois deixe-o solto até o fim.
6. Quando o perfil terminar, o motor para e o LED da D1 Mini pisca. Pare a gravação apertando Ctrl+C uma única vez.
7. Confira o resumo no final. A mensagem "Nenhuma amostra perdida." indica que a coleta está íntegra.

Repita para cada classe, trocando a hélice e o nome do arquivo. Para que o perfil recomece do início, leve o potenciômetro ao mínimo antes de começar a próxima classe.

## Cuidados

As conexões do sensor precisam ser firmes. Como o protocolo inclui segurar e balançar o protótipo, jumpers encaixados ou protoboard podem perder contato. Os sintomas são overruns seguidos do script parando com a mensagem de coleta travada. Prefira fios soldados e presos perto do sensor.

Ruídos do motor podem derrubar a conexão USB ou travar o I2C. Para reduzir o risco, mantenha os fios do sensor curtos e afastados dos fios do motor e do ESC, use um cabo USB curto e preso à bancada e deixe o notebook na bateria, desconectado da tomada, durante a coleta.

Aperte Ctrl+C apenas uma vez para encerrar. Se o arquivo `.xlsx` estiver sendo gerado, um segundo Ctrl+C interrompe só a conversão, e os dados continuam completos no `.csv`.