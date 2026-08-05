#include "config.h"
#include "estado.h"
#include <EEPROM.h>

ConfigRiego configRiego;

// Firma de 4 bytes al principio de la EEPROM: si no coincide, significa
// que la placa nunca guardó nada (primer arranque) o que el contenido
// no es reconocible - en cualquier caso NO es una falla real, se
// arranca con los valores por defecto (igual que aprendimos con el NVS
// del ESP32: no confundir "todavía no se guardó nada" con un error).
static const uint32_t FIRMA_VALIDA = 0x52494731;  // "RIG1"
static const int DIRECCION_FIRMA = 0;
static const int DIRECCION_DATOS = sizeof(uint32_t);
static const int TAMANIO_EEPROM = DIRECCION_DATOS + sizeof(ConfigRiego);

bool configCargar() {
  EEPROM.begin(TAMANIO_EEPROM);
  uint32_t firma = 0;
  EEPROM.get(DIRECCION_FIRMA, firma);
  if (firma != FIRMA_VALIDA) {
    configRiego = ConfigRiego();  // valores por defecto
    return configGuardar();
  }
  EEPROM.get(DIRECCION_DATOS, configRiego);
  return true;
}

bool configGuardar() {
  EEPROM.put(DIRECCION_FIRMA, FIRMA_VALIDA);
  EEPROM.put(DIRECCION_DATOS, configRiego);
  return EEPROM.commit();
}
