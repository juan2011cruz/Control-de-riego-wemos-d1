# Controlador de riego Wemos D1 Mini (ESP8266) - 2 sectores, 3 horarios

Proyecto PlatformIO. Abrir la carpeta `riego_d1mini/` en VS Code con la
extensión PlatformIO, o compilar con `pio run` desde la terminal.

## Mapa de pines

| Función                          | Pin  | GPIO  |
|-----------------------------------|------|-------|
| LED WiFi                          | D0   | 16    |
| Relé Sector 1                     | D1   | 5     |
| Relé Sector 2                     | D2   | 4     |
| Pulsador MARCHA 1 (NA)            | D3   | 0     |
| Pulsador MARCHA 2 (NA)            | D4   | 2     |
| LED ERROR                         | D5   | 14    |
| LED AUTO/PAUSA (combinado)        | D6   | 12    |
| Pulsador PARADA 1 (NC)            | D7   | 13    |
| Pulsador PARADA 2 (NC)            | D8   | 15    |
| Señal de tanque (analógica)       | A0   | -     |

RX/TX quedan completamente libres para el Monitor Serie.

**Por qué esta asignación y no otra**: D3/GPIO0 y D4/GPIO2 deben leer
HIGH al arrancar (o el ESP8266 entra en modo de descarga de firmware en
vez de arrancar normalmente) — por eso llevan los pulsadores de MARCHA,
que en reposo (no presionados) están en HIGH. D8/GPIO15 debe leer LOW al
arrancar, por eso lleva PARADA 2, que en reposo está en LOW. D0/GPIO16 no
tiene pull-up interno, por eso se usa como salida (LED) y no como
entrada.

## Simplificaciones de hardware respecto a la versión ESP32

- **Sin LED por sector**: se usa el LED que ya trae el propio módulo de
  relé. El pin del relé sigue siendo 1 solo GPIO por sector, igual que
  antes.
- **Sin LED de tanque en el ESP8266**: se alimenta directo desde el relé
  de llenado del tablero, sin pasar por la placa.
- **LED de AUTO y PAUSA combinados en un solo LED**: como esos dos
  estados nunca están activos a la vez, un mismo LED alcanza — fijo =
  riego automático corriendo, parpadeando = pausado.
- **Señal de tanque leída por A0** (analógica, con umbral e histéresis)
  en vez de una entrada digital dedicada, porque el D1 Mini solo tiene
  un pin analógico y hacía falta liberar uno digital.

## Horarios múltiples

Hasta 3 horarios independientes, cada uno con su propia hora y sus
propias duraciones de sector. Un horario con las dos duraciones en 0
queda desactivado (no hace falta un interruptor aparte). Cada horario
respeta las mismas reglas ya validadas en la versión ESP32: si la hora
programada de un horario ya pasó (ej. corte de luz), ese horario se
salta ese día; si el ciclo se interrumpe por PAUSA o TANQUE, se retoma
con el tiempo exacto restante al liberarse.

## Almacenamiento persistente

ESP8266 no tiene el NVS/`Preferences` de ESP32; acá se usa `EEPROM`
(emulada en flash) con una firma de 4 bytes al principio para distinguir
"placa recién flasheada, sin nada guardado todavía" (normal, no es un
error) de datos realmente corruptos.

## Watchdog

ESP8266 tiene un watchdog por software que el propio core de Arduino
alimenta automáticamente casi siempre; se llama `ESP.wdtFeed()`
explícitamente en cada vuelta de loop() y alrededor del escaneo de redes
WiFi (bloqueante) como refuerzo de seguridad.

## Registro por Serial

Igual que en la versión ESP32: imprime cada cambio de estado y los
eventos de WiFi por Serial a 115200 baudios (`pio device monitor -b
115200`).

## Caveats

- **Orden de arranque**: `webServerSetup()` siempre después de
  `wifiSetup()` (mismo motivo que en la versión ESP32: evitar problemas
  de la pila de red no inicializada).
- **Zona horaria**: `wifi_mgr.cpp` tiene `GMT_OFFSET_SEC = -3 * 3600`
  (Argentina). Ajustar si corresponde.
- **Autochequeo de "sectores simultáneos"**: solo detecta un bug de
  lógica propia, no un relé físicamente trabado (no hay contactos de
  realimentación en el hardware actual).
- **Sin margen de pines**: esta placa quedó con los 9 pines digitales y
  el único pin analógico completamente ocupados. Si más adelante querés
  agregar un sensor de lluvia/humedad, no hay pin libre — haría falta un
  expansor I2C (quedó descartado ahora a pedido tuyo, pero es la forma
  de sumar E/S sin cambiar de placa).
