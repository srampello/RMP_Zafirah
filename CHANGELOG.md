# Changelog

Todos los cambios relevantes del proyecto Zafirah se documentan en este archivo.

## [0.2] - 2026-11-01

### Cambiado
- `firmware/Zafirah`: el PID corre a ritmo fijo cada 2 ms (antes ~5,5 ms y variable).
  KD 0,09 → 0,25, KI 0,0002 → 0,00007 y límite de I 10000 → 27500, reescalados para el ciclo nuevo.
- Los sensores se leen una sola vez por ciclo: `readLine()` ya llama a `read()` por dentro.
- ADC con prescaler 32: leer los 8 sensores × 3 muestras baja de ~2,7 ms a ~0,7 ms.
- PWM de los motores (D9 y D10, Timer1) a ~31 kHz en vez de ~490 Hz.
- La calibración dura 5 s por tiempo, en lugar de 200 llamadas a `calibrate()`.
- El arranque suave se reemplaza por una rampa de velocidad que también actúa al volver de una pausa.
- El robot arranca al soltar el botón.

### Agregado
- Reversa de la rueda interna en curvas cerradas (`VEL_REVERSA`).
- Freno en curva fuerte con `VEL_CURVA` y `UMBRAL_CURVA` (antes `VEL_CURVA` estaba definida pero no se usaba).

### Corregido
- Con el robot centrado, antes de cumplirse `TIEMPO_RECTA`, la velocidad base quedaba en 0 después
  de una pausa y luego saltaba de golpe a la velocidad de recta.
- La línea comentada `TCCR2B` apuntaba al Timer2, que no maneja los pines de los motores.

### Pendiente
- Probar en pista y ajustar KP, KD, `VEL_REVERSA` y las velocidades.

## [0.1] - 2026-11-01

### Inicial
- Firmware original: PID con QTRSensors, velocidad extra en recta, arranque suave y pausa con el botón.
