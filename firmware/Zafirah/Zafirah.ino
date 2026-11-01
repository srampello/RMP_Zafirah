// Zafirah -  زافيرة   (v2)

// 1. Librerias y defines
// 2. Variables globales
// 3. Setup (calibración + espera de botón)
// 4. Loop (seguimiento de línea a ritmo fijo)
// 5. Funciones auxiliares
//
// Cambios respecto de la v1:
//  - Se lee el array UNA vez por ciclo (readLine ya llama a read() por dentro).
//  - ADC más rápido (prescaler 32): lectura de 8 sensores x 3 muestras ~0,7 ms en vez de ~2,7 ms.
//  - PID a ritmo fijo de 2 ms (antes ~5,5 ms y variable). KD/KI reescalados.
//  - PWM de los motores (pines 9 y 10, Timer1) a ~31 kHz en vez de ~490 Hz.
//  - La rueda interna puede ir hacia atrás en curvas cerradas (VEL_REVERSA).
//  - Se usa VEL_CURVA para frenar en curvas fuertes.
//  - Rampa de velocidad: arranque suave también al volver de una pausa.
//  - Corregido: al estar centrado pero antes de TIEMPO_RECTA, la base quedaba en 0.


// ---------------------------------------------------------
// 1. Librerías y Defines
// ---------------------------------------------------------
#include <QTRSensors.h>

// Sensores
#define NUM_SENSORS             8
#define NUM_SAMPLES_PER_SENSOR  3
#define EMITTER_PIN             QTR_NO_EMITTER_PIN
#define CENTRO                  3500

QTRSensorsAnalog qtra((unsigned char[]) {0, 1, 2, 3, 4, 5, 6, 7}, NUM_SENSORS, NUM_SAMPLES_PER_SENSOR, EMITTER_PIN);
unsigned int sensorValues[NUM_SENSORS];

// Lazo de control
#define PERIODO_US    2000   // El PID corre cada 2 ms fijos
#define TIEMPO_CALIB  5000   // ms de calibración (como antes, ~5 s)

// ---------------------------------------------------------
// 2. Variables globales
// ---------------------------------------------------------

// Velocidades (PWM 0-255)
#define VEL_MAX       170   // Velocidad normal
#define VEL_RECTA     190   // Velocidad en recta
#define VEL_CURVA     135   // Velocidad en curva fuerte (= VEL_MAX para desactivar)
#define VEL_REVERSA   60    // Cuánto puede ir para atrás la rueda interna (0 = como la v1)
#define RAMPA         1     // PWM que sube la base por ciclo (1 cada 2 ms -> 0 a 170 en ~0,34 s)

// PID (reescalado para ciclo de 2 ms: KD x2,75 / KI ÷2,75 respecto de la v1)
#define KP        0.033
#define KD        0.25
#define KI        0.00007
#define LIMITE_I  27500L

// Detección de recta y curva
#define UMBRAL_RECTA  300
#define TIEMPO_RECTA  200   // ms
#define UMBRAL_CURVA  2500

int  P = 0, D = 0, LAST = 0;
long I = 0;
float vel = 0;

int VelMax = 0;            // Velocidad base actual (con rampa)
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
  // ADC más rápido: prescaler 32 (500 kHz). Con 0x04 (prescaler 16) es todavía más rápido.
  ADCSRA = (ADCSRA & 0xF8) | 0x05;

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

  // Lectura (una sola vez: readLine ya lee los sensores)
  unsigned int position = qtra.readLine(sensorValues, QTR_EMITTERS_ON, 1); // 1 = línea blanca
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

  // Durante el arranque se limita la corrección como en la v1
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
// 5. Funciones auxiliares
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
  LAST = (int)qtra.readLine(sensorValues, QTR_EMITTERS_ON, 1) - CENTRO; // evita pico de D
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

// Calibración de sensores
void calibrar() {
  while (digitalRead(boton) == HIGH) {}
  delay(750);
  digitalWrite(led, HIGH);
  unsigned long t0 = millis();
  while (millis() - t0 < TIEMPO_CALIB) {
    qtra.calibrate();
  }
  digitalWrite(led, LOW);
}
