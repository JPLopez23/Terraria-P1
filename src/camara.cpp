/**
 * camara.cpp — implementación del recorrido de cámara (ver camara.hpp).
 */
#include "camara.hpp"

#include <algorithm>
#include <cmath>

void Camara::reiniciar(const Mundo& m, const Config& cfg) {
    visW = cfg.w / TILE_PX;
    visH = cfg.h / TILE_PX;
    xInicio = 6.0f;
    xFin    = static_cast<float>(m.ancho - visW - 6);
    if (xFin < xInicio) xFin = xInicio;   // mundo angosto: no hay viaje
    x = xInicio;
    y = alturaSuave(m, x + visW * 0.5f) - visH * 0.34f;
    y = std::max(0.0f, std::min(static_cast<float>(m.alto - visH), y));
}

float Camara::alturaSuave(const Mundo& m, float tileX) const {
    // Promedio de la superficie en una ventana de 24 columnas: la cámara
    // sigue el relieve grande, no cada diente del terreno.
    int centro = static_cast<int>(tileX);
    float suma = 0.0f;
    int cuenta = 0;
    for (int dx = -12; dx <= 12; dx += 2) {
        int cx = std::max(0, std::min(m.ancho - 1, centro + dx));
        suma += m.alturaSuperficie[cx];
        ++cuenta;
    }
    return suma / cuenta;
}
