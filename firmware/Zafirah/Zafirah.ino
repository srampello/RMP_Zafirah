// Zafirah -  زافيرة

// 1. Librerias y defines
// 2. Variables globales
// 3. Setup (con espera de botón + arranque suave)
// 4. Loop (seguimiento de línea)
// 5. Funciones auxiliares


// ---------------------------------------------------------
// 1. Librerías y Defines
// ---------------------------------------------------------
#include <QTRSensors.h>

// Sensores
#define NUM_SENSORS   8
#define NUM_SAMPLES_PER_SENSOR  3
#define EMITTER_PIN   QTR_NO_EMITTER_PIN

QTRSensorsAnalog qtra((unsigned char[]) {0, 1, 2, 3, 4, 5, 6, 7}, NUM_SENSORS, NUM_SAMPLES_PER_SENSOR, EMITTER_PIN);
unsigned int sensorValues[NUM_SENSORS];
unsigned int position = 0;

// ---------------------------------------------------------
// 2. Variables globales
// ---------------------------------------------------------

// Velocidad
#define VEL_MAX       170  // Velocidad
#define VEL_RECTA     190   // Velocidad en recta
#define VEL_CURVA     135   // Velocidad en curva
#define VEL_ARRANQUE  100    

// PID
#define KP  0.033
#define KD  0.09
#define KI  0.0002

// Dereccion recta
#define UMBRAL_RECTA  300
#define TIEMPO_RECTA  200

int P = 0, D = 0, I = 0, LAST = 0;
float vel = 0;

// Velocidad
int VelMax = 0; // Velocidad maxima dinamica
int v_izq = 0;
int v_der = 0;

unsigned long tiempoRecta = 0;
bool enRecta = false;

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
  // Ajuste de PWM (Timer2 más rápido para mejor control de motores)
  //TCCR2B = TCCR2B & B11111000 | B00000011;  // PWM ~980 Hz 
  pinMode(led,  OUTPUT);
  pinMode(ledLargada, OUTPUT);
  pinMode(boton,  INPUT);  
  
  // TB6612FNG
  pinMode(PWMA, OUTPUT);  pinMode(AIN1, OUTPUT);  pinMode(AIN2, OUTPUT);
  pinMode(PWMB, OUTPUT);  pinMode(BIN1, OUTPUT);  pinMode(BIN2, OUTPUT);
  pinMode(STB,  OUTPUT);

  delay(250);
  digitalWrite (STB, HIGH); // Habilitamos el puente h
  digitalWrite(led, LOW);
  digitalWrite(ledLargada, LOW);
  

  calibrar(); // Calibramos los sensores
  configurarMotores(); // Configurar los motores                                                                
  delay(250);

   // ===== Espera al botón para arrancar =====
  while (digitalRead(boton) == HIGH) {
    detenerMotores();
  }

  // ===== Arranque suave =====
  digitalWrite(ledLargada, HIGH);
  arranqueSuave();
  VelMax = VEL_MAX;
}

// ---------------------------------------------------------
// 4. Loop principal
// ---------------------------------------------------------
void loop()
{
  // Pausa manual con boton
  while (digitalRead(boton) == LOW) {
    digitalWrite(ledLargada, LOW);
    detenerMotores();
  }
  // Lectura sensores y PID
  qtra.read(sensorValues); // Leemos la señal de los sensores
  position = qtra.readLine(sensorValues, QTR_EMITTERS_ON, 1); // 0 linea negra, 1 linea blanca                                                                                        
  P = (position - 3500);

  // PID
  D=(P - LAST);
  I += P;
  I = constrain(I, -10000, 10000);
  LAST = P;

  vel=(P*KP) + (D*KD) + (I*KI);

  v_der = VelMax - vel;
  v_izq = VelMax + vel;

  v_der = constrain(v_der, 0, 255);
  v_izq = constrain(v_izq, 0, 255);
  
  if(abs(P) < UMBRAL_RECTA){
    // Estamos cerca del centro
    if(!enRecta){
      tiempoRecta = millis();   //Inicio del tiempo en recta
      enRecta = true;
    }
    // Si pasaron mas de TIEMPO_RECTA en recta -> acelerar
    if(millis() - tiempoRecta > TIEMPO_RECTA){
      VelMax =  VEL_RECTA;
      digitalWrite(led,HIGH);
    }
  }else{
    // EL robot ya no esta en la recta
    VelMax = VEL_MAX;
    enRecta = false; 
    digitalWrite(led,LOW);
  }
  
  
  // Aplica velocidad Motores 
  analogWrite(PWMA,v_der);
  analogWrite(PWMB,v_izq);
  digitalWrite(AIN1,HIGH); digitalWrite(AIN2,LOW);
  digitalWrite(BIN1,HIGH); digitalWrite(BIN2,LOW);
}

// ---------------------------------------------------------
// 5. Funciones auxiliares
// ---------------------------------------------------------

// Arranque progresivo de motores
void arranqueSuave(){
  I = 0; LAST = 0; // Reset PID antes de arrancar
  for(int i=0; i<=VEL_ARRANQUE; i+=2){     // subir de a poco hasta ~200 PWM
    // Leer sensores y calcular error
    qtra.read(sensorValues);
    position = qtra.readLine(sensorValues, QTR_EMITTERS_ON, 1);
    P = position - 3500;

    D = (P - LAST);
    I += P;
    I = constrain(I, -10000, 10000);
    LAST = P;

    float vel = (P*KP) + (D*KD) + (I*KI);

    // Limitar vel con el ramp-up
    if(vel > i) vel = i;
    if(vel < -i) vel = -i;

    v_der = i - vel;
    v_izq = i + vel;

    if(v_der > 255) v_der = 255;
    if(v_izq > 255) v_izq = 255;

    analogWrite(PWMA, v_der);
    analogWrite(PWMB, v_izq);
    digitalWrite(AIN1,HIGH); digitalWrite(AIN2,LOW);
    digitalWrite(BIN1,HIGH); digitalWrite(BIN2,LOW);

    delay(1);  // suavidad del arranque
  }
}

void detenerMotores() {
  digitalWrite(led, LOW);
  analogWrite(PWMA, 0);
  analogWrite(PWMB, 0);
  VelMax = 0;
}

// Calibracion sensores
void calibrar (){
  while (digitalRead(boton) == HIGH) {}
  delay (750);
  digitalWrite(led, HIGH);
  for (int j = 0; j <200; j++){                                 
    qtra.calibrate();
  }
  digitalWrite(led, LOW);
}

// Configuracion motores (parados al inicio)
void configurarMotores(){
  digitalWrite(AIN1,LOW);
  digitalWrite(AIN2,HIGH);
  digitalWrite(BIN1,LOW);
  digitalWrite(BIN2,HIGH);
  analogWrite(PWMA,0);
  analogWrite(PWMB,0);
}



