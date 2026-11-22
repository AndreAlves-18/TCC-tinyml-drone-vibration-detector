// Raspberry Pi Pico W (RP2040) - core Arduino-Pico
// Coleta de dataset com ADXL345 a 800 Hz usando a FIFO interna do sensor.
// Envia pela serial USB: contador,x,y,z (valores brutos, 3,9 mg/LSB).
// A conversão para m/s² é feita no Python.

#include <Servo.h>
#include <Wire.h>

Servo ESC;

// ---------------------------------------------------------------------------
// ADXL345
// ---------------------------------------------------------------------------
#define ADXL_ADDR_LOW    0x53   // SDO em GND
#define ADXL_ADDR_HIGH   0x1D   // SDO em 3,3 V

#define REG_DEVID        0x00
#define REG_BW_RATE      0x2C
#define REG_POWER_CTL    0x2D
#define REG_INT_SOURCE   0x30
#define REG_DATA_FORMAT  0x31
#define REG_DATAX0       0x32
#define REG_FIFO_CTL     0x38
#define REG_FIFO_STATUS  0x39

#define BW_RATE_800HZ    0x0D   // 800 Hz, filtro interno de 400 Hz
#define FORMAT_16G_FULL  0x0B   // resolução total, ±16g
#define FIFO_STREAM      0x80   // modo stream: guarda as 32 amostras mais recentes

uint8_t adxlAddr = ADXL_ADDR_LOW;

// ---------------------------------------------------------------------------
// Controle do motor
// ---------------------------------------------------------------------------
const unsigned long INTERVALO_ESC_US = 20000;   // ESC atualizado a 50 Hz
unsigned long ultimoESC = 0;

uint32_t contador = 0;   // número da amostra, para detectar perdas no Python

// Leitura com média para reduzir ruído
int lerPot() {
  long soma = 0;
  for (int i = 0; i < 5; i++) soma += analogRead(A0);
  return map(soma / 5, 0, 1023, 0, 27); // limitado a 15%
}

// ---------------------------------------------------------------------------
// Funções I2C do ADXL345
// ---------------------------------------------------------------------------
bool presente(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

void escreveReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(adxlAddr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

uint8_t leReg(uint8_t reg) {
  Wire.beginTransmission(adxlAddr);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(adxlAddr, (size_t)1);
  return Wire.read();
}

// Lê uma amostra da FIFO. Os 6 bytes precisam ser lidos numa única
// transação; ao terminar, a FIFO libera a próxima amostra.
void leAmostra(int16_t &x, int16_t &y, int16_t &z) {
  uint8_t b[6];
  Wire.beginTransmission(adxlAddr);
  Wire.write(REG_DATAX0);
  Wire.endTransmission(false);
  Wire.requestFrom(adxlAddr, (size_t)6);
  for (int i = 0; i < 6; i++) b[i] = Wire.read();

  x = (int16_t)((b[1] << 8) | b[0]);
  y = (int16_t)((b[3] << 8) | b[2]);
  z = (int16_t)((b[5] << 8) | b[4]);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  // Inicializa e arma o ESC
  ESC.attach(15, 1000, 2000);
  ESC.write(0);    // sinal mínimo para armar
  delay(5000);     // aguarda beeps de confirmação do ESC

  // I2C a 400 kHz (o padrão do Wire é 100 kHz, lento demais para 800 Hz)
  Wire.setSDA(4);
  Wire.setSCL(5);
  Wire.begin();
  Wire.setClock(400000);

  if (presente(ADXL_ADDR_LOW))       adxlAddr = ADXL_ADDR_LOW;
  else if (presente(ADXL_ADDR_HIGH)) adxlAddr = ADXL_ADDR_HIGH;
  else {
    Serial.println("# ERRO: ADXL345 nao encontrado!");
    while (1) delay(10);
  }

  if (leReg(REG_DEVID) != 0xE5) {
    Serial.println("# ERRO: dispositivo nao e um ADXL345!");
    while (1) delay(10);
  }

  escreveReg(REG_POWER_CTL, 0x00);           // standby durante a configuração
  escreveReg(REG_BW_RATE, BW_RATE_800HZ);
  escreveReg(REG_DATA_FORMAT, FORMAT_16G_FULL);
  escreveReg(REG_FIFO_CTL, 0x00);            // modo bypass: esvazia a FIFO
  escreveReg(REG_FIFO_CTL, FIFO_STREAM);
  escreveReg(REG_POWER_CTL, 0x08);           // inicia a medição

  Serial.println("# ADXL345 800Hz 16g full_res");
  Serial.println("n,x,y,z");
}

void loop() {
  // Motor atualizado a 50 Hz, independente da amostragem
  unsigned long agora = micros();
  if (agora - ultimoESC >= INTERVALO_ESC_US) {
    ultimoESC = agora;
    ESC.write(lerPot());
  }

  // Bit de overrun: a FIFO encheu e amostras antigas foram descartadas
  if (leReg(REG_INT_SOURCE) & 0x01) {
    Serial.println("# OVERRUN");
  }

  // Esvazia a FIFO enviando cada amostra
  uint8_t pendentes = leReg(REG_FIFO_STATUS) & 0x3F;
  char linha[40];
  for (uint8_t i = 0; i < pendentes; i++) {
    int16_t x, y, z;
    leAmostra(x, y, z);
    int len = snprintf(linha, sizeof(linha), "%lu,%d,%d,%d\n",
                       (unsigned long)contador++, x, y, z);
    Serial.write(linha, len);
  }
}





// // Raspberry Pi Pico W (RP2040)

// #include <Servo.h>
// #include <Wire.h>
// #include <Adafruit_MPU6050.h>
// #include <Adafruit_Sensor.h>

// Servo ESC;
// Adafruit_MPU6050 mpu;

// unsigned long tempoInicio;
// unsigned long ultimaLeitura;
// const unsigned int INTERVALO_US = 10000; // 100 Hz

// // Leitura com média para reduzir ruído
// int lerPot() {
//   long soma = 0;
//   for (int i = 0; i < 5; i++) soma += analogRead(A0);
//   return map(soma / 5, 0, 1023, 0, 27); // limitado a 15%
// }

// void setup() {
//   Serial.begin(115200);
//   while (!Serial) delay(10);

//   // Inicializa e arma o ESC corretamente
//   ESC.attach(15, 1000, 2000);
//   ESC.write(0);    // ← sinal mínimo para armar
//   delay(5000);     // aguarda beeps de confirmação do ESC

//   // Inicializa MPU6050
//   Wire.setSDA(4);
//   Wire.setSCL(5);
//   Wire.begin();

//   if (!mpu.begin()) {
//     Serial.println("ERRO: MPU6050 nao encontrado!");
//     while (1) delay(10);
//   }

//   mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
//   mpu.setFilterBandwidth(MPU6050_BAND_44_HZ);

//   Serial.println("Time (s),X Acceleration,Y Acceleration,Z Acceleration");

//   tempoInicio = micros();
//   ultimaLeitura = tempoInicio;
// }

// void loop() {
//   unsigned long agora = micros();

//   if (agora - ultimaLeitura >= INTERVALO_US) {
//     ultimaLeitura = agora;

//     // Motor e MPU atualizados juntos a 100 Hz
//     ESC.write(lerPot());

//     sensors_event_t a, g, temp;
//     mpu.getEvent(&a, &g, &temp);

//     float tempo = (agora - tempoInicio) / 1000000.0;

//     Serial.print(tempo, 6);   Serial.print(",");
//     Serial.print(a.acceleration.x, 6); Serial.print(",");
//     Serial.print(a.acceleration.y, 6); Serial.print(",");
//     Serial.println(a.acceleration.z, 6);
//   }
// }