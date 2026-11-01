# Zafirah — firmware del seguidor

Seguidor de línea blanca con PID para Arduino Nano, 8 × QRE1113GR y TB6612FNG.

## Requisitos

- Arduino IDE con placa **Arduino Nano** (ATmega328P).
- Librería **QTRSensors 3.1.0** de Pololu. La 4.x cambió la API (`QTRSensorsAnalog` ya no existe) y no compila con este sketch.

## Uso

1. Encender y apretar el botón. A los 0,75 s se enciende el LED de estado y calibra durante 5 s: pasar toda la barra sobre la línea y el fondo.
2. Apoyar el robot sobre la línea y apretar el botón: se enciende el LED de largada.
3. Al soltar el botón arranca. La velocidad base sube en rampa, de 0 a `VEL_MAX` en ~0,34 s.
4. Mantener apretado el botón frena el robot. Al soltarlo reinicia el PID y vuelve a arrancar con rampa.

## Cómo funciona

- Cada 2 ms (`PERIODO_US`) se lee la posición de la línea (0–7000, centro 3500) y se calcula el PID.
- **Recta:** si el error se mantiene menor a `UMBRAL_RECTA` durante `TIEMPO_RECTA`, la base sube a `VEL_RECTA` y se enciende el LED de estado.
- **Curva:** si el error supera `UMBRAL_CURVA`, la base baja de golpe a `VEL_CURVA`.
- La base sube con rampa (`RAMPA` por ciclo) y baja al instante.
- Cada rueda va de `-VEL_REVERSA` a 255: en curvas cerradas la rueda interna puede ir hacia atrás.

## Parámetros

| Parámetro | Valor | Qué hace |
|---|---:|---|
| `PERIODO_US` | 2000 | Período del PID en µs |
| `TIEMPO_CALIB` | 5000 | Duración de la calibración en ms |
| `VEL_MAX` | 170 | Velocidad base normal (PWM) |
| `VEL_RECTA` | 190 | Velocidad base en recta |
| `VEL_CURVA` | 135 | Velocidad base en curva fuerte (igual a `VEL_MAX` para desactivar) |
| `VEL_REVERSA` | 60 | Máxima reversa de la rueda interna (0 = sin reversa) |
| `RAMPA` | 1 | PWM que sube la base en cada ciclo |
| `KP` | 0.033 | Proporcional |
| `KD` | 0.25 | Derivativo (por ciclo de 2 ms) |
| `KI` | 0.00007 | Integral (por ciclo de 2 ms) |
| `LIMITE_I` | 27500 | Tope del acumulador integral |
| `UMBRAL_RECTA` | 300 | Error máximo para considerar recta |
| `TIEMPO_RECTA` | 200 | ms centrado antes de acelerar |
| `UMBRAL_CURVA` | 2500 | Error a partir del cual frena |

KD, KI y `LIMITE_I` se reescalaron desde la v0.1 (ciclo de ~5,5 ms) para que el comportamiento inicial sea equivalente. Si se cambia `PERIODO_US`, hay que volver a ajustar KD y KI.

## Ajuste en pista

1. Probar primero con estas velocidades. Si oscila rápido en la recta, bajar KD; si se queda corto en las curvas, subir KP.
2. Si en curvas cerradas gira raro o se clava, probar con `VEL_REVERSA 0`.
3. Subir `VEL_MAX` de a 10 y ajustar `VEL_CURVA` y `UMBRAL_CURVA` según la pista.

## Registros del microcontrolador

- `ADCSRA`: prescaler 32 (ADC a 500 kHz). Con `0x04` (prescaler 16) lee todavía más rápido, con algo menos de precisión.
- `TCCR1B`: Timer1 sin prescaler, PWM de D9 y D10 a ~31 kHz. No afecta `millis()` ni `delay()`.

Verificado: compila para ATmega328P con QTRSensors 3.1.0. Pendiente de probar en pista.
