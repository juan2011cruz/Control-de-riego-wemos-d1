#pragma once
#include <Arduino.h>

// ============================================================
// Estados principales (uno solo activo).
// Prioridad de transición: ERROR > TANQUE > PAUSA > MANUAL > AUTO > IDLE
// ============================================================
enum class Estado : uint8_t {
  BOOT,
  WIFI_CONNECT,
  AP,
  IDLE,
  AUTO,
  MANUAL,
  TANQUE,
  PAUSA,
  ERROR
};

enum class Sector : uint8_t {
  NINGUNO = 0,
  UNO = 1,
  DOS = 2
};

// Lista cerrada de fallas que llevan al estado ERROR.
enum class CodigoError : uint8_t {
  NINGUNO = 0,
  FALLA_NVS,              // no se pudo inicializar el almacenamiento persistente
  SECTORES_SIMULTANEOS,   // autochequeo de lógica: los 2 relés energizados a la vez
  PULSADOR_TRABADO,       // un pulsador permanece activo de forma continua > 10 min
  TANQUE_INCONSISTENTE    // reservado para cuando se agregue realimentación del tanque
};

constexpr uint8_t CANTIDAD_HORARIOS = 3;

// Un horario de riego: hora de disparo + duración propia por sector.
// duracionSector1Min == 0 && duracionSector2Min == 0  =>  horario desactivado.
struct HorarioRiego {
  uint8_t  hora               = 6;
  uint8_t  minuto              = 30;
  uint16_t duracionSector1Min = 0;
  uint16_t duracionSector2Min = 0;
};

// ---------------- Configuración persistente (EEPROM) ----------------
struct ConfigRiego {
  char         ssid[33]     = "";
  char         password[65] = "";
  HorarioRiego horarios[CANTIDAD_HORARIOS];
  bool         modoAutomatico = true;
};

extern ConfigRiego configRiego;
extern Estado estadoActual;
