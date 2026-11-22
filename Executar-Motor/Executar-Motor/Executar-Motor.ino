// WEMOS D1 Mini (ESP8266)

#include <Servo.h>

Servo ESC;

const int LIMITE_PCT = 15;   // aceleração máxima no potenciômetro (%)

// retorna a largura do pulso do ESC em microssegundos
int lerPot() {
  long soma = 0;
  for (int i = 0; i < 5; i++) soma += analogRead(A0);
  return map(soma / 5, 0, 1023, 1000, 1000 + LIMITE_PCT * 10);
}

void setup() {
  ESC.attach(D5, 1000, 2000);        // D5 = GPIO14
  ESC.write(0);
}

void loop() {
  ESC.writeMicroseconds(lerPot());
  delay(10);
}