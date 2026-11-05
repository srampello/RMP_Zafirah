# Changelog

Todos los cambios relevantes del proyecto Zafirah se documentan en este archivo.

## [0.3] - 2026-11-05

Ideas tomadas del firmware de [EVA-RT](https://github.com/EXOTIC-TEAM-MX/EVA-RT) (EXOTIC TEAM MX), con implementación propia.

### Cambiado
- Lectura de sensores propia, sin la librería QTRSensors: ADC de 8 bits a 1 MHz por registros.
  Los 8 sensores se leen en ~0,2 ms (antes ~0,7 ms).
- Calibración robusta: un máximo o mínimo solo se guarda si se repite en 10 muestras seguidas
  (cada una promedio de 8 lecturas). El LED parpadea mientras calibra.
- Umbral por sensor (`UMBRAL_PCT`, 50 %): lo que queda debajo cuenta como fondo y no entra en la posición.

### Agregado
- Recuperación fuera de línea: si ningún sensor ve la línea, el robot deja el PID y gira fuerte
  hacia el último lado donde la vio (`VEL_FUERA_EXT` 220 adelante, `VEL_FUERA_INT` 150 atrás).
- `LINEA_BLANCA` para elegir línea blanca o negra, y `DESCARTE_ADC` contra el arrastre entre canales.

### Pendiente
- Probar en pista. Ajustar `UMBRAL_PCT`, `VEL_FUERA_EXT` y `VEL_FUERA_INT`; la v0.2 tampoco se probó todavía.

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
