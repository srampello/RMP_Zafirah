# Zafirah — firmware del seguidor

Seguidor de línea blanca con PID para Arduino Nano, 8 × QRE1113GR y TB6612FNG.

## Requisitos

- Arduino IDE con placa **Arduino Nano** (ATmega328P).
- No necesita librerías: desde la v0.3 los sensores se leen con código propio (antes usaba QTRSensors 3.1.0).

## Uso

1. Encender y apretar el botón. A los 0,75 s el LED de estado empieza a parpadear y calibra durante 5 s: pasar toda la barra varias veces sobre la línea y el fondo, sin apuro (cada sensor tiene que quedar unos milisegundos quieto sobre la línea y sobre el fondo para que cuente).
2. Apoyar el robot sobre la línea y apretar el botón: se enciende el LED de largada.
3. Al soltar el botón arranca. La velocidad base sube en rampa, de 0 a `VEL_MAX` en ~0,34 s.
4. Mantener apretado el botón frena el robot. Al soltarlo reinicia el PID y vuelve a arrancar con rampa.

## Cómo funciona

### Sensores

- Cada sensor se lee con el ADC en **8 bits a 1 MHz** (~13 µs por conversión). Con `DESCARTE_ADC` se hace una conversión extra que se descarta al cambiar de canal, para que la lectura de un sensor no arrastre la del anterior. Los 8 sensores se leen en ~0,2 ms.
- **Calibración robusta:** cada paso toma 10 muestras, cada una promedio de 8 lecturas. Un máximo solo se guarda si las 10 muestras lo superan, y lo mismo con el mínimo. Un pico de ruido aislado no cambia la calibración.
- **Umbral:** cada lectura se pasa a una escala de 0 (fondo) a 255 (línea). Lo que queda por debajo de `UMBRAL_PCT` cuenta como fondo y vale 0; lo de arriba se reescala de 0 a 255. Solo los sensores que de verdad ven la línea entran en la posición.
- La posición es el promedio ponderado de esos sensores: 0 a 7000, centro en 3500.

### Control

- Cada 2 ms (`PERIODO_US`) se lee la posición y se calcula el PID.
- **Recta:** si el error se mantiene menor a `UMBRAL_RECTA` durante `TIEMPO_RECTA`, la base sube a `VEL_RECTA` y se enciende el LED de estado.
- **Curva:** si el error supera `UMBRAL_CURVA`, la base baja de golpe a `VEL_CURVA`.
- La base sube con rampa (`RAMPA` por ciclo) y baja al instante.
- Sobre la línea, cada rueda va de `-VEL_REVERSA` a 255: en curvas cerradas la rueda interna puede ir hacia atrás.
- **Fuera de línea:** si ningún sensor supera el umbral, el robot deja el PID y gira fuerte hacia el último lado donde vio la línea (el que la vio en los dos sensores del borde): rueda externa a `VEL_FUERA_EXT` hacia adelante e interna a `VEL_FUERA_INT` hacia atrás. Al reencontrarla vuelve al PID sin pico en la derivada.

## Parámetros

| Parámetro | Valor | Qué hace |
|---|---:|---|
| `LINEA_BLANCA` | true | `false` para línea negra sobre fondo blanco |
| `UMBRAL_PCT` | 50 | % entre fondo y línea desde el cual un sensor ve la línea |
| `DESCARTE_ADC` | true | Conversión extra descartada al cambiar de canal |
| `TIEMPO_CALIB` | 5000 | Duración de la calibración en ms |
| `CALIB_PROMEDIO` | 8 | Lecturas promediadas por muestra de calibración |
| `CALIB_MUESTRAS` | 10 | Muestras seguidas que tienen que coincidir |
| `PERIODO_US` | 2000 | Período del PID en µs |
| `VEL_MAX` | 170 | Velocidad base normal (PWM) |
| `VEL_RECTA` | 190 | Velocidad base en recta |
| `VEL_CURVA` | 135 | Velocidad base en curva fuerte (igual a `VEL_MAX` para desactivar) |
| `VEL_REVERSA` | 60 | Máxima reversa de la rueda interna sobre la línea (0 = sin reversa) |
| `VEL_FUERA_EXT` | 220 | Rueda externa al perder la línea (adelante) |
| `VEL_FUERA_INT` | 150 | Rueda interna al perder la línea (atrás) |
| `RAMPA` | 1 | PWM que sube la base en cada ciclo |
| `KP` | 0.033 | Proporcional |
| `KD` | 0.25 | Derivativo (por ciclo de 2 ms) |
| `KI` | 0.00007 | Integral (por ciclo de 2 ms) |
| `LIMITE_I` | 27500 | Tope del acumulador integral |
| `UMBRAL_RECTA` | 300 | Error máximo para considerar recta |
| `TIEMPO_RECTA` | 200 | ms centrado antes de acelerar |
| `UMBRAL_CURVA` | 2500 | Error a partir del cual frena |

KD, KI y `LIMITE_I` se reescalaron en la v0.2 (ciclo de ~5,5 ms a 2 ms). Si se cambia `PERIODO_US`, hay que volver a ajustar KD y KI. Con la lectura de ~0,2 ms se puede bajar a 1000 µs: en ese caso, duplicar KD y dividir KI por 2.

## Ajuste en pista

1. Probar primero con estas velocidades. Si oscila rápido en la recta, bajar KD; si se queda corto en las curvas, subir KP.
2. **Umbral:** con 50 usan la posición los sensores medio tapados por la línea. Si con luz fuerte o fondo gastado el robot "ve" línea donde no hay, subirlo a 60–75. Si la corrección se siente a saltos, bajarlo a 30–40.
3. **Fuera de línea:** si se pasa de largo en las curvas cerradas y tarda en volver, subir `VEL_FUERA_INT` y `VEL_FUERA_EXT` (EVA-RT usa 255 en las dos). Si al volver se pasa al otro lado, bajarlas.
4. Si en curvas cerradas gira raro o se clava, probar con `VEL_REVERSA 0`.
5. Subir `VEL_MAX` de a 10 y ajustar `VEL_CURVA` y `UMBRAL_CURVA` según la pista.

## Registros del microcontrolador

- `ADMUX`: referencia AVcc y resultado justificado a la izquierda (`ADLAR`): los 8 bits altos quedan en `ADCH`.
- `ADCSRA`: ADC habilitado con prescaler 16 (1 MHz). La precisión baja a ~8 bits, que es justo lo que se usa.
- `TCCR1B`: Timer1 sin prescaler, PWM de D9 y D10 a ~31 kHz. No afecta `millis()` ni `delay()`.
- Como el sketch maneja el ADC por registros, **no usar `analogRead()`**: cambia la configuración.

## Origen de las ideas

La lectura en 8 bits, la calibración robusta con umbral y la recuperación fuera de línea están inspiradas en el firmware de [EVA-RT](https://github.com/EXOTIC-TEAM-MX/EVA-RT), de EXOTIC TEAM MX. El código de Zafirah es propio, no copia el de EVA-RT.

Verificado: compila para ATmega328P sin advertencias (6,8 KB de flash) y la lectura de línea se probó en simulación. Pendiente de probar en pista.
