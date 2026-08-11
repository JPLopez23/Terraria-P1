/**
 * camara.hpp — recorrido lateral de la cámara y región visible.
 *
 * La cámara vive en coordenadas de tile (esquina superior izquierda de la
 * pantalla). El mundo es más ancho que la pantalla justamente para que
 * haya terreno que recorrer.
 */
#pragma once

#include "mundo.hpp"
#include "config.hpp"

/** Píxeles por tile en pantalla — el "zoom" del juego. */
constexpr int TILE_PX = 8;

struct Camara {
    float x = 0.0f;   // tile de la esquina superior izquierda
    float y = 0.0f;
    int visW = 0;     // tiles visibles a lo ancho
    int visH = 0;

    // coloca la cámara al inicio del recorrido de un mundo nuevo.
    void reiniciar(const Mundo& m, const Config& cfg);

private:
    float xInicio = 0.0f, xFin = 0.0f;

    /** alturaSuave — superficie promediada alrededor de una columna (para no vibrar). */
    float alturaSuave(const Mundo& m, float tileX) const;
};
