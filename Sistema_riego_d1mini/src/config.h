#pragma once

// Carga la configuración desde EEPROM hacia configRiego. En una placa
// recién flasheada (primer arranque, sin nada guardado todavía) escribe
// los valores por defecto en vez de tratarlo como una falla.
bool configCargar();

// Guarda el contenido actual de configRiego en EEPROM.
bool configGuardar();
