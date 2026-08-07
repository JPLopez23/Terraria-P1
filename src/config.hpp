/**
 * config.hpp — parámetros del programa leídos de la línea de comandos.
 *
 * Regla de parseo: strtol/strtof verificando errno Y el puntero final —
 * nunca atoi, que ante basura devuelve 0 en silencio. Cada flag de CLI
 * (--kebab-case) mapea 1:1 a un campo snake_case de Config.
 */
#pragma once

#include <cstdint>
#include <string>

/** Códigos de salida distintos por tipo de error (programación defensiva). */
enum CodigoSalida {
    SALIDA_OK              = 0,
    SALIDA_ERROR_USO       = 2,   // flag desconocido o sin valor
    SALIDA_ERROR_VALOR     = 3,   // valor no numérico o fuera de rango
};

/** Config — todos los parámetros del programa. Sin valores hard-coded en el resto del código. */
struct Config {
    int         w           = 1280;     // --w: ancho de ventana en píxeles (mínimo 640)
    int         h           = 720;      // --h: alto de ventana en píxeles (mínimo 480)
    double      duration    = 0.0;      // --duration: segundos antes de salir (0 = infinito)
    bool        vsync       = false;    // --vsync: sincronizar con el refresco (¡apagado al medir!)
};

// lee y valida los argumentos del programa.
int parsearArgs(int argc, char** argv, Config& cfg);

// muestra la ayuda de la línea de comandos.
void imprimirUso(const char* nombrePrograma);
