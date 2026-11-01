# Conexiones existentes — Zafirah

Documento de referencia rápida para las conexiones que usa el firmware actual.

## Arduino Nano

| Pin | Función | Modo |
|---:|---|---|
| A0–A7 | QRE1113GR 1 a 8 | Entrada analógica |
| D2 | Botón | `INPUT`, activo en LOW |
| D3 | LED de largada | Salida |
| D4 | TB6612FNG BIN1 | Salida |
| D5 | TB6612FNG BIN2 | Salida |
| D6 | TB6612FNG STBY | Salida, en HIGH durante todo el funcionamiento |
| D7 | TB6612FNG AIN1 | Salida |
| D8 | TB6612FNG AIN2 | Salida |
| D9 | TB6612FNG PWMA | PWM (Timer1) |
| D10 | TB6612FNG PWMB | PWM (Timer1) |
| D13 | LED de estado | Salida, encendido en recta |

## Sensores

- 8 × QRE1113GR leídos con `QTRSensorsAnalog` en A0–A7, sin pin de emisores (`QTR_NO_EMITTER_PIN`): los LED infrarrojos quedan siempre encendidos.
- A6 y A7 solo existen como entradas analógicas en el Nano: no sirven como pines digitales.
- Orden: A0 a la izquierda y A7 a la derecha, según el sentido de corrección del PID (con la línea bajo A7 la posición sube y el robot dobla a la derecha).
- Línea blanca (`readLine(..., 1)`). Posición de 0 a 7000, centro en 3500.

## Driver TB6612FNG

| Canal | Motor | Pines |
|---|---|---|
| A | Derecho | PWMA D9, AIN1 D7, AIN2 D8 |
| B | Izquierdo | PWMB D10, BIN1 D4, BIN2 D5 |

- Adelante: IN1 en HIGH, IN2 en LOW. Atrás: IN1 en LOW, IN2 en HIGH.
- Motores: 2 × N20 6 V 2000 RPM.
- Desde la v0.2 el PWM de D9 y D10 corre a ~31 kHz (Timer1 con prescaler 1). El TB6612FNG admite hasta 100 kHz.
- Timer1 queda reservado para los motores: no usar la librería `Servo` ni `analogWrite` en otros pines de Timer1.

## Botón

- Configurado como `INPUT` sin pull-up interno: el botón necesita un pull-up externo y cierra a GND.
- Primera pulsación: calibración. Segunda: largada al soltar. En carrera: frena mientras está apretado.
