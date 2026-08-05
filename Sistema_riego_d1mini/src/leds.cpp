#include "leds.h"
#include "pines.h"
#include "estado.h"
#include "wifi_mgr.h"

struct Parpadeo {
  unsigned long ultimoCambio = 0;
  bool encendido = false;
};

static Parpadeo pWifi, pError, pAutoPausa;

static void parpadear(uint8_t pin, Parpadeo &p, unsigned long periodoMs) {
  if (millis() - p.ultimoCambio >= periodoMs) {
    p.ultimoCambio = millis();
    p.encendido = !p.encendido;
    digitalWrite(pin, p.encendido ? HIGH : LOW);
  }
}

void ledsSetup() {
  pinMode(PIN_LED_WIFI, OUTPUT);
  pinMode(PIN_LED_ERROR, OUTPUT);
  pinMode(PIN_LED_AUTO_PAUSA, OUTPUT);
}

void todosLosLedsOn() {
  digitalWrite(PIN_LED_WIFI, HIGH);
  digitalWrite(PIN_LED_ERROR, HIGH);
  digitalWrite(PIN_LED_AUTO_PAUSA, HIGH);
}

void todosLosLedsOff() {
  digitalWrite(PIN_LED_WIFI, LOW);
  digitalWrite(PIN_LED_ERROR, LOW);
  digitalWrite(PIN_LED_AUTO_PAUSA, LOW);
}

void ledsLoop() {
  // ---- WIFI ----
  if (estadoActual == Estado::WIFI_CONNECT) {
    parpadear(PIN_LED_WIFI, pWifi, 300);
  } else if (wifiEnModoAP()) {
    parpadear(PIN_LED_WIFI, pWifi, 150);
  } else if (wifiConectado()) {
    digitalWrite(PIN_LED_WIFI, HIGH);
  } else {
    digitalWrite(PIN_LED_WIFI, LOW);
  }

  // ---- AUTO / PAUSA combinados en un solo LED (nunca activos a la vez) ----
  if (estadoActual == Estado::AUTO) {
    digitalWrite(PIN_LED_AUTO_PAUSA, HIGH);
  } else if (estadoActual == Estado::PAUSA) {
    parpadear(PIN_LED_AUTO_PAUSA, pAutoPausa, 400);
  } else {
    digitalWrite(PIN_LED_AUTO_PAUSA, LOW);
  }

  // ---- ERROR: parpadeo rápido = sin hora válida, fijo = error crítico ----
  if (estadoActual == Estado::ERROR) {
    digitalWrite(PIN_LED_ERROR, HIGH);
  } else if (wifiConectado() && !horaValidaNTP()) {
    parpadear(PIN_LED_ERROR, pError, 150);
  } else {
    digitalWrite(PIN_LED_ERROR, LOW);
  }
}
