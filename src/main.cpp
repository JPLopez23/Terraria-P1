/**
 * main.cpp — Terraria Forge: screensaver de mundo 2D con iluminación por
 * ray tracing de sombras, en versión secuencial y paralela con OpenMP.
 *
 * Universidad del Valle de Guatemala · Computación Paralela y Distribuida.
 *
 * En este punto el programa ya captura y valida sus argumentos: nada queda
 * hard-coded a partir de aquí.
 */
#include "config.hpp"

#include <cstdio>

int main(int argc, char** argv) {
    Config cfg;
    int codigo = parsearArgs(argc, argv, cfg);
    if (codigo != SALIDA_OK) return codigo;

    std::printf("Terraria Forge | ventana %dx%d | vsync %s | duracion %.1f s\n",
                cfg.w, cfg.h, cfg.vsync ? "on" : "off", cfg.duration);
    return SALIDA_OK;
}
