#pragma once

#include "mundo.hpp"
#include "config.hpp"

#include <vector>

 // Lightmap : malla de luz en espacio de pantalla, más fina que los tiles.
struct Lightmap {
    int   ancho = 0, alto = 0;     // celdas
    float escala  = 0.25f;         // tiles por celda de lightmap
    float origenX = 0.0f;
    float origenY = 0.0f;
    std::vector<float> M_luzR, M_luzG, M_luzB;

    /** redimensionar : ajusta las tres matrices de luz al tamaño dado. */
    void redimensionar(int ancho_, int alto_) {
        ancho = ancho_;  alto = alto_;
        size_t n = static_cast<size_t>(ancho) * alto;
        M_luzR.assign(n, 0.0f);
        M_luzG.assign(n, 0.0f);
        M_luzB.assign(n, 0.0f);
    }
};

/** Estadísticas del frame : checksum físico para comparar versiones. */
struct EstadisticasLuz {
    double energia = 0.0;          // suma de R+G+B de todo el lightmap: checksum físico
    long   celdasIluminadas = 0;   // celdas con luz por encima del ambiente
};

 // trazarRayo : traza un rayo desde: x0,y0 hasta: x1,y1 en coordenadas de tile.
float trazarRayo(const float* M_bloqueo,
                 float x0, float y0, float x1, float y1, int G_w, int G_h);
