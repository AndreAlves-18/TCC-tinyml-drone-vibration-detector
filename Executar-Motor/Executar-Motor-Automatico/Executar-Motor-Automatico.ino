// WEMOS D1 Mini (ESP8266)
// Perfil automático de rotação para a coleta do dataset.
//
// O perfil é gerado a partir de uma semente fixa, então é exatamente o mesmo
// em todas as execuções: todas as classes recebem a mesma sequência de rotações.
//
// Uso:
//   1. Ligue com o potenciômetro no mínimo (o ESC arma em 5 s).
//   2. Inicie o capturar_dados.py.
//   3. Gire o potenciômetro até o máximo: o perfil começa (LED aceso).
//   4. Ao fim do perfil o motor para sozinho e o LED pisca. Pare a captura.
//   Para interromper a qualquer momento, baixe o potenciômetro até a metade.
//   Para repetir, leve o potenciômetro ao mínimo e depois ao máximo de novo.

#include <Servo.h>

Servo ESC;

// ---------------------------------------------------------------------------
// Configuração do perfil
// ---------------------------------------------------------------------------
const float    PCT_MIN    = 5.0;      // menor aceleração em que o motor gira estável (%)
const float    PCT_MAX    = 15.0;     // aceleração máxima (%)
const uint32_t DURACAO_MS = 600000;   // duração do perfil: 10 min
const uint32_t SEMENTE    = 12345;    // mesma semente = mesmo perfil sempre

// ---------------------------------------------------------------------------
// Perfil: lista de pontos (tempo, nível) ligados por retas.
// nível 0 = PCT_MIN, nível 1 = PCT_MAX
// ---------------------------------------------------------------------------
struct Ponto { uint32_t t; float nivel; };
const int MAX_PONTOS = 1000;
Ponto perfil[MAX_PONTOS];
int nPontos = 0;
uint32_t tMontagem = 0;

// gerador pseudoaleatório próprio (xorshift32): mesma sequência em toda execução
uint32_t estadoRng = SEMENTE;
float aleatorio() {                       // valor entre 0 e 1
  estadoRng ^= estadoRng << 13;
  estadoRng ^= estadoRng >> 17;
  estadoRng ^= estadoRng << 5;
  return (estadoRng & 0xFFFFFF) / 16777215.0f;
}
float entre(float a, float b) { return a + (b - a) * aleatorio(); }

void ponto(uint32_t dt_ms, float nivel) {
  if (nPontos >= MAX_PONTOS) return;
  tMontagem += dt_ms;
  perfil[nPontos++] = { tMontagem, nivel };
}

// Monta ciclos de ~2 min até cobrir a duração total. Cada ciclo tem:
//   rampas lentas, degraus com patamares e variações aleatórias.
void montaPerfil() {
  ponto(0, 0.0f);
  ponto(2000, 0.0f);                       // 2 s no mínimo para o motor estabilizar
  while (tMontagem < DURACAO_MS) {
    // 1. rampa lenta subindo e descendo (15 s cada)
    ponto(15000, 1.0f);
    ponto(15000, 0.0f);

    // 2. degraus: sobe e desce em 4 níveis, 4 s em cada
    const float degraus[] = {0.25f, 0.5f, 0.75f, 1.0f, 0.75f, 0.5f, 0.25f, 0.0f};
    for (float d : degraus) {
      ponto(300, d);                       // transição rápida
      ponto(4000, d);                      // patamar
    }

    // 3. variações aleatórias: 20 alvos com transições e patamares de duração variável
    for (int i = 0; i < 20; i++) {
      float alvo = aleatorio();
      ponto((uint32_t)entre(300, 2000), alvo);   // transição
      ponto((uint32_t)entre(500, 3000), alvo);   // patamar
    }
  }
}

// Nível do perfil no instante t (ms desde o início), por interpolação linear
float nivelNoTempo(uint32_t t) {
  static int i = 0;
  if (t < perfil[i].t) i = 0;
  while (i < nPontos - 2 && t >= perfil[i + 1].t) i++;
  Ponto a = perfil[i], b = perfil[i + 1];
  if (b.t <= a.t) return b.nivel;
  float f = (float)(t - a.t) / (float)(b.t - a.t);
  if (f > 1.0f) f = 1.0f;
  return a.nivel + (b.nivel - a.nivel) * f;
}

// ---------------------------------------------------------------------------
// Potenciômetro: liga, desliga e reinicia o perfil
// ---------------------------------------------------------------------------
float lerPot() {                           // 0 a 1
  long soma = 0;
  for (int i = 0; i < 5; i++) soma += analogRead(A0);
  return (soma / 5) / 1023.0f;
}

enum Estado { ESPERANDO, RODANDO, FIM };
Estado estado = ESPERANDO;
uint32_t inicioPerfil = 0;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);         // LED apagado (acende com LOW)

  ESC.attach(D5, 1000, 2000);              // D5 = GPIO14, seguro pra boot
  ESC.writeMicroseconds(1000);             // sinal mínimo para armar
  delay(5000);                             // tempo para o ESC armar

  montaPerfil();
}

void loop() {
  float pot = lerPot();

  switch (estado) {
    case ESPERANDO:
      ESC.writeMicroseconds(1000);
      if (pot > 0.95f) {                   // potenciômetro no máximo: começa
        inicioPerfil = millis();
        estado = RODANDO;
        digitalWrite(LED_BUILTIN, LOW);
      }
      break;

    case RODANDO: {
      uint32_t t = millis() - inicioPerfil;
      if (pot < 0.5f || t >= DURACAO_MS) { // interrompido ou terminou
        ESC.writeMicroseconds(1000);
        estado = FIM;
        break;
      }
      float pct = PCT_MIN + (PCT_MAX - PCT_MIN) * nivelNoTempo(t);
      ESC.writeMicroseconds(1000 + (int)(pct * 10.0f));   // 1% = 10 µs
      break;
    }

    case FIM:
      ESC.writeMicroseconds(1000);
      digitalWrite(LED_BUILTIN, (millis() / 250) % 2 ? LOW : HIGH);   // pisca
      if (pot < 0.05f) {                   // potenciômetro no mínimo: pronto para repetir
        estado = ESPERANDO;
        digitalWrite(LED_BUILTIN, HIGH);
      }
      break;
  }

  delay(10);
}
