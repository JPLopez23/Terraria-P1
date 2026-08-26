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

void Camara::actualizar(Fase fase, float tFase, float dt, const Mundo& m) {
    float objetivoY;

    switch (fase) {
        case Fase::ENSAMBLANDO:
        case Fase::ENCENDIENDO:
            // Deriva lenta mientras el mundo se arma: nunca un frame estático.
            x += 0.45f * dt;
            objetivoY = alturaSuave(m, x + visW * 0.5f) - visH * 0.34f;
            break;

        case Fase::RECORRIENDO: {
            float p = std::min(1.0f, tFase / DUR_RECORRIENDO);
            float e = easeInOutCubic(p);
            x = xInicio + (xFin - xInicio) * e;
            // El gran tour vertical: primero SUBE al cielo: islas flotantes,
            float perfil;
            if (p < 0.28f)
                perfil = -std::sin(p / 0.28f * 3.14159265f) * (m.alto * 0.16f);
            else
                perfil =  std::sin((p - 0.28f) / 0.72f * 3.14159265f) * (m.alto * 0.42f);
            objetivoY = alturaSuave(m, x + visW * 0.5f) - visH * 0.34f + perfil;
            break;
        }

        default:  // DESARMANDO y GENERANDO: quieta, el espectáculo es la caída
            objetivoY = y;
            break;
    }

    // Suavizado exponencial del eje vertical: el horizontal ya es suave.
    float alfa = std::min(1.0f, dt * 1.8f);
    y += (objetivoY - y) * alfa;

    x = std::max(0.0f, std::min(static_cast<float>(m.ancho - visW), x));
    y = std::max(0.0f, std::min(static_cast<float>(m.alto - visH), y));
}
