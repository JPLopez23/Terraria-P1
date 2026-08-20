#include "camara.hpp"

#include <algorithm>
#include <cmath>

namespace {
/**
 * suavizarViaje: Suavizado para la aceleracion y frenado de la camara.
 */
float suavizarViaje(float t) {
    return (t < 0.5f) ? 4.0f * t * t * t
                      : 1.0f - 4.0f * (1.0f - t) * (1.0f - t) * (1.0f - t);
}
}  // namespace

void Camara::reiniciar(const Mundo& m, const Config& cfg) {
    visW = cfg.w / TILE_PX;
    visH = cfg.h / TILE_PX;
    xInicio = 6.0f;
    xFin    = static_cast<float>(m.ancho - visW - 6);
    if (xFin < xInicio) xFin = xInicio;   // Sin movimiento si el mundo es estrecho
    x = xInicio;
    y = alturaSuave(m, x + visW * 0.5f) - visH * 0.34f;
    y = std::max(0.0f, std::min(static_cast<float>(m.alto - visH), y));
}

float Camara::alturaSuave(const Mundo& m, float tileX) const {
    // Promedio de la superficie para evitar oscilaciones bruscas.
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

void Camara::actualizar(float dt, const Mundo& m) {
    // Movimiento horizontal continuo de ida y vuelta.
    tPaseo = std::fmod(tPaseo + dt, 2.0f * DUR_PASEO);
    float p = (tPaseo < DUR_PASEO) ? tPaseo / DUR_PASEO
                                   : (2.0f * DUR_PASEO - tPaseo) / DUR_PASEO;
    x = xInicio + (xFin - xInicio) * suavizarViaje(p);

    // Ajusta la altura de la camara con suavizado exponencial.
    float objetivoY = alturaSuave(m, x + visW * 0.5f) - visH * 0.34f;
    float alfa = std::min(1.0f, dt * 1.8f);
    y += (objetivoY - y) * alfa;

    x = std::max(0.0f, std::min(static_cast<float>(m.ancho - visW), x));
    y = std::max(0.0f, std::min(static_cast<float>(m.alto - visH), y));
}
