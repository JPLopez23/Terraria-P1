#pragma once

#include "mundo.hpp"
#include "animacion.hpp"
#include "config.hpp"

/** Píxeles por tile en pantalla : el "zoom" del juego. */
constexpr int TILE_PX = 8;

struct Camara {
    float x = 0.0f;   // tile de la esquina superior izquierda
    float y = 0.0f;
    int visW = 0;     // tiles visibles a lo ancho
    int visH = 0;

    // reiniciar : coloca la cámara al inicio del recorrido de un mundo nuevo.
    void reiniciar(const Mundo& m, const Config& cfg);

    // actualizar : mueve la cámara según la fase del ciclo.
    void actualizar(Fase fase, float tFase, float dt, const Mundo& m);

private:
    float xInicio = 0.0f, xFin = 0.0f;

    /** alturaSuave : superficie promediada alrededor de una columna: para no vibrar. */
    float alturaSuave(const Mundo& m, float tileX) const;
};
