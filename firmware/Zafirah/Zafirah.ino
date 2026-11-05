// Zafirah -  زافيرة   (v0.3)

// 1. Defines y parámetros
// 2. Variables globales
// 3. Setup (calibración + espera de botón)
// 4. Loop (seguimiento de línea a ritmo fijo)
// 5. Sensores (lectura propia del ADC, sin librería)
// 6. Motores y funciones auxiliares
//
// Cambios respecto de la v0.2 (ideas tomadas de EVA-RT, EXOTIC TEAM MX; implementación propia):
//  - Lectura de sensores propia: ADC de 8 bits a 1 MHz, sin la librería QTRSensors.
//    Los 8 sensores se leen en ~0,2 ms (antes ~0,7 ms).
//  - Calibración robusta: un máximo o mínimo solo se guarda si se repite en 10 lecturas
//    seguidas (cada una promedio de 8). Los picos de ruido no arruinan la calibración.
//  - Umbral por sensor (UMBRAL_PCT): lo que está por debajo cuenta como fondo (0) y no
//    entra en el cálculo de la posición.
//  - Recuperación fuera de línea: si ningún sensor ve la línea, gira fuerte hacia el
//    último lado donde la vio (VEL_FUERA_EXT / VEL_FUERA_INT).


// ---------------------------------------------------------
// 1. Defines y parámetros
// ---------------------------------------------------------

// Sensores
#define NUM_SENSORS     8          // A0 (izquierda) ... A7 (derecha)
#define CENTRO          3500       // (NUM_SENSORS - 1) * 1000 / 2
#define LINEA_BLANCA    true       // false = línea negra sobre fondo blanco
#define UMBRAL_PCT      50         // % entre fondo y línea a partir del cual un sensor "ve" la línea (EVA-RT usa 75)
#define DESCARTE_ADC    true       // Descarta la 1ª conversión de cada canal (evita que un sensor "contagie" al siguiente)

// Calibración
#define TIEMPO_CALIB    5000       // ms
#define CALIB_PROMEDIO  8          // lecturas promediadas por muestra
#define CALIB_MUESTRAS  10         // muestras seguidas que tienen que coincidir

// Lazo de control
#define PERIODO_US      2000       // El PID corre cada 2 ms fijos

// Velocidades (PWM 0-255)
#define VEL_MAX         170        // Velocidad normal
#define VEL_RECTA       190        // Velocidad en recta
#define VEL_CURVA       135        // Velocidad en curva fuerte (= VEL_MAX para desactivar)
#define VEL_REVERSA     60         // Cuánto puede ir para atrás la rueda interna sobre la línea (0 = sin reversa)
#define RAMPA           1          // PWM que sube la base por ciclo

// Fuera de línea: giro hacia el último lado visto
#define VEL_FUERA_EXT   220        // Rueda externa, hacia adelante
#define VEL_FUERA_INT   150        // Rueda interna, hacia atrás (EVA-RT usa 255 y 255)

// PID
#define KP        0.033
#define KD        0.25
#define KI        0.00007
#define LIMITE_I  27500L

// Detección de recta y curva
#define UMBRAL_RECTA  300
#define TIEMPO_RECTA  200          // ms
#define UMBRAL_CURVA  2500

// ---------------------------------------------------------
// 2. Variables globales
// ---------------------------------------------------------

uint8_t sensorValues[NUM_SENSORS]; // 0 = fondo, 255 = línea (después de calibrar)
uint8_t calMin[NUM_SENSORS];       // lectura cruda mínima (8 bits)
uint8_t calMax[NUM_SENSORS];       // lectura cruda máxima (8 bits)
uint8_t umbral[NUM_SENSORS];       // umbral en escala 0-255 de "línea"

bool enLinea = false;
bool ladoDerecho = false;          // último lado donde se vio la línea

int  P = 0, D = 0, LAST = 0;
long I = 0;
float vel = 0;

int VelMax = 0;                    // Velocidad base actual (con rampa)
int v_izq = 0;
int v_der = 0;

unsigned long tiempoRecta = 0;
bool enRecta = false;
bool arrancando = true;
unsigned long tUltimo = 0;

// TB6612FNG - Pines
int AIN1 = 7, AIN2 = 8, PWMA = 9;
int STB  = 6;
int BIN1 = 4, BIN2 = 5, PWMB = 10;

// LED y Boton
int led = 13;
int ledLargada = 3;
int boton = 2;

// ---------------------------------------------------------
// 3. Setup
// ---------------------------------------------------------
void setup() {
  // ADC: referencia AVcc, resultado justificado a la izquierda (8 bits en ADCH), prescaler 16 (1 MHz)
  ADMUX  = (1 << REFS0) | (1 << ADLAR);
  ADCSRA = (1 << ADEN) | (1 << ADPS2);

  // PWM de pines 9 y 10 (Timer1) a ~31 kHz. No afecta millis()/delay().
  TCCR1B = (TCCR1B & 0xF8) | 0x01;

  pinMode(led, OUTPUT);
  pinMode(ledLargada, OUTPUT);
  pinMode(boton, INPUT);

  // TB6612FNG
  pinMode(PWMA, OUTPUT);  pinMode(AIN1, OUTPUT);  pinMode(AIN2, OUTPUT);
  pinMode(PWMB, OUTPUT);  pinMode(BIN1, OUTPUT);  pinMode(BIN2, OUTPUT);
  pinMode(STB,  OUTPUT);

  delay(250);
  digitalWrite(STB, HIGH);   // Habilitamos el puente H
  digitalWrite(led, LOW);
  digitalWrite(ledLargada, LOW);
  detenerMotores();

  calibrar();                // Botón -> 0,75 s -> calibra TIEMPO_CALIB
  delay(250);

  // ===== Espera al botón para arrancar (arranca al soltarlo) =====
  while (digitalRead(boton) == HIGH) {}
  digitalWrite(ledLargada, HIGH);
  while (digitalRead(boton) == LOW) {}
  delay(20);                 // antirrebote

  reiniciarControl();        // El arranque suave lo hace la rampa
}

// ---------------------------------------------------------
// 4. Loop principal
// ---------------------------------------------------------
void loop() {
  // Pausa manual con botón (frena mientras está apretado, sigue al soltar)
  if (digitalRead(boton) == LOW) {
    digitalWrite(ledLargada, LOW);
    detenerMotores();
    while (digitalRead(boton) == LOW) {}
    delay(20);
    digitalWrite(ledLargada, HIGH);
    reiniciarControl();
  }

  // Ritmo fijo: el PID siempre corre cada PERIODO_US
  while (micros() - tUltimo < PERIODO_US) {}
  tUltimo = micros();

  unsigned int position = leerLinea();

  // ===== Fuera de línea: giro fuerte hacia el último lado =====
  if (!enLinea) {
    P = ladoDerecho ? CENTRO : -CENTRO;
    LAST = P;                // sin pico de D al volver
    I = 0;
    enRecta = false;
    digitalWrite(led, LOW);
    if (ladoDerecho) {       // la línea se fue a la derecha -> girar a la derecha
      motor(PWMA, AIN1, AIN2, -VEL_FUERA_INT);  // derecha atrás
      motor(PWMB, BIN1, BIN2,  VEL_FUERA_EXT);  // izquierda adelante
    } else {
      motor(PWMA, AIN1, AIN2,  VEL_FUERA_EXT);
      motor(PWMB, BIN1, BIN2, -VEL_FUERA_INT);
    }
    return;
  }

  P = (int)position - CENTRO;

  // PID
  D = P - LAST;
  I += P;
  I = constrain(I, -LIMITE_I, LIMITE_I);
  LAST = P;
  vel = (P * KP) + (D * KD) + (I * KI);

  // Velocidad base objetivo según recta / curva
  int objetivo = VEL_MAX;
  if (abs(P) < UMBRAL_RECTA) {
    if (!enRecta) {
      tiempoRecta = millis();
      enRecta = true;
    }
    if (millis() - tiempoRecta > TIEMPO_RECTA) objetivo = VEL_RECTA;
  } else {
    enRecta = false;
    if (abs(P) > UMBRAL_CURVA) objetivo = VEL_CURVA;
  }
  digitalWrite(led, objetivo == VEL_RECTA ? HIGH : LOW);

  // Sube con rampa, baja de golpe (frena enseguida al entrar a una curva)
  if (VelMax < objetivo) VelMax = min(VelMax + RAMPA, objetivo);
  else                   VelMax = objetivo;

  // Durante el arranque se limita la corrección
  if (arrancando) {
    if (VelMax >= objetivo) arrancando = false;
    else vel = constrain(vel, -VelMax, VelMax);
  }

  int c = (int)vel;
  v_der = constrain(VelMax - c, -VEL_REVERSA, 255);
  v_izq = constrain(VelMax + c, -VEL_REVERSA, 255);

  motor(PWMA, AIN1, AIN2, v_der);
  motor(PWMB, BIN1, BIN2, v_izq);
}

// ---------------------------------------------------------
// 5. Sensores
// ---------------------------------------------------------

// Una conversión de 8 bits en el canal indicado (~13 µs)
uint8_t adc8(uint8_t canal) {
  ADMUX = (ADMUX & 0xF0) | canal;
  if (DESCARTE_ADC) {
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC)) {}
  }
  ADCSRA |= (1 << ADSC);
  while (ADCSRA & (1 << ADSC)) {}
  return ADCH;
}

// Lectura cruda de los 8 sensores
void leerCrudo(uint8_t *v) {
  for (uint8_t i = 0; i < NUM_SENSORS; i++) v[i] = adc8(i);
}

// Pasa la lectura cruda a 0-255 (255 = línea) y aplica el umbral
void leerCalibrado() {
  uint8_t raw[NUM_SENSORS];
  leerCrudo(raw);
  enLinea = false;
  for (uint8_t i = 0; i < NUM_SENSORS; i++) {
    int rango = calMax[i] - calMin[i];
    int n;                                  // "cuánto de línea" ve el sensor, 0-255
    if (rango <= 0) n = 0;
    else if (LINEA_BLANCA) n = (long)(calMax[i] - (int)raw[i]) * 255 / rango;  // blanco = lectura baja
    else                   n = (long)((int)raw[i] - calMin[i]) * 255 / rango;
    n = constrain(n, 0, 255);

    if (n > umbral[i]) {
      sensorValues[i] = (long)(n - umbral[i]) * 255 / (255 - umbral[i]);
      if (sensorValues[i] == 0) sensorValues[i] = 1;
      enLinea = true;
    } else {
      sensorValues[i] = 0;
    }
  }
  if (enLinea) {
    if (sensorValues[0] || sensorValues[1]) ladoDerecho = false;
    else if (sensorValues[NUM_SENSORS - 1] || sensorValues[NUM_SENSORS - 2]) ladoDerecho = true;
  }
}

// Posición de la línea 0-7000 (promedio ponderado de los sensores sobre el umbral)
unsigned int leerLinea() {
  leerCalibrado();
  unsigned long suma = 0, pond = 0;
  for (uint8_t i = 0; i < NUM_SENSORS; i++) {
    if (sensorValues[i]) {
      pond += (unsigned long)sensorValues[i] * i * 1000;
      suma += sensorValues[i];
    }
  }
  if (!suma) return ladoDerecho ? 2 * CENTRO : 0;
  return pond / suma;
}

// Un paso de calibración: 10 muestras (promedio de 8 lecturas cada una).
// Solo se actualiza un máximo/mínimo si las 10 muestras lo superan: descarta picos.
void calibrarPaso() {
  uint8_t alto[NUM_SENSORS], bajo[NUM_SENSORS];
  for (uint8_t j = 0; j < CALIB_MUESTRAS; j++) {
    uint16_t acum[NUM_SENSORS] = {0};
    uint8_t raw[NUM_SENSORS];
    for (uint8_t k = 0; k < CALIB_PROMEDIO; k++) {
      leerCrudo(raw);
      for (uint8_t i = 0; i < NUM_SENSORS; i++) acum[i] += raw[i];
    }
    for (uint8_t i = 0; i < NUM_SENSORS; i++) {
      uint8_t m = (acum[i] + CALIB_PROMEDIO / 2) / CALIB_PROMEDIO;
      if (j == 0 || m > alto[i]) alto[i] = m;
      if (j == 0 || m < bajo[i]) bajo[i] = m;
    }
  }
  for (uint8_t i = 0; i < NUM_SENSORS; i++) {
    if (bajo[i] > calMax[i]) calMax[i] = bajo[i];
    if (alto[i] < calMin[i]) calMin[i] = alto[i];
  }
}

// Calibración de sensores
void calibrar() {
  for (uint8_t i = 0; i < NUM_SENSORS; i++) { calMin[i] = 255; calMax[i] = 0; }

  while (digitalRead(boton) == HIGH) {}
  delay(750);
  unsigned long t0 = millis();
  while (millis() - t0 < TIEMPO_CALIB) {
    digitalWrite(led, (millis() / 100) % 2);   // parpadea mientras calibra
    calibrarPaso();
  }
  digitalWrite(led, LOW);

  for (uint8_t i = 0; i < NUM_SENSORS; i++) umbral[i] = (uint16_t)255 * UMBRAL_PCT / 100;
}

// ---------------------------------------------------------
// 6. Motores y funciones auxiliares
// ---------------------------------------------------------

// v > 0 adelante, v < 0 atrás
void motor(int pwm, int in1, int in2, int v) {
  if (v >= 0) {
    digitalWrite(in1, HIGH); digitalWrite(in2, LOW);
    analogWrite(pwm, v);
  } else {
    digitalWrite(in1, LOW);  digitalWrite(in2, HIGH);
    analogWrite(pwm, -v);
  }
}

// Reset del PID y de la rampa (arranque y vuelta de pausa)
void reiniciarControl() {
  I = 0;
  LAST = (int)leerLinea() - CENTRO;          // evita pico de D
  VelMax = 0;
  enRecta = false;
  arrancando = true;
  tUltimo = micros();
}

void detenerMotores() {
  digitalWrite(led, LOW);
  motor(PWMA, AIN1, AIN2, 0);
  motor(PWMB, BIN1, BIN2, 0);
  VelMax = 0;
}
