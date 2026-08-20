#pragma once

#include "mundo.hpp"
#include "config.hpp"

/** Pixeles por tile. */
constexpr int TILE_PX = 8;

/** Duracion en segundos de un trayecto. */
constexpr float DUR_PASEO = 18.0f;

struct Camara {
    float x = 0.0f;   // Posicion X en tiles
    float y = 0.0f;
    int visW = 0;     // Ancho visible en tiles
    int visH = 0;     // Alto visible en tiles

    /**
     * Inicializa la camara en la posicion inicial del mundo.
     */
    void reiniciar(const Mundo& m, const Config& cfg);

    /**
     * Actualiza la posicion de la camara en cada frame.
     */
    void actualizar(float dt, const Mundo& m);

private:
    float xInicio = 0.0f, xFin = 0.0f;
    float tPaseo  = 0.0f;   // Tiempo acumulado en el ciclo de paseo

    /** Promedio del relieve cercano. */
    float alturaSuave(const Mundo& m, float tileX) const;
};
