#include "wifi_mgr.h"
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <time.h>
#include "estado.h"
#include "config.h"

static DNSServer dnsServer;
static bool enModoAP = false;

static unsigned long inicioIntentoConexion = 0;
static const unsigned long TIMEOUT_CONEXION_MS = 20000;

static unsigned long ultimoIntentoReconexion = 0;
static const unsigned long INTERVALO_RECONEXION_MS = 30000;

static const char* NTP_SERVER = "pool.ntp.org";

static void iniciarAP() {
  WiFi.persistent(false);
  WiFi.disconnect();
  delay(100);

  WiFi.mode(WIFI_AP);
  delay(100);

  IPAddress ip(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  
  WiFi.softAPConfig(ip, gateway, subnet);

  if (WiFi.softAP("RIEGO-ESP8266", "12345678")) {
    Serial.println("[WIFI] AP iniciado correctamente. Red: RIEGO-ESP8266");
  } else {
    Serial.println("[WIFI] Error al iniciar AP");
  }

  // Iniciar servidor DNS en el puerto 53 redirigiendo todo a la IP del ESP8266
  dnsServer.start(53, "*", ip);
  enModoAP = true;
  
  if (estadoActual != Estado::ERROR) {
    estadoActual = Estado::AP;
  }
}

void wifiSetup() {
  WiFi.persistent(false);

  // Si no hay SSID guardado en la configuración, levantamos el AP de inmediato
  if (strlen(configRiego.ssid) == 0) {
    Serial.println("[WIFI] Sin SSID guardado, iniciando Modo AP...");
    iniciarAP();
    return;
  }
  
  Serial.print("[WIFI] Intentando conectar a: ");
  Serial.println(configRiego.ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(configRiego.ssid, configRiego.password);
  
  inicioIntentoConexion = millis();
  if (estadoActual != Estado::ERROR) {
    estadoActual = Estado::WIFI_CONNECT;
  }
}

void wifiLoop() {
  if (enModoAP) {
    dnsServer.processNextRequest();
    return;
  }

  if (estadoActual == Estado::WIFI_CONNECT) {
    if (WiFi.status() == WL_CONNECTED) {
      configTime(-3 * 3600, 0, NTP_SERVER); 
      Serial.print("[WIFI] Conectado exitosamente. IP: ");
      Serial.println(WiFi.localIP());
      estadoActual = Estado::IDLE;
    } else if (millis() - inicioIntentoConexion > TIMEOUT_CONEXION_MS) {
      Serial.println("[WIFI] Timeout de conexión STA, pasando a modo AP...");
      iniciarAP();
    }
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - ultimoIntentoReconexion > INTERVALO_RECONEXION_MS) {
      ultimoIntentoReconexion = millis();
      WiFi.disconnect();
      WiFi.begin(configRiego.ssid, configRiego.password);
    }
  }
}

bool wifiConectado() {
  return WiFi.status() == WL_CONNECTED;
}

bool wifiEnModoAP() {
  return enModoAP;
}

bool horaValidaNTP() {
  struct tm t;
  time_t now;
  time(&now);
  localtime_r(&now, &t);
  return (t.tm_year + 1900) > 2020;
}