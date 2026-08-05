#pragma once
#include "estado.h"

void riegoSetup();
void riegoLoop();

// Acciones expuestas para pulsadores físicos y para el servidor web.
void accionMarchaSector(Sector s);
void accionPararSector(Sector s);
void entrarPausa();
void salirPausa();
void dispararError(CodigoError codigo);

// Se llama al guardar horarios desde la web: olvida qué horarios ya se
// resolvieron hoy, para que un horario recién cambiado pueda dispararse
// el mismo día.
void reiniciarProgramacionAutomatica();

// Consultas para LEDs y servidor web.
Sector sectorActivo();
long tiempoRestanteSegundos();  // -1 si no aplica (no hay ciclo automático corriendo)
CodigoError codigoErrorActual();
bool hayTanqueActivo();
