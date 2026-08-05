#include "web_server.h"
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <time.h>
#include <cstdio>
#include "estado.h"
#include "config.h"
#include "riego.h"
#include "wifi_mgr.h"

static ESP8266WebServer server(80);

// Credenciales de acceso a la web
static const char* USUARIO_WEB = "Juan2011cruz";
static const char* PASSWORD_WEB = "Cable4821"; // ¡Cámbiala por una segura!

// ---------------- Utilidades de texto optimizadas ----------------
static const __FlashStringHelper* nombreEstado(Estado e) {
  switch (e) {
    case Estado::BOOT: return F("Arrancando");
    case Estado::WIFI_CONNECT: return F("Conectando WiFi");
    case Estado::AP: return F("Modo configuracion");
    case Estado::IDLE: return F("En espera");
    case Estado::AUTO: return F("Riego automatico");
    case Estado::MANUAL: return F("Riego manual");
    case Estado::TANQUE: return F("Llenando tanque");
    case Estado::PAUSA: return F("Pausado");
    case Estado::ERROR: return F("Error");
  }
  return F("?");
}

static const __FlashStringHelper* nombreSector(Sector s) {
  if (s == Sector::UNO) return F("Sector 1");
  if (s == Sector::DOS) return F("Sector 2");
  return F("Ninguno");
}

static const __FlashStringHelper* nombreError(CodigoError c) {
  switch (c) {
    case CodigoError::FALLA_NVS: return F("Fallo de almacenamiento");
    case CodigoError::SECTORES_SIMULTANEOS: return F("Fallo logico");
    case CodigoError::PULSADOR_TRABADO: return F("Pulsador trabado");
    case CodigoError::TANQUE_INCONSISTENTE: return F("Tanque inconsistente");
    default: return F("");
  }
}

// ---------------- Página principal con diseño exacto y optimizada en Flash ----------------
static const char PAGINA_PRINCIPAL[] PROGMEM = R"HTML(<!DOCTYPE html><html lang="es"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>BR Riego</title>
<link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap" rel="stylesheet">
<style>
:root{--bg:#0b0f19;--sur:#111827;--card:#161f30;--bor:#2a374a;--txt:#f3f4f6;--mut:#9ca3af;--pri:#1d4ed8;--pri-h:#2563eb;--suc:#16a34a;--dan:#dc2626;--war:#d97706;--inf:#0284c7}
*{box-sizing:border-box}body{font-family:'Inter',sans-serif;background:var(--bg);color:var(--txt);margin:0;padding:16px;display:flex;justify-content:center}
.c{width:100%;max-width:440px}
.header{display:flex;align-items:center;gap:12px;margin-bottom:16px}
.logo{background:#1d4ed8;width:40px;height:40px;border-radius:10px;display:flex;align-items:center;justify-content:center;font-size:1.2rem}
.h-title{font-size:1.2rem;font-weight:700;margin:0;letter-spacing:0.5px}
.h-sub{font-size:0.8rem;color:var(--mut);margin:0}
.card{background:var(--sur);border:1px solid var(--bor);border-radius:14px;padding:16px;margin-bottom:12px}
.main-status{display:flex;justify-content:space-between;align-items:center}
.status-info .st-title{font-size:0.95rem;font-weight:700;display:flex;align-items:center;gap:8px}
.dot{width:10px;height:10px;border-radius:50%;background:var(--suc);display:inline-block}
.status-info .st-sub{font-size:0.75rem;color:var(--mut);margin-top:4px}
.status-time{text-align:right}
.status-time .t-hora{font-size:1.15rem;font-weight:700}
.status-time .t-fecha{font-size:0.7rem;color:var(--mut)}

.grid-3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:8px;margin-bottom:12px}
.mini-card{background:var(--card);border:1px solid var(--bor);border-radius:10px;padding:10px;font-size:0.8rem}
.mini-card .mc-tit{color:var(--mut);font-size:0.75rem;margin-bottom:4px;display:flex;align-items:center;gap:4px}
.mini-card .mc-val{font-weight:600;font-size:0.85rem}
.mini-card .mc-sub{font-size:0.7rem;color:var(--mut)}

.grid-2{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-bottom:12px}

.sec-title{font-size:0.75rem;font-weight:700;color:var(--mut);text-transform:uppercase;letter-spacing:0.5px;margin-bottom:8px}

.grid-btns{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-bottom:8px}
button{padding:12px;border:none;border-radius:10px;font-size:0.9rem;font-weight:600;cursor:pointer;color:#fff;width:100%;display:flex;align-items:center;justify-content:center;gap:6px}
.b1{background:var(--suc)}.b2{background:var(--suc)}.stop{background:var(--dan);grid-column:span 2}
.pausa{background:var(--war)}.reanudar{background:var(--inf)}.reiniciar{background:#374151}
button:active{opacity:0.9}

.auto-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:12px}
.switch{position:relative;display:inline-block;width:44px;height:24px}
.switch input{opacity:0;width:0;height:0}
.slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#374151;transition:.3s;border-radius:24px}
.slider:before{position:absolute;content:"";height:18px;width:18px;left:3px;bottom:3px;background-color:white;transition:.3s;border-radius:50%}
input:checked+.slider{background-color:var(--suc)}
input:checked+.slider:before{transform:translateX(20px)}

.horarios-grid{display:grid;grid-template-columns:1fr 1fr 1fr;gap:6px;margin-bottom:12px}
.horario-card{background:var(--card);border:1px solid var(--bor);border-radius:10px;padding:8px;font-size:0.75rem}
.horario-card .hc-tit{font-weight:600;margin-bottom:4px;color:var(--mut)}
.horario-card input{width:100%;padding:4px;background:var(--bg);border:1px solid var(--bor);color:#fff;border-radius:4px;font-size:0.75rem;margin-top:2px;text-align:center}
.horario-card label{font-size:0.65rem;color:var(--mut);display:block;margin-top:4px}

.btn-guardar{background:var(--pri);margin-top:4px}
.footer-text{text-align:center;font-size:0.7rem;color:var(--mut);margin-top:16px}
</style></head><body><div class="c">

<div class="header">
  <div class="logo">💧</div>
  <div>
    <h1 class="h-title">BR RIEGO</h1>
    <p class="h-sub">Controlador de Riego</p>
  </div>
</div>

<div class="card">
  <div class="main-status">
    <div class="status-info">
      <div class="st-title"><span class="dot"></span><span id="st-text">EN ESPERA</span></div>
      <div class="st-sub">Sistema listo</div>
    </div>
    <div class="status-time">
      <div class="t-hora" id="t-hora">--:--:--</div>
      <div class="t-fecha" id="t-fecha">--/--/----</div>
    </div>
  </div>
</div>

<div class="grid-3">
  <div class="mini-card">
    <div class="mc-tit">WiFi</div>
    <div class="mc-val" id="wifi-rssi" style="display:flex;align-items:flex-end;gap:2px;height:18px;margin-top:2px">-</div>
    <div class="mc-sub" id="wifi-stat">Estado</div>
  </div>
  <div class="mini-card">
    <div class="mc-tit">IP Local</div>
    <div class="mc-val" id="ip-val" style="font-size:0.7rem;margin-top:4px">---.---</div>
    <div class="mc-sub">Conectado</div>
  </div>
  <div class="mini-card">
    <div class="mc-tit">Tanque</div>
    <div class="mc-val" id="tanque-val">Libre</div>
    <div class="mc-sub">Nivel OK</div>
  </div>
</div>

<div class="grid-2">
  <div class="mini-card">
    <div class="mc-tit">Sector activo</div>
    <div class="mc-val" id="sector-val">Ninguno</div>
  </div>
  <div class="mini-card">
    <div class="mc-tit">Tiempo restante</div>
    <div class="mc-val" id="restante-val">--:--</div>
  </div>
</div>

<div class="card">
  <div class="sec-title">Control Manual</div>
  <div class="grid-btns">
    <button class="b1" onclick="c('sector1')">Sector 1</button>
    <button class="b2" onclick="c('sector2')">Sector 2</button>
    <button class="pausa" onclick="c('pausa')">Pausa</button>
    <button class="reanudar" onclick="c('reanudar')">Reanudar</button>
    <button class="stop" onclick="c('stop')">DETENER TODO</button>
  </div>
</div>

<div class="card">
  <div class="auto-header">
    <div class="sec-title" style="margin-bottom:0">Riego Automático</div>
    <label class="switch"><input type="checkbox" id="auto"><span class="slider"></span></label>
  </div>
  
  <div class="horarios-grid">
    <div class="horario-card">
      <div class="hc-tit">Horario 1</div>
      <input type="time" id="h0">
      <label>Sec 1 (min)</label><input type="number" id="s1_0" min="0">
      <label>Sec 2 (min)</label><input type="number" id="s2_0" min="0">
    </div>
    <div class="horario-card">
      <div class="hc-tit">Horario 2</div>
      <input type="time" id="h1">
      <label>Sec 1 (min)</label><input type="number" id="s1_1" min="0">
      <label>Sec 2 (min)</label><input type="number" id="s2_1" min="0">
    </div>
    <div class="horario-card">
      <div class="hc-tit">Horario 3</div>
      <input type="time" id="h2">
      <label>Sec 1 (min)</label><input type="number" id="s1_2" min="0">
      <label>Sec 2 (min)</label><input type="number" id="s2_2" min="0">
    </div>
  </div>
  
  <button class="button btn-guardar" onclick="g()">Guardar Configuración</button>
</div>

<div class="card" style="padding:10px">
  <button class="reiniciar" onclick="if(confirm('¿Reiniciar controlador?'))c('reiniciar')">Reiniciar Controlador</button>
</div>

<div class="footer-text">Firmware v1.0 | BR Riego D1 Mini</div>

</div><script>
function c(x){fetch('/comando',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'accion='+x}).then(r);}
function g(){
let b='auto='+(document.getElementById('auto').checked?1:0);
for(let i=0;i<3;i++){
let v=document.getElementById('h'+i).value.split(':');
b+='&h'+i+'='+(v[0]||0)+'&m'+i+'='+(v[1]||0)+'&s1_'+i+'='+document.getElementById('s1_'+i).value+'&s2_'+i+'='+document.getElementById('s2_'+i).value;
}
fetch('/guardar',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b}).then(()=>{alert('Guardado');r();});
}
function r(){
fetch('/estado').then(x=>x.json()).then(d=>{
document.getElementById('st-text').innerText=d.e;
document.getElementById('t-hora').innerText=d.ho;

let rssi = d.r;
let barras = 0;
if (rssi >= -55) barras = 4;
else if (rssi >= -65) barras = 3;
else if (rssi >= -75) barras = 2;
else if (rssi >= -85) barras = 1;
else barras = 0;

let htmlSignal = '<div style="display:flex;align-items:flex-end;gap:2px;height:14px;margin-top:2px;">';
for(let i=1; i<=4; i++) {
    let h = i * 3.5;
    let color = i <= barras ? 'var(--suc)' : 'var(--bor)';
    htmlSignal += `<div style="width:4px;height:${h}px;background:${color};border-radius:1px;"></div>`;
}
htmlSignal += `</div><span style="font-size:0.65rem;color:var(--mut);margin-left:4px;">(${rssi}dBm)</span>`;
document.getElementById('wifi-rssi').innerHTML = htmlSignal;

document.getElementById('wifi-stat').innerText=d.w;
document.getElementById('ip-val').innerText=d.i;
document.getElementById('tanque-val').innerText=d.t;
document.getElementById('sector-val').innerText=d.s;
document.getElementById('restante-val').innerText=d.re;

let now=new Date();
let f=String(now.getDate()).padStart(2,'0')+'/'+String(now.getMonth()+1).padStart(2,'0')+'/'+now.getFullYear();
document.getElementById('t-fecha').innerText=f;

if(!window.ic){
document.getElementById('auto').checked=d.a;
for(let i=0;i<3;i++){
document.getElementById('h'+i).value=d.hr[i].h;
document.getElementById('s1_'+i).value=d.hr[i].s1;
document.getElementById('s2_'+i).value=d.hr[i].s2;
}
window.ic=true;
}
});
}
r();setInterval(r,2000);
</script></body></html>)HTML";

// Función auxiliar para validar la autenticación
static bool autenticar() {
  return server.authenticate(USUARIO_WEB, PASSWORD_WEB);
}

static void manejarRaiz() {
  if (!autenticar()) {
    return server.requestAuthentication();
  }
  server.send_P(200, "text/html", PAGINA_PRINCIPAL);
}

static void manejarEstado() {
  if (!autenticar()) {
    server.send(401, "text/plain", "Unauthorized");
    return;
  }

  struct tm t;
  char horaBuf[9] = "--:--:--";
  if (getLocalTime(&t, 10)) {
    strftime(horaBuf, sizeof(horaBuf), "%H:%M:%S", &t);
  }

  long restanteSeg = tiempoRestanteSegundos();
  char restanteBuf[12];
  if (restanteSeg < 0) {
    snprintf(restanteBuf, sizeof(restanteBuf), "-");
  } else {
    snprintf(restanteBuf, sizeof(restanteBuf), "%ld:%02ld", restanteSeg / 60, restanteSeg % 60);
  }

  String json = "{";
  json += "\"e\":\"" + String(nombreEstado(estadoActual)) + "\",";
  json += "\"ho\":\"" + String(horaBuf) + "\",";
  json += "\"w\":\"" + String(wifiConectado() ? "Conectado" : "Desconectado") + "\",";
  json += "\"r\":" + String(WiFi.RSSI()) + ",";
  json += "\"i\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"t\":\"" + String(hayTanqueActivo() ? "Activo" : "Libre") + "\",";
  json += "\"s\":\"" + String(nombreSector(sectorActivo())) + "\",";
  json += "\"re\":\"" + String(restanteBuf) + "\",";
  json += "\"a\":" + String(configRiego.modoAutomatico ? "true" : "false") + ",";
  json += "\"err\":\"" + String(nombreError(codigoErrorActual())) + "\",";
  json += "\"hr\":[";
  for (uint8_t i = 0; i < CANTIDAD_HORARIOS; i++) {
    char hBuf[6];
    snprintf(hBuf, sizeof(hBuf), "%02u:%02u", configRiego.horarios[i].hora, configRiego.horarios[i].minuto);
    json += "{\"h\":\"" + String(hBuf) + "\",";
    json += "\"s1\":" + String(configRiego.horarios[i].duracionSector1Min) + ",";
    json += "\"s2\":" + String(configRiego.horarios[i].duracionSector2Min) + "}";
    if (i < CANTIDAD_HORARIOS - 1) json += ",";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

static void manejarComando() {
  if (!autenticar()) {
    server.send(401, "text/plain", "Unauthorized");
    return;
  }
  if (!server.hasArg("accion")) {
    server.send(400, "text/plain", "Falta accion");
    return;
  }
  String accion = server.arg("accion");
  if (accion == "sector1") accionMarchaSector(Sector::UNO);
  else if (accion == "sector2") accionMarchaSector(Sector::DOS);
  else if (accion == "stop") { accionPararSector(Sector::UNO); accionPararSector(Sector::DOS); }
  else if (accion == "pausa") entrarPausa();
  else if (accion == "reanudar") salirPausa();
  else if (accion == "reiniciar") {
    server.send(200, "text/plain", "OK");
    delay(200);
    ESP.restart();
    return;
  }
  server.send(200, "text/plain", "OK");
}

static void manejarGuardar() {
  if (!autenticar()) {
    server.send(401, "text/plain", "Unauthorized");
    return;
  }
  if (server.hasArg("auto")) configRiego.modoAutomatico = (server.arg("auto").toInt() == 1);
  for (uint8_t i = 0; i < CANTIDAD_HORARIOS; i++) {
    String si = String(i);
    if (server.hasArg("h" + si)) configRiego.horarios[i].hora = server.arg("h" + si).toInt();
    if (server.hasArg("m" + si)) configRiego.horarios[i].minuto = server.arg("m" + si).toInt();
    if (server.hasArg("s1_" + si)) configRiego.horarios[i].duracionSector1Min = server.arg("s1_" + si).toInt();
    if (server.hasArg("s2_" + si)) configRiego.horarios[i].duracionSector2Min = server.arg("s2_" + si).toInt();
  }
  configGuardar();
  reiniciarProgramacionAutomatica();
  server.send(200, "text/plain", "OK");
}

static void manejarRaizAP() {
  yield();
  int n = WiFi.scanNetworks();
  yield();
  String opciones = "";
  for (int i = 0; i < n; i++) {
    opciones += "<option value=\"" + WiFi.SSID(i) + "\">" + WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + "dBm)</option>";
  }
  
  String pagina = F("<!DOCTYPE html><html lang=\"es\"><head><meta charset=\"UTF-8\">"
                    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                    "<title>Configurar WiFi</title>"
                    "<link href=\"https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap\" rel=\"stylesheet\">"
                    "<style>"
                    "body{font-family:'Inter',sans-serif;background:#0b0f19;color:#f3f4f6;padding:16px;display:flex;justify-content:center}"
                    ".c{width:100%;max-width:380px;background:#111827;border:1px solid #2a374a;border-radius:14px;padding:20px}"
                    "h2{font-size:1.1rem;margin-top:0;margin-bottom:14px}"
                    "select,input{width:100%;padding:10px;margin-bottom:14px;background:#0b0f19;color:#fff;border:1px solid #2a374a;border-radius:8px;font-size:0.9rem}"
                    "button{background:#1d4ed8;color:#fff;border:none;padding:12px;border-radius:8px;width:100%;font-weight:600;font-size:0.9rem;cursor:pointer}"
                    "label{font-size:0.8rem;color:#9ca3af;display:block;margin-bottom:4px}"
                    "</style></head><body><div class=\"c\"><h2>Configurar Red WiFi</h2>"
                    "<form method=\"POST\" action=\"/wifi_guardar\">"
                    "<label>Red:</label><select name=\"ssid\">");
  pagina += opciones;
  pagina += F("</select><label>Contraseña:</label><input type=\"password\" name=\"pass\">"
              "<button type=\"submit\">Guardar y Reiniciar</button></form></div></body></html>");
  server.send(200, "text/html", pagina);
}

static void manejarWifiGuardar() {
  if (server.hasArg("ssid")) strlcpy(configRiego.ssid, server.arg("ssid").c_str(), sizeof(configRiego.ssid));
  if (server.hasArg("pass")) strlcpy(configRiego.password, server.arg("pass").c_str(), sizeof(configRiego.password));
  configGuardar();
  server.send(200, "text/plain", "OK");
  delay(200);
  ESP.restart();
}

void webServerSetup() {
  server.on("/", HTTP_GET, []() {
    if (wifiEnModoAP()) manejarRaizAP();
    else manejarRaiz();
  });
  server.on("/estado", HTTP_GET, manejarEstado);
  server.on("/comando", HTTP_POST, manejarComando);
  server.on("/guardar", HTTP_POST, manejarGuardar);
  server.on("/wifi_guardar", HTTP_POST, manejarWifiGuardar);
  server.onNotFound([]() {
    if (wifiEnModoAP()) manejarRaizAP();
    else server.send(404, "text/plain", "Not Found");
  });
  server.begin();
}

void webServerLoop() {
  server.handleClient();
}