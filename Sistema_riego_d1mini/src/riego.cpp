#include "riego.h"
#include "pines.h"
#include "config.h"
#include <time.h>

// ============================================================
// Estado interno del ciclo automático
// ============================================================
enum class FaseAuto : uint8_t { NINGUNA, SECTOR1, SECTOR2 };

static FaseAuto faseAuto = FaseAuto::NINGUNA;
static unsigned long finFaseMs = 0;
static int horarioEnCurso = -1;  // índice (0..2) del horario que originó el ciclo AUTO actual
static int ultimoDiaEjecutado[CANTIDAD_HORARIOS] = {-1, -1, -1};

struct CicloAutoGuardado {
  bool valido = false;
  FaseAuto fase = FaseAuto::NINGUNA;
  int horarioIndice = -1;
  unsigned long restanteMs = 0;
};
static CicloAutoGuardado autoGuardado;

// ============================================================
// Interlock de 2s entre sectores (aplica a web, físico y automático)
// ============================================================
static const unsigned long INTERLOCK_MS = 2000;
static Sector sectorActivoReal = Sector::NINGUNO;
static Sector sectorPendiente = Sector::NINGUNO;
static unsigned long tiempoActivacionPendiente = 0;

// ============================================================
// Error / tanque
// ============================================================
static CodigoError codigoError = CodigoError::NINGUNO;
static Estado estadoAntesDeTanque = Estado::IDLE;
static bool tanqueActivoActual = false;

// ============================================================
// Debounce de pulsadores
// ============================================================
struct Boton {
  uint8_t pin;
  bool activoEnAlto;
  bool estadoEstable = false;
  bool ultimaLecturaCruda = false;
  unsigned long ultimoCambioMs = 0;

  Boton(uint8_t p, bool activo) : pin(p), activoEnAlto(activo) {}
};

static const unsigned long DEBOUNCE_MS = 40;
static const unsigned long TIEMPO_MAX_PULSADOR_TRABADO_MS = 10UL * 60UL * 1000UL;  // 10 min

// PARADA es NC: "presionado" = línea abierta = HIGH (con pull-up, botón a GND).
// MARCHA es NA: "presionado" = línea a GND = LOW.
static Boton btnMarcha1 = {PIN_BTN_MARCHA1, false};
static Boton btnParada1 = {PIN_BTN_PARADA1, true};
static Boton btnMarcha2 = {PIN_BTN_MARCHA2, false};
static Boton btnParada2 = {PIN_BTN_PARADA2, true};

static bool actualizarBoton(Boton &b) {
  bool lecturaCruda = (digitalRead(b.pin) == HIGH);
  bool presionadoCrudo = (lecturaCruda == b.activoEnAlto);
  if (presionadoCrudo != b.ultimaLecturaCruda) {
    b.ultimaLecturaCruda = presionadoCrudo;
    b.ultimoCambioMs = millis();
  }
  if (millis() - b.ultimoCambioMs >= DEBOUNCE_MS) {
    b.estadoEstable = presionadoCrudo;
  }
  return b.estadoEstable;
}

// ============================================================
// Relés de sector (bajo nivel) + autochequeo de lógica
// ============================================================
static void escribirRele(Sector s, bool activar) {
  uint8_t pin = (s == Sector::UNO) ? PIN_RELE_SECTOR1 : PIN_RELE_SECTOR2;
  digitalWrite(pin, (activar == RELE_ACTIVO_EN_ALTO) ? HIGH : LOW);
}

static bool releEsta(Sector s) {
  uint8_t pin = (s == Sector::UNO) ? PIN_RELE_SECTOR1 : PIN_RELE_SECTOR2;
  return (digitalRead(pin) == HIGH) == RELE_ACTIVO_EN_ALTO;
}

void dispararError(CodigoError codigo) {
  escribirRele(Sector::UNO, false);
  escribirRele(Sector::DOS, false);
  sectorActivoReal = Sector::NINGUNO;
  sectorPendiente = Sector::NINGUNO;
  codigoError = codigo;
  estadoActual = Estado::ERROR;
}

static void activarReleSector(Sector s) {
  Sector otro = (s == Sector::UNO) ? Sector::DOS : Sector::UNO;
  if (releEsta(otro)) {
    // Autochequeo de una falla de lógica interna propia; no detecta un
    // relé físicamente trabado (para eso haría falta realimentación de
    // contactos, que este hardware no tiene).
    dispararError(CodigoError::SECTORES_SIMULTANEOS);
    return;
  }
  escribirRele(s, true);
}

static void detenerTodo() {
  escribirRele(Sector::UNO, false);
  escribirRele(Sector::DOS, false);
  sectorActivoReal = Sector::NINGUNO;
  sectorPendiente = Sector::NINGUNO;
}

// Activación unificada de un sector, con el interlock de 2s SIEMPRE
// aplicado, sin importar si el pedido viene de la web, de un pulsador
// físico o del ciclo automático.
static void activarSector(Sector s) {
  if (s == Sector::NINGUNO) {
    detenerTodo();
    return;
  }
  if (sectorActivoReal == s) return;
  detenerTodo();
  sectorPendiente = s;
  tiempoActivacionPendiente = millis() + INTERLOCK_MS;
}

static void actualizarInterlock() {
  if (sectorPendiente != Sector::NINGUNO && millis() >= tiempoActivacionPendiente) {
    Sector s = sectorPendiente;
    sectorPendiente = Sector::NINGUNO;
    activarReleSector(s);
    if (estadoActual != Estado::ERROR) {
      sectorActivoReal = s;
    }
  }
}

// ============================================================
// Guardado/restauración del ciclo automático (tanque y pausa)
// ============================================================
static void guardarCicloAutoSiActivo() {
  if (estadoActual == Estado::AUTO && faseAuto != FaseAuto::NINGUNA) {
    autoGuardado.valido = true;
    autoGuardado.fase = faseAuto;
    autoGuardado.horarioIndice = horarioEnCurso;
    long restante = (long)(finFaseMs - millis());
    autoGuardado.restanteMs = restante > 0 ? (unsigned long)restante : 0;
  } else {
    autoGuardado.valido = false;
  }
}

static void restaurarCicloAutoSiCorresponde() {
  if (!autoGuardado.valido) {
    faseAuto = FaseAuto::NINGUNA;
    horarioEnCurso = -1;
    estadoActual = Estado::IDLE;
    return;
  }
  estadoActual = Estado::AUTO;
  faseAuto = autoGuardado.fase;
  horarioEnCurso = autoGuardado.horarioIndice;
  Sector s = (faseAuto == FaseAuto::SECTOR1) ? Sector::UNO : Sector::DOS;
  activarSector(s);
  finFaseMs = tiempoActivacionPendiente + autoGuardado.restanteMs;
  autoGuardado.valido = false;
}

// ============================================================
// Tanque (leído por A0, umbral con zona muerta anti-ruido)
// ============================================================
static void entrarTanque() {
  if (estadoActual == Estado::ERROR) return;
  if (estadoActual == Estado::TANQUE) return;
  estadoAntesDeTanque = estadoActual;
  if (estadoActual == Estado::AUTO) {
    guardarCicloAutoSiActivo();
  }
  detenerTodo();
  estadoActual = Estado::TANQUE;
}

static void salirTanque() {
  if (estadoAntesDeTanque == Estado::PAUSA) {
    estadoActual = Estado::PAUSA;  // sigue pausado, no retoma solo
  } else if (estadoAntesDeTanque == Estado::AUTO) {
    restaurarCicloAutoSiCorresponde();
  } else {
    estadoActual = Estado::IDLE;  // MANUAL o IDLE: no se retoma solo
  }
}

static bool tanqueUltimaLecturaCruda = false;
static unsigned long tanqueUltimoCambio = 0;
static const unsigned long DEBOUNCE_TANQUE_MS = 300;  // algo mayor que en digital, por ruido de la lectura analógica
static bool esperandoLiberacionTanque = false;
static unsigned long tanqueLiberadoEnMs = 0;
static const unsigned long RETARDO_SALIDA_TANQUE_MS = 3000;

// En el ESP8266 el ADC (pin A0) comparte hardware con la calibración de RF
// del WiFi: llamar a analogRead() en cada vuelta del loop() (miles de
// veces por segundo) interfiere con esa calibración y puede impedir que
// WiFi.begin() complete la conexión. Se limita la lectura real del ADC a
// una cada 50ms (20 Hz de todos modos sobra frente al debounce de 300ms);
// entre lecturas se reutiliza el último valor.
static const unsigned long INTERVALO_LECTURA_ADC_MS = 50;
static unsigned long proximaLecturaAdcMs = 0;
static int ultimoValorAdcTanque = 0;

static bool leerTanqueCrudo() {
  if (millis() >= proximaLecturaAdcMs) {
    ultimoValorAdcTanque = analogRead(PIN_TANQUE_ACTIVO_A0);
    proximaLecturaAdcMs = millis() + INTERVALO_LECTURA_ADC_MS;
  }
  if (ultimoValorAdcTanque >= UMBRAL_TANQUE_ACTIVO) return true;
  if (ultimoValorAdcTanque <= UMBRAL_TANQUE_LIBRE) return false;
  return tanqueActivoActual;  // zona muerta: mantener el último estado estable
}

static void procesarSenalTanque() {
  bool crudo = leerTanqueCrudo();
  if (crudo != tanqueUltimaLecturaCruda) {
    tanqueUltimaLecturaCruda = crudo;
    tanqueUltimoCambio = millis();
  }
  if (millis() - tanqueUltimoCambio >= DEBOUNCE_TANQUE_MS && crudo != tanqueActivoActual) {
    tanqueActivoActual = crudo;
    if (tanqueActivoActual) {
      entrarTanque();
      esperandoLiberacionTanque = false;
    } else {
      esperandoLiberacionTanque = true;
      tanqueLiberadoEnMs = millis();
    }
  }

  if (esperandoLiberacionTanque && estadoActual == Estado::TANQUE &&
      millis() - tanqueLiberadoEnMs >= RETARDO_SALIDA_TANQUE_MS) {
    esperandoLiberacionTanque = false;
    salirTanque();
  }
}

// ============================================================
// Pausa
// ============================================================
void entrarPausa() {
  if (estadoActual == Estado::ERROR || estadoActual == Estado::TANQUE) return;
  if (estadoActual == Estado::PAUSA) return;
  if (estadoActual == Estado::AUTO) {
    guardarCicloAutoSiActivo();
  } else {
    autoGuardado.valido = false;
  }
  detenerTodo();
  estadoActual = Estado::PAUSA;
}

void salirPausa() {
  if (estadoActual != Estado::PAUSA) return;
  restaurarCicloAutoSiCorresponde();
}

// ============================================================
// Acciones de MARCHA/PARADA (pulsadores físicos y web)
// ============================================================
void accionPararSector(Sector s) {
  if (estadoActual == Estado::ERROR) return;
  if (estadoActual == Estado::TANQUE) return;
  if (estadoActual == Estado::PAUSA) return;
  if (sectorActivoReal == s || sectorPendiente == s) {
    detenerTodo();
    if (estadoActual == Estado::AUTO) {
      faseAuto = FaseAuto::NINGUNA;
      horarioEnCurso = -1;
      autoGuardado.valido = false;
    }
    estadoActual = Estado::IDLE;
  }
}

void accionMarchaSector(Sector s) {
  if (estadoActual == Estado::ERROR) return;
  if (estadoActual == Estado::TANQUE) return;
  if (estadoActual == Estado::PAUSA) return;
  if (estadoActual == Estado::AUTO) {
    faseAuto = FaseAuto::NINGUNA;
    horarioEnCurso = -1;
    autoGuardado.valido = false;
  }
  estadoActual = Estado::MANUAL;
  activarSector(s);
}

// ---- Combos de 2s para entrar/salir de PAUSA ----
static bool comboParadaEnCurso = false;
static unsigned long inicioComboParada = 0;
static bool comboMarchaEnCurso = false;
static unsigned long inicioComboMarcha = 0;
static const unsigned long HOLD_COMBO_MS = 2000;

// Ventana de gracia antes de ejecutar accionPararSector() individual.
// Sin esto, al mantener PARADA1+PARADA2 juntos para entrar en PAUSA, el
// flanco de subida de cada botón dispara la parada individual de
// inmediato: si había un ciclo AUTO en curso, accionPararSector() ya lo
// cancela (horarioEnCurso = -1, autoGuardado invalidado) antes de que se
// cumplan los 2s de la combo, así que entrarPausa() ya no tiene nada que
// guardar. Con la gracia, se espera un instante corto por si el otro
// PARADA también se presiona (combo), y en ese caso se cancela la acción
// individual y queda todo en manos de la combo.
static const unsigned long GRACIA_COMBO_PARADA_MS = 150;
static bool pendienteParada1 = false, pendienteParada2 = false;
static unsigned long inicioPendienteParada1 = 0, inicioPendienteParada2 = 0;

static void procesarBotones() {
  bool p1 = actualizarBoton(btnParada1);
  bool p2 = actualizarBoton(btnParada2);
  bool m1 = actualizarBoton(btnMarcha1);
  bool m2 = actualizarBoton(btnMarcha2);

  static bool p1Anterior = false, p2Anterior = false;
  if (p1 && !p1Anterior) { pendienteParada1 = true; inicioPendienteParada1 = millis(); }
  if (p2 && !p2Anterior) { pendienteParada2 = true; inicioPendienteParada2 = millis(); }
  p1Anterior = p1;
  p2Anterior = p2;

  if (p1 && p2) {
    // Combo en curso (ambos presionados a la vez dentro de la ventana de
    // gracia): se cancela cualquier acción individual pendiente.
    pendienteParada1 = false;
    pendienteParada2 = false;
  } else {
    // No hay combo. Si se cumplió la ventana de gracia mientras seguía
    // presionado, o si se soltó antes de cumplirla (toque rápido que
    // nunca llegó a coincidir con el otro PARADA), de todos modos se
    // ejecuta la parada individual — nunca se descarta una pulsación real.
    if (pendienteParada1 && (!p1 || millis() - inicioPendienteParada1 >= GRACIA_COMBO_PARADA_MS)) {
      accionPararSector(Sector::UNO);
      pendienteParada1 = false;
    }
    if (pendienteParada2 && (!p2 || millis() - inicioPendienteParada2 >= GRACIA_COMBO_PARADA_MS)) {
      accionPararSector(Sector::DOS);
      pendienteParada2 = false;
    }
  }

  static bool m1Anterior = false, m2Anterior = false;
  if (m1 && !m1Anterior) accionMarchaSector(Sector::UNO);
  if (m2 && !m2Anterior) accionMarchaSector(Sector::DOS);
  m1Anterior = m1;
  m2Anterior = m2;

  if (p1 && p2) {
    if (!comboParadaEnCurso) {
      comboParadaEnCurso = true;
      inicioComboParada = millis();
    } else if (millis() - inicioComboParada >= HOLD_COMBO_MS) {
      entrarPausa();
      comboParadaEnCurso = false;
    }
  } else {
    comboParadaEnCurso = false;
  }

  if (estadoActual == Estado::PAUSA && m1 && m2) {
    if (!comboMarchaEnCurso) {
      comboMarchaEnCurso = true;
      inicioComboMarcha = millis();
    } else if (millis() - inicioComboMarcha >= HOLD_COMBO_MS) {
      salirPausa();
      comboMarchaEnCurso = false;
    }
  } else {
    comboMarchaEnCurso = false;
  }
}

static void chequearPulsadoresTrabados() {
  unsigned long ahora = millis();
  Boton *botones[] = {&btnMarcha1, &btnParada1, &btnMarcha2, &btnParada2};
  for (Boton *b : botones) {
    if (b->estadoEstable && (ahora - b->ultimoCambioMs > TIEMPO_MAX_PULSADOR_TRABADO_MS)) {
      dispararError(CodigoError::PULSADOR_TRABADO);
    }
  }
}

// ============================================================
// Ciclo automático (3 horarios independientes)
// ============================================================
static void iniciarCicloAuto(int indice) {
  horarioEnCurso = indice;
  estadoActual = Estado::AUTO;
  faseAuto = FaseAuto::SECTOR1;
  activarSector(Sector::UNO);
  finFaseMs = tiempoActivacionPendiente +
              (unsigned long)configRiego.horarios[indice].duracionSector1Min * 60000UL;
}

static void chequearInicioAutomatico() {
  if (!configRiego.modoAutomatico) return;
  if (estadoActual != Estado::IDLE) return;
  struct tm t;
  if (!getLocalTime(&t, 0)) return;  // sin hora válida, no arranca

  for (uint8_t i = 0; i < CANTIDAD_HORARIOS; i++) {
    const HorarioRiego &h = configRiego.horarios[i];
    if (h.duracionSector1Min == 0 && h.duracionSector2Min == 0) continue;  // horario desactivado
    if (t.tm_hour == h.hora && t.tm_min == h.minuto) {
      if (ultimoDiaEjecutado[i] == t.tm_yday) continue;  // este horario ya se resolvió hoy
      ultimoDiaEjecutado[i] = t.tm_yday;
      iniciarCicloAuto(i);
      return;  // uno por vez; si otro coincidiera el mismo minuto, se evalúa el próximo loop ya en AUTO
    } else if (t.tm_hour > h.hora || (t.tm_hour == h.hora && t.tm_min > h.minuto)) {
      // Ya pasó la hora de este horario hoy (ej. corte de luz): se
      // salta, no se dispara fuera de horario.
      ultimoDiaEjecutado[i] = t.tm_yday;
    }
  }
}

static void actualizarCicloAuto() {
  if (estadoActual != Estado::AUTO) return;
  if (millis() < finFaseMs) return;
  if (faseAuto == FaseAuto::SECTOR1) {
    faseAuto = FaseAuto::SECTOR2;
    activarSector(Sector::DOS);
    finFaseMs = tiempoActivacionPendiente +
                (unsigned long)configRiego.horarios[horarioEnCurso].duracionSector2Min * 60000UL;
  } else {
    detenerTodo();
    faseAuto = FaseAuto::NINGUNA;
    horarioEnCurso = -1;
    estadoActual = Estado::IDLE;
  }
}

void reiniciarProgramacionAutomatica() {
  for (uint8_t i = 0; i < CANTIDAD_HORARIOS; i++) ultimoDiaEjecutado[i] = -1;
}

// ============================================================
// Ciclo de vida del módulo
// ============================================================
void riegoSetup() {
  pinMode(PIN_RELE_SECTOR1, OUTPUT);
  pinMode(PIN_RELE_SECTOR2, OUTPUT);
  escribirRele(Sector::UNO, false);
  escribirRele(Sector::DOS, false);

  pinMode(PIN_BTN_MARCHA1, INPUT_PULLUP);
  pinMode(PIN_BTN_PARADA1, INPUT_PULLUP);
  pinMode(PIN_BTN_MARCHA2, INPUT_PULLUP);
  pinMode(PIN_BTN_PARADA2, INPUT_PULLUP);
  // PIN_TANQUE_ACTIVO_A0 es analógico, no necesita pinMode.
}

void riegoLoop() {
  procesarSenalTanque();
  chequearPulsadoresTrabados();

  if (estadoActual == Estado::ERROR) {
    return;
  }
  if (estadoActual == Estado::TANQUE) {
    return;
  }

  procesarBotones();
  actualizarInterlock();

  if (estadoActual == Estado::PAUSA) {
    return;
  }

  chequearInicioAutomatico();
  actualizarCicloAuto();
}

// ============================================================
// Consultas (LEDs, servidor web)
// ============================================================
Sector sectorActivo() { return sectorActivoReal; }
CodigoError codigoErrorActual() { return codigoError; }
bool hayTanqueActivo() { return tanqueActivoActual; }

long tiempoRestanteSegundos() {
  if (estadoActual != Estado::AUTO || horarioEnCurso < 0) return -1;
  if (sectorPendiente != Sector::NINGUNO) {
    long faltaInterlock = (long)(tiempoActivacionPendiente - millis());
    if (faltaInterlock < 0) faltaInterlock = 0;
    long duracionFase = (faseAuto == FaseAuto::SECTOR1)
                             ? (long)configRiego.horarios[horarioEnCurso].duracionSector1Min * 60
                             : (long)configRiego.horarios[horarioEnCurso].duracionSector2Min * 60;
    return faltaInterlock / 1000 + duracionFase;
  }
  long resto = (long)(finFaseMs - millis()) / 1000;
  return resto > 0 ? resto : 0;
}
