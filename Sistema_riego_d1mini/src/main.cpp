#include <Arduino.h>
#include "estado.h"
#include "config.h"
#include "leds.h"
#include "wifi_mgr.h"
#include "riego.h"
#include "web_server.h"

// Definición única del estado global (declarado extern en estado.h)
Estado estadoActual = Estado::BOOT;

static unsigned long inicioBoot = 0;
static const unsigned long DURACION_BOOT_MS = 1000;
static Estado ultimoEstadoImpreso = Estado::BOOT;

static const char* nombreEstadoDebug(Estado e) {
  switch (e) {
    case Estado::BOOT: return "BOOT";
    case Estado::WIFI_CONNECT: return "WIFI_CONNECT";
    case Estado::AP: return "AP";
    case Estado::IDLE: return "IDLE";
    case Estado::AUTO: return "AUTO";
    case Estado::MANUAL: return "MANUAL";
    case Estado::TANQUE: return "TANQUE";
    case Estado::PAUSA: return "PAUSA";
    case Estado::ERROR: return "ERROR";
  }
  return "?";
}

void setup() {
  Serial.begin(115200);
  delay(200);  // da tiempo a que el monitor serie enganche los primeros mensajes
  Serial.println("\n[BOOT] Controlador de riego (D1 Mini) arrancando...");

  riegoSetup();  // deja pines de relés/entradas en un estado seguro sí o sí
  ledsSetup();

  bool configOk = configCargar();
  Serial.print("[BOOT] configCargar() -> ");
  Serial.println(configOk ? "OK" : "FALLÓ");
  if (configOk) {
    Serial.print("[BOOT] SSID guardado: ");
    Serial.println(strlen(configRiego.ssid) ? configRiego.ssid : "(vacío)");
  }

  todosLosLedsOn();
  inicioBoot = millis();

  if (!configOk) {
    // Falla real de almacenamiento (raro en la práctica: EEPROM.begin()
    // casi no falla en ESP8266). Se va directo a modo AP igual, ya que
    // tampoco hay SSID confiable disponible, y se bloquea el riego.
    wifiSetup();       // primero WiFi, deja lista la pila de red
    webServerSetup();  // recién ahora es seguro abrir el servidor web
    dispararError(CodigoError::FALLA_NVS);
  }
}

void loop() {
  ESP.wdtFeed();  // alimenta el watchdog por software del ESP8266

  if (estadoActual == Estado::BOOT) {
    if (millis() - inicioBoot >= DURACION_BOOT_MS) {
      todosLosLedsOff();
      wifiSetup();       // primero WiFi, deja lista la pila de red
      webServerSetup();  // recién ahora arranca el servidor web
    }
  } else {
    wifiLoop();
    riegoLoop();
    webServerLoop();
  }

  ledsLoop();

  if (estadoActual != ultimoEstadoImpreso) {
    ultimoEstadoImpreso = estadoActual;
    Serial.print("[ESTADO] -> ");
    Serial.println(nombreEstadoDebug(estadoActual));
  }
}
