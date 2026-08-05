#pragma once
#include <Arduino.h>

// ============================================================
// MAPA DE PINES - Wemos D1 Mini (ESP8266)
// Se usan los nombres serigrafiados en la placa (D0-D8, A0).
//
// Restricciones de arranque del ESP8266 que definieron esta asignación:
// - D3 (GPIO0) y D4 (GPIO2) deben leer HIGH al arrancar -> se les
//   asignaron los pulsadores de MARCHA, cuyo reposo (no presionado) es
//   HIGH (contacto NA con pull-up).
// - D8 (GPIO15) debe leer LOW al arrancar -> se le asignó PARADA 2,
//   cuyo reposo es LOW (contacto NC con pull-up: cerrado = GND).
// - D0 (GPIO16) no tiene pull-up interno, por eso se usa para una
//   salida (LED) y no para una entrada.
// - RX/TX quedan completamente libres para el Monitor Serie.
// - Los sectores ya no llevan LED propio: se usa el LED que trae el
//   propio módulo de relé. El LED de tanque queda fuera del ESP8266,
//   alimentado directo por el relé de llenado del tablero.
// ============================================================

// ---------------- Salidas ----------------
constexpr uint8_t PIN_LED_WIFI       = D0;  // GPIO16
constexpr uint8_t PIN_RELE_SECTOR1   = D1;  // GPIO5
constexpr uint8_t PIN_RELE_SECTOR2   = D2;  // GPIO4
constexpr uint8_t PIN_LED_ERROR      = D5;  // GPIO14
constexpr uint8_t PIN_LED_AUTO_PAUSA = D6;  // GPIO12 (fijo = AUTO, parpadea = PAUSA)
constexpr bool RELE_ACTIVO_EN_ALTO = false;  // ajustar según el módulo de relé usado

// ---------------- Entradas digitales ----------------
// MARCHA = contacto NA (reposo = HIGH). PARADA = contacto NC (reposo = LOW).
constexpr uint8_t PIN_BTN_MARCHA1 = D3;  // GPIO0  (debe estar HIGH en reposo)
constexpr uint8_t PIN_BTN_MARCHA2 = D4;  // GPIO2  (debe estar HIGH en reposo)
constexpr uint8_t PIN_BTN_PARADA1 = D7;  // GPIO13
constexpr uint8_t PIN_BTN_PARADA2 = D8;  // GPIO15 (debe estar LOW en reposo)

// ---------------- Señal de tanque (analógica) ----------------
// El D1 Mini solo tiene un pin analógico (A0, ya escalado en la placa
// a 0-3.3V), así que la señal del contacto auxiliar del tanque se lee
// por umbral en vez de por una entrada digital dedicada.
constexpr uint8_t PIN_TANQUE_ACTIVO_A0 = A0;
constexpr int UMBRAL_TANQUE_ACTIVO = 700;  // por encima de esto: se considera activo
constexpr int UMBRAL_TANQUE_LIBRE  = 300;  // por debajo de esto: se considera libre
// (zona intermedia = se mantiene el último estado estable, para no titilar por ruido)
