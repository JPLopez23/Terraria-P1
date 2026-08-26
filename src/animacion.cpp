#include "animacion.hpp"
#include "ruido.hpp"

#include <algorithm>
#include <cmath>

const char* nombreFase(Fase fase) {
    switch (fase) {
        case Fase::GENERANDO:   return "GENERANDO";
        case Fase::ENSAMBLANDO: return "ENSAMBLANDO";
        case Fase::ENCENDIENDO: return "ENCENDIENDO";
        case Fase::RECORRIENDO: return "RECORRIENDO";
        default:                return "DESARMANDO";
    }
}

void inicializarAnimacion(Mundo& m) {
    for (int y = 0; y < m.alto; ++y) {
        for (int x = 0; x < m.ancho; ++x) {
            int i = idx(x, y, m.ancho);
            uint32_t h = mezclarHash(m.semilla, static_cast<uint32_t>(x),
                                     static_cast<uint32_t>(y) ^ 0x77777777u);

            // Ola diagonal: los tiles de la izquierda-arriba llegan primero.
            m.M_retraso.datos[i] = 0.65f * (static_cast<float>(x) / m.ancho)
                                 + 0.25f * (static_cast<float>(y) / m.alto)
                                 + 0.10f * hashAFloat(h);

            // Punto de entrada: cae desde arriba con deriva lateral aleatoria :
            // el "vuelo" de cada tile es distinto pero determinista por semilla.
            float dx = (hashAFloat(mezclarHash(h, 1u, 1u)) - 0.5f) * 70.0f;
            float dy = -(25.0f + hashAFloat(mezclarHash(h, 2u, 2u)) * 65.0f);
            m.M_origX.datos[i] = x + dx;
            m.M_origY.datos[i] = y + dy;
            m.M_anim.datos[i]  = 0.0f;
        }
    }
}

void actualizarEnsamblaje(Mundo& m, float tFase) {
    // Ventana de vuelo: cada tile tarda VENTANA del tiempo normalizado en
    // llegar; el resto del tiempo espera su turno según M_retraso.
    const float VENTANA = 0.16f;
    float t01 = std::min(1.0f, tFase / DUR_ENSAMBLANDO);
    int total = m.ancho * m.alto;
    for (int i = 0; i < total; ++i) {
        float u = (t01 * (1.0f + VENTANA) - m.M_retraso.datos[i]) / VENTANA;
        u = std::max(0.0f, std::min(1.0f, u));
        m.M_anim.datos[i] = easeOutCubic(u);
    }
}

void fijarAnimCompleta(Mundo& m) {
    m.M_anim.llenar(1.0f);
}

void actualizarDesarme(Mundo& m, float tFase) {
    // El tile deja de bloquear la luz conforme cae: M_anim 1→0 en ~0.45 s
    int total = m.ancho * m.alto;
    for (int i = 0; i < total; ++i) {
        float t = tiempoCaida(m.M_retraso.datos[i], tFase);
        m.M_anim.datos[i] = std::max(0.0f, 1.0f - t * 2.2f);
    }
}
