#include "luz.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <omp.h>

// Tope de luz por canal: se permite pasar de 1.0 para que el render
// pueda hacer un bloom leve cerca de las fuentes.
static constexpr float LUZ_MAXIMA = 1.6f;

// Color de la luz ambiental nocturna: azulada, estilo luna de Terraria.
static constexpr float AMBIENTE_R = 0.34f;
static constexpr float AMBIENTE_G = 0.40f;
static constexpr float AMBIENTE_B = 0.58f;

// Profundidad: en tiles bajo la superficie a la que el ambiente se apaga.
static constexpr float PROFUNDIDAD_AMBIENTE = 22.0f;

float trazarRayo(const float* M_bloqueo,
                 float x0, float y0, float x1, float y1, int G_w, int G_h)
{
    float dx = x1 - x0, dy = y1 - y0;
    int pasos = static_cast<int>(std::max(std::fabs(dx), std::fabs(dy))) + 1;
    float ix = dx / pasos, iy = dy / pasos;

    float transmitancia = 1.0f;
    float x = x0, y = y0;

    for (int s = 0; s < pasos; ++s) {
        int tx = static_cast<int>(x), ty = static_cast<int>(y);
        if (tx >= 0 && tx < G_w && ty >= 0 && ty < G_h) {
            // M_bloqueo ya trae OPACIDAD[tipo] * M_anim precalculado para el
            transmitancia *= (1.0f - M_bloqueo[idx(tx, ty, G_w)]);
            if (transmitancia < 0.01f) return 0.0f;   // corte temprano
        }
        x += ix;  y += iy;
    }
    return transmitancia;
}

// Rejilla de bloqueo del frame: OPACIDAD[tipo] anim precalculado una vez y
static std::vector<float> M_bloqueoFrame;

 // precalcularBloqueo : llena la rejilla de bloqueo del frame actual.
static void precalcularBloqueo(const Mundo& m, bool paralelo) {
    size_t total = static_cast<size_t>(m.ancho) * m.alto;
    if (M_bloqueoFrame.size() != total) M_bloqueoFrame.resize(total);
    const uint8_t* tipo = m.M_tipo.datos.data();
    const float* anim = m.M_anim.datos.data();
    float* destino = M_bloqueoFrame.data();
    if (paralelo) {
        #pragma omp parallel for
        for (long i = 0; i < static_cast<long>(total); ++i)
            destino[i] = OPACIDAD[tipo[i]] * anim[i];
    } else {
        for (long i = 0; i < static_cast<long>(total); ++i)
            destino[i] = OPACIDAD[tipo[i]] * anim[i];
    }
}

 // llenarAmbiente : luz ambiental por profundidad, sumada ANTES de trazar rayos.
static void llenarAmbiente(const Mundo& m, Lightmap& L) {
    for (int ly = 0; ly < L.alto; ++ly) {
        float wy = L.origenY + (ly + 0.5f) * L.escala;
        for (int lx = 0; lx < L.ancho; ++lx) {
            float wx = L.origenX + (lx + 0.5f) * L.escala;
            int cx = std::max(0, std::min(m.ancho - 1, static_cast<int>(wx)));
            float profundidad = wy - m.alturaSuperficie[cx];
            float a = 1.0f - profundidad / PROFUNDIDAD_AMBIENTE;
            a = std::max(0.0f, std::min(1.0f, a));
            int i = ly * L.ancho + lx;
            L.M_luzR[i] = AMBIENTE_R * a;
            L.M_luzG[i] = AMBIENTE_G * a;
            L.M_luzB[i] = AMBIENTE_B * a;
        }
    }
}

 // filtrarFuentesVisibles : descarta fuentes apagadas o fuera del alcance
static std::vector<Fuente> filtrarFuentesVisibles(const std::vector<Fuente>& fuentes,
                                                  const Lightmap& L) {
    std::vector<Fuente> visibles;
    visibles.reserve(64);
    float x0 = L.origenX, y0 = L.origenY;
    float x1 = L.origenX + L.ancho * L.escala;
    float y1 = L.origenY + L.alto * L.escala;
    for (const Fuente& f : fuentes) {
        if (f.intensidad <= 0.001f) continue;
        if (f.x + f.radio < x0 || f.x - f.radio > x1 ||
            f.y + f.radio < y0 || f.y - f.radio > y1) continue;
        visibles.push_back(f);
    }
    return visibles;
}

 // aporteFuenteEnCelda : luz que una fuente entrega a una celda: wx,wy:
static inline float aporteFuenteEnCelda(const Mundo& m, const Fuente& f,
                                        float wx, float wy, int K) {
    // Distancia al cuadrado primero: el sqrt solo se paga si la fuente está
    // en rango: este chequeo corre celdas×fuentes veces por frame.
    float dx = f.x - wx, dy = f.y - wy;
    float d2 = dx * dx + dy * dy;
    if (d2 > f.radio * f.radio) return 0.0f;
    float d = std::sqrt(d2);

    float caida = 1.0f - d / f.radio;
    caida *= caida;                                  // caída cuadrática

    float potencial = f.intensidad * caida;
    if (potencial < 0.008f) return 0.0f;             // invisible: no trazar
    int kEfectivo = (potencial > 0.22f) ? K : 1;     // sombra suave solo de cerca

    const float* bloqueo = M_bloqueoFrame.data();
    float visible = 0.0f;
    for (int k = 0; k < kEfectivo; ++k) {
        float ang = 6.2831853f * k / kEfectivo;
        float sx = f.x + 0.45f * std::cos(ang);
        float sy = f.y + 0.45f * std::sin(ang);
        visible += trazarRayo(bloqueo, wx, wy, sx, sy, m.ancho, m.alto);
    }
    visible /= kEfectivo;

    return potencial * visible;
}

 // calcularFila : ilumina una fila completa del lightmap.
static void calcularFila(const Mundo& m, const std::vector<Fuente>& fuentes,
                         Lightmap& L, int K, int ly) {
    float wy = L.origenY + (ly + 0.5f) * L.escala;
    for (int lx = 0; lx < L.ancho; ++lx) {
        float wx = L.origenX + (lx + 0.5f) * L.escala;
        int i = ly * L.ancho + lx;
        float acR = L.M_luzR[i], acG = L.M_luzG[i], acB = L.M_luzB[i]; // arranca del ambiente

        for (const Fuente& f : fuentes) {
            float e = aporteFuenteEnCelda(m, f, wx, wy, K);
            acR += f.r * e;  acG += f.g * e;  acB += f.b * e;
        }

        L.M_luzR[i] = std::min(acR, LUZ_MAXIMA);  // se permite pasar de 1.0 para el bloom
        L.M_luzG[i] = std::min(acG, LUZ_MAXIMA);
        L.M_luzB[i] = std::min(acB, LUZ_MAXIMA);
    }
}

EstadisticasLuz calcularIluminacion(const Mundo& m, const std::vector<Fuente>& fuentes,
                                    Lightmap& L, const Config& cfg) {
    // Rejilla de bloqueo del frame + ambiente base: Oceldas, despreciable
    const bool paralela = (cfg.version != 0);
    precalcularBloqueo(m, paralela);
    llenarAmbiente(m, L);

    // Solo las fuentes que pueden tocar la pantalla entran al lazo caliente.
    std::vector<Fuente> visibles = filtrarFuentesVisibles(fuentes, L);

    if (paralela) {
        // LA versión paralela del proyecto: mismo algoritmo, filas del
        #pragma omp parallel for
        for (int ly = 0; ly < L.alto; ++ly)
            calcularFila(m, visibles, L, cfg.muestras, ly);
    } else {
        // Versión secuencial: exactamente el mismo trabajo, un solo hilo.
        for (int ly = 0; ly < L.alto; ++ly)
            calcularFila(m, visibles, L, cfg.muestras, ly);
    }

    // Estadísticas del frame: energía total y celdas iluminadas: sirven de
    EstadisticasLuz est;
    const long celdas = static_cast<long>(L.ancho) * L.alto;
    for (long i = 0; i < celdas; ++i) {
        double s = L.M_luzR[i] + L.M_luzG[i] + L.M_luzB[i];
        est.energia += s;
        if (s > 0.05) ++est.celdasIluminadas;
    }
    return est;
}
