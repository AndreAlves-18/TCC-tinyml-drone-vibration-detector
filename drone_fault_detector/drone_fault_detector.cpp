#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// TFLite Micro
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

// modelo
#include "drone_fault_model.h"

// configurações do ADXL345 (barramento i2c0, 400 kHz)
#define I2C_PORT          i2c0
#define I2C_SDA_PIN       4
#define I2C_SCL_PIN       5
#define ADXL345_ADDR_LOW  0x53   // SDO em GND
#define ADXL345_ADDR_HIGH 0x1D   // SDO em 3,3 V
#define ADXL345_DEVID     0xE5

#define REG_DEVID         0x00
#define REG_BW_RATE       0x2C
#define REG_POWER_CTL     0x2D
#define REG_DATA_FORMAT   0x31
#define REG_DATAX0        0x32
#define REG_FIFO_CTL      0x38
#define REG_FIFO_STATUS   0x39

#define BW_RATE_CFG       0x0D   // 800 Hz (filtro interno de 400 Hz)
#define DATA_FORMAT_CFG   0x0B   // resolução total, ±16g
#define SCALE_G_PER_LSB   0.0039f

#define WINDOW_SIZE       512
#define N_AXES            3

static uint8_t adxl_addr = ADXL345_ADDR_LOW;

// configurações do LCD 16x2 com PCF8574 (barramento i2c1, 100 kHz)
#define LCD_I2C_PORT   i2c1
#define LCD_SDA_PIN    6      // pino físico 9
#define LCD_SCL_PIN    7      // pino físico 10
#define LCD_COLS       16

// bits do PCF8574 no módulo
#define LCD_RS         0x01
#define LCD_ENABLE     0x04
#define LCD_BACKLIGHT  0x08

static uint8_t lcd_addr = 0x27;   // detectado automaticamente (0x27 ou 0x3F)

// número de classes do modelo
#define N_CLASSES      5

#define LIMIAR_RMS     0.5f   // m/s²: abaixo disso, motor parado

// nomes das classes
const char* CLASS_NAMES[] = {"Normal", "Desbal. N1", "Desbal. N2", "Danif. N1", "Danif. N2"};

// memória para o TFLite Micro
constexpr int TENSOR_ARENA_SIZE = 32 * 1024;   // 32 KB
uint8_t tensor_arena[TENSOR_ARENA_SIZE];

// buffer da janela
float window_buffer[WINDOW_SIZE][N_AXES];

// parâmetros do Z-score copiar do Colab após treino
// const float SCALER_MEAN[N_AXES]  = {0.58280464f, 0.11885266f, 8.88627601f};
// const float SCALER_SCALE[N_AXES] = { 7.90078791f,  10.12838644f,  9.40442866f};

const float SCALER_MEAN[N_AXES]  = {0.57114059f,  0.15809539f,  8.87053609f};
const float SCALER_SCALE[N_AXES] = {6.72922134f,  8.25188271f,  7.26392323f};


static bool adxl_write_reg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    return i2c_write_blocking(I2C_PORT, adxl_addr, buf, 2, false) == 2;
}

static bool adxl_read_regs(uint8_t reg, uint8_t *dst, size_t len) {
    if (i2c_write_blocking(I2C_PORT, adxl_addr, &reg, 1, true) != 1) return false;
    return i2c_read_blocking(I2C_PORT, adxl_addr, dst, len, false) == (int)len;
}

// confere o DEVID e configura taxa, faixa e modo de medição
bool adxl_init() {
    uint8_t devid = 0;
    if (!adxl_read_regs(REG_DEVID, &devid, 1) || devid != ADXL345_DEVID) return false;
    return adxl_write_reg(REG_POWER_CTL, 0x00)          // standby
        && adxl_write_reg(REG_BW_RATE, BW_RATE_CFG)
        && adxl_write_reg(REG_DATA_FORMAT, DATA_FORMAT_CFG)
        && adxl_write_reg(REG_FIFO_CTL, 0x00)           // esvazia a FIFO
        && adxl_write_reg(REG_FIFO_CTL, 0x80)           // modo stream
        && adxl_write_reg(REG_POWER_CTL, 0x08);         // inicia a medição
}

// lê os 3 eixos em m/s² (little endian: byte baixo primeiro)
void adxl_read_accel(float *ax, float *ay, float *az) {
    uint8_t raw[6] = {0};
    adxl_read_regs(REG_DATAX0, raw, 6);

    int16_t x = (int16_t)((raw[1] << 8) | raw[0]);
    int16_t y = (int16_t)((raw[3] << 8) | raw[2]);
    int16_t z = (int16_t)((raw[5] << 8) | raw[4]);

    *ax = x * SCALE_G_PER_LSB * 9.81f;
    *ay = y * SCALE_G_PER_LSB * 9.81f;
    *az = z * SCALE_G_PER_LSB * 9.81f;
}

// preenche a janela com amostras consecutivas da FIFO, no ritmo do sensor
void adxl_collect_window() {
    int i = 0;
    while (i < WINDOW_SIZE) {
        uint8_t status = 0;
        adxl_read_regs(REG_FIFO_STATUS, &status, 1);
        int pendentes = status & 0x3F;
        while (pendentes-- > 0 && i < WINDOW_SIZE) {
            adxl_read_accel(&window_buffer[i][0],
                            &window_buffer[i][1],
                            &window_buffer[i][2]);
            i++;
        }
    }
}

// intensidade da vibração da janela (mesmo cálculo do notebook)
float window_rms() {
    float soma_var = 0.0f;
    for (int c = 0; c < N_AXES; c++) {
        float media = 0.0f;
        for (int t = 0; t < WINDOW_SIZE; t++) media += window_buffer[t][c];
        media /= WINDOW_SIZE;

        float var = 0.0f;
        for (int t = 0; t < WINDOW_SIZE; t++) {
            float d = window_buffer[t][c] - media;
            var += d * d;
        }
        soma_var += var / WINDOW_SIZE;
    }
    return sqrtf(soma_var);
}

// normaliza uma janela com Z-score 
void normalize_window(float input[WINDOW_SIZE][N_AXES],
                      float output[WINDOW_SIZE][N_AXES]) {
    for (int t = 0; t < WINDOW_SIZE; t++) {
        for (int c = 0; c < N_AXES; c++) {
            output[t][c] = (input[t][c] - SCALER_MEAN[c]) / SCALER_SCALE[c];
        }
    }
}

// envia meio byte ao LCD com o pulso de ENABLE
static void lcd_write_nibble(uint8_t nibble, uint8_t mode) {
    uint8_t val = (nibble & 0xF0) | mode | LCD_BACKLIGHT;
    uint8_t seq[2] = { (uint8_t)(val | LCD_ENABLE), val };
    i2c_write_blocking(LCD_I2C_PORT, lcd_addr, seq, 2, false);
    sleep_us(40);
}

static void lcd_send(uint8_t byte, uint8_t mode) {
    lcd_write_nibble(byte & 0xF0, mode);
    lcd_write_nibble((uint8_t)(byte << 4), mode);
}

static void lcd_command(uint8_t cmd) { lcd_send(cmd, 0); }
static void lcd_char(char c)         { lcd_send((uint8_t)c, LCD_RS); }

static void lcd_clear() {
    lcd_command(0x01);
    sleep_ms(2);
}

static void lcd_set_cursor(uint8_t row, uint8_t col) {
    lcd_command(0x80 | (col + (row ? 0x40 : 0x00)));
}

// escreve uma linha inteira, completando com espaços (evita piscar)
static void lcd_print_line(uint8_t row, const char* text) {
    char line[LCD_COLS + 1];
    snprintf(line, sizeof(line), "%-16.16s", text);
    lcd_set_cursor(row, 0);
    for (int i = 0; i < LCD_COLS; i++) lcd_char(line[i]);
}

// escreve o texto centralizado na linha
static void lcd_print_centered(uint8_t row, const char* text) {
    char line[LCD_COLS + 1];
    int len = strlen(text);
    if (len > LCD_COLS) len = LCD_COLS;
    int pad = (LCD_COLS - len) / 2;
    snprintf(line, sizeof(line), "%*s%.*s", pad, "", len, text);
    lcd_print_line(row, line);
}

static bool i2c_device_present(i2c_inst_t* port, uint8_t addr) {
    uint8_t dummy;
    return i2c_read_blocking(port, addr, &dummy, 1, false) >= 0;
}

static bool lcd_init() {
    i2c_init(LCD_I2C_PORT, 100 * 1000);
    gpio_set_function(LCD_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(LCD_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(LCD_SDA_PIN);
    gpio_pull_up(LCD_SCL_PIN);

    if (i2c_device_present(LCD_I2C_PORT, 0x27))      lcd_addr = 0x27;
    else if (i2c_device_present(LCD_I2C_PORT, 0x3F)) lcd_addr = 0x3F;
    else return false;

    sleep_ms(50);
    lcd_write_nibble(0x30, 0); sleep_ms(5);
    lcd_write_nibble(0x30, 0); sleep_ms(5);
    lcd_write_nibble(0x30, 0); sleep_us(150);
    lcd_write_nibble(0x20, 0);   // modo 4 bits

    lcd_command(0x28);   // 2 linhas, fonte 5x8
    lcd_command(0x0C);   // display ligado, sem cursor
    lcd_command(0x06);   // cursor avança para a direita
    lcd_clear();
    return true;
}

// mostra um erro no LCD e trava
static void fatal_error(const char* l1, const char* l2) {
    lcd_print_line(0, l1);
    lcd_print_line(1, l2);
    while (true) tight_loop_contents();
}

int main() {
    sleep_ms(500);   // aguarda a alimentação do LCD estabilizar
    if (!lcd_init()) {
        while (true) tight_loop_contents();   // sem LCD não há onde mostrar erro
    }
    lcd_print_line(0, "Drone Fault Det.");
    lcd_print_line(1, "Iniciando...");
    

    // inicializa I2C
    i2c_init(I2C_PORT, 400 * 1000);   // 400 kHz
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
    if (i2c_device_present(I2C_PORT, ADXL345_ADDR_LOW))       adxl_addr = ADXL345_ADDR_LOW;
    else if (i2c_device_present(I2C_PORT, ADXL345_ADDR_HIGH)) adxl_addr = ADXL345_ADDR_HIGH;
    else fatal_error("ERRO: ADXL345", "nao encontrado");

    if (!adxl_init()) {
        fatal_error("ERRO: ADXL345", "config falhou");
    }
    lcd_print_line(1, "ADXL345 OK");

    // carrega o modelo TFLite
    const tflite::Model* tfl_model = tflite::GetModel(g_model);
    if (tfl_model->version() != TFLITE_SCHEMA_VERSION) {
        fatal_error("ERRO: modelo", "versao invalida");
    }

    // registra apenas as operações que o modelo usa
    static tflite::MicroMutableOpResolver<10> resolver;
    resolver.AddConv2D();
    resolver.AddDepthwiseConv2D();
    resolver.AddReshape();
    resolver.AddSoftmax();
    resolver.AddMean();
    resolver.AddMaxPool2D();
    resolver.AddFullyConnected();
    resolver.AddQuantize();
    resolver.AddExpandDims();
    resolver.AddDequantize();

    static tflite::MicroInterpreter interpreter(
        tfl_model, resolver, tensor_arena, TENSOR_ARENA_SIZE);

    if (interpreter.AllocateTensors() != kTfLiteOk) {
        fatal_error("ERRO: Allocate", "Tensors falhou");
    }

    TfLiteTensor* input  = interpreter.input(0);
    TfLiteTensor* output = interpreter.output(0);
    if (input->bytes != WINDOW_SIZE * N_AXES * sizeof(float)) {
        char msg[LCD_COLS + 1];
        snprintf(msg, sizeof(msg), "bytes: %u", (unsigned)input->bytes);
        fatal_error("ERRO: entrada", msg);
    }

    // mede a frequência real de amostragem (deve ficar perto de 790 Hz)
    // esvazia a FIFO para medir só amostras novas
    adxl_write_reg(REG_FIFO_CTL, 0x00);
    adxl_write_reg(REG_FIFO_CTL, 0x80);
    uint32_t t_start = time_us_32();
    adxl_collect_window();
    uint32_t t_end = time_us_32();
    float fs_real = WINDOW_SIZE / ((t_end - t_start) / 1e6f);

    // mede o tempo de uma inferência (precisa ser menor que a janela, ~324 ms)
    memset(input->data.f, 0, input->bytes);
    uint32_t t_inf = time_us_32();
    interpreter.Invoke();
    float ms_inf = (time_us_32() - t_inf) / 1000.0f;

    char line[LCD_COLS + 1];
    snprintf(line, sizeof(line), "fs: %.1f Hz", fs_real);
    lcd_print_line(0, line);
    snprintf(line, sizeof(line), "inf: %.1f ms", ms_inf);
    lcd_print_line(1, line);
    sleep_ms(3000);

    // loop principal
    while (true) {

        // coleta janela de tamanho WINDOW_SIZE pela FIFO
        adxl_collect_window();

        // motor parado: fora das condições de treino, não consulta o modelo
        if (window_rms() < LIMIAR_RMS) {
            lcd_print_centered(0, "MOTOR PARADO");
            lcd_print_line(1, "");
            continue;
        }

        // normaliza
        static float norm_buffer[WINDOW_SIZE][N_AXES];
        normalize_window(window_buffer, norm_buffer);

        // copia para o modelo
        float* input_data = input->data.f;
        for (int t = 0; t < WINDOW_SIZE; t++) {
            for (int c = 0; c < N_AXES; c++) {
                input_data[t * N_AXES + c] = norm_buffer[t][c];
            }
        }

        // inferência
        if (interpreter.Invoke() != kTfLiteOk) {
            lcd_print_line(0, "ERRO: Invoke");
            lcd_print_line(1, "");
            continue;
        }

        // lê probabilidades e acha a classe
        float* probs = output->data.f;
        int best = 0;
        for (int i = 1; i < N_CLASSES; i++) {
            if (probs[i] > probs[best]) best = i;
        }
        
        int conf = (int)(probs[best] * 100.0f + 0.5f);

        if (best == 0) {
            lcd_print_centered(0, "NORMAL");
            snprintf(line, sizeof(line), "conf %d%%", conf);
            lcd_print_centered(1, line);
        } else {
            lcd_print_centered(0, "*** ALERTA ***");
            snprintf(line, sizeof(line), "%-12s%3d%%", CLASS_NAMES[best], conf);
            lcd_print_line(1, line);
        }
    }

    return 0;
}