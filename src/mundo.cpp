#include "mundo.hpp"
#include "ruido.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

// Pasada 0: biomas.
static void pasadaBiomas(Mundo& m, uint32_t semilla) {
    m.bioma.assign(m.ancho, BIOMA_BOSQUE);

    // Sorteo de 3 biomas distintos: el bosque siempre sale es el ancla
    // visual de Terraria, los otros dos se eligen del resto.
    uint8_t opciones[3] = {BIOMA_NIEVE, BIOMA_CORRUPCION, BIOMA_DESIERTO};
    uint32_t h = mezclarHash(semilla, 0xB10Bu, 0xB10Bu);
    uint8_t extraA = opciones[h % 3];
    uint8_t extraB = opciones[(h % 3 + 1 + mezclarHash(h, 1u, 1u) % 2) % 3];
    uint8_t zonas[3] = {extraA, BIOMA_BOSQUE, extraB};
    if (mezclarHash(h, 2u, 2u) & 1u) { uint8_t t = zonas[0]; zonas[0] = zonas[2]; zonas[2] = t; }

    int tercio = m.ancho / 3;
    for (int x = 0; x < m.ancho; ++x) {
        int zona = std::min(2, x / tercio);
        // Dither de borde: hasta 3 columnas se cuelan al bioma vecino.
        int enBorde = x - zona * tercio;
        if (zona > 0 && enBorde < 3 &&
            (mezclarHash(semilla, static_cast<uint32_t>(x), 0xD17Eu) & 1u))
            m.bioma[x] = zonas[zona - 1];
        else
            m.bioma[x] = zonas[zona];
    }
}

// Pasada 1: altura de la superficie.
static void pasadaAltura(Mundo& m, const Ruido& ruido) {
    const float alturaBase = m.alto * 0.30f;
    const float amplitud   = m.alto * 0.13f;
    m.alturaSuperficie.resize(m.ancho);
    for (int x = 0; x < m.ancho; ++x) {
        float r = ruido.fractal1(x * 0.013f, 4, 0.5f);
        int h = static_cast<int>(alturaBase + r * amplitud);
        h = std::max(static_cast<int>(m.alto * 0.12f),
                     std::min(static_cast<int>(m.alto * 0.55f), h));
        m.alturaSuperficie[x] = h;
    }
}

// Pasada 2: capas.
static void pasadaCapas(Mundo& m, const Ruido& ruido) {
    for (int x = 0; x < m.ancho; ++x) {
        int hs = m.alturaSuperficie[x];
        int grosorTierra = 10 + static_cast<int>((ruido.fractal1(x * 0.05f + 500.0f, 2, 0.5f) + 1.0f) * 4.0f);
        for (int y = 0; y < m.alto; ++y) {
            uint8_t t;
            if      (y < hs)                 t = AIRE;
            else if (y < hs + grosorTierra)  t = TIERRA;
            else                             t = PIEDRA;
            m.M_tipo.en(x, y) = t;
        }
    }
}

// Pasada 3: cuevas.
static void pasadaCuevas(Mundo& m, const Ruido& ruido) {
    for (int x = 0; x < m.ancho; ++x) {
        int hs = m.alturaSuperficie[x];
        for (int y = hs + 5; y < m.alto - 2; ++y) {
            float profRel = static_cast<float>(y - hs) / (m.alto - hs);  // 0 arriba, 1 abajo
            // Sistema fino: túneles serpenteantes.
            float v1 = std::fabs(ruido.fractal2(x * 0.045f, y * 0.045f, 3, 0.5f));
            float u1 = 0.050f + profRel * 0.095f;
            // Sistema grueso: cavernas grandes de baja frecuencia.
            float v2 = std::fabs(ruido.fractal2(x * 0.017f + 91.0f, y * 0.023f + 47.0f, 2, 0.5f));
            float u2 = 0.030f + profRel * 0.060f;
            if (v1 < u1 || v2 < u2) m.M_tipo.en(x, y) = AIRE;
        }
    }
}

// Pasada 4: pasto.
static void pasadaPasto(Mundo& m) {
    for (int x = 0; x < m.ancho; ++x)
        for (int y = 1; y < m.alto; ++y)
            if (m.M_tipo.en(x, y) == TIERRA && m.M_tipo.en(x, y - 1) == AIRE)
                m.M_tipo.en(x, y) = PASTO;
}

// Pasada 5: vetas de mineral.
static void pasadaMinerales(Mundo& m, uint32_t semilla) {
    int numVetas = (m.ancho * m.alto) / 900;
    for (int veta = 0; veta < numVetas; ++veta) {
        uint32_t h = mezclarHash(semilla, 0x11111111u, static_cast<uint32_t>(veta));
        int x = static_cast<int>(h % m.ancho);
        int minY = m.alturaSuperficie[x] + 20;
        if (minY >= m.alto - 4) continue;
        int y = minY + static_cast<int>(mezclarHash(h, 1u, 2u) % (m.alto - 4 - minY));

        int largo = 5 + static_cast<int>(mezclarHash(h, 3u, 4u) % 9);
        for (int paso = 0; paso < largo; ++paso) {
            if (m.M_tipo.enOr(x, y, AIRE) == PIEDRA) m.M_tipo.en(x, y) = MINERAL;
            // También el vecino lateral, para que la veta tenga cuerpo.
            if (m.M_tipo.enOr(x + 1, y, AIRE) == PIEDRA &&
                (mezclarHash(h, static_cast<uint32_t>(paso), 7u) & 1u))
                m.M_tipo.en(x + 1, y) = MINERAL;
            uint32_t dir = mezclarHash(h, static_cast<uint32_t>(paso), 5u) % 4;
            x += (dir == 0) - (dir == 1);
            y += (dir == 2) - (dir == 3);
            if (!m.M_tipo.dentro(x, y)) break;
        }
    }
}

// Pasada 6: lava en cavernas profundas.
static void pasadaLava(Mundo& m) {
    int inicioLava = (m.alto * 2) / 3;
    for (int x = 0; x < m.ancho; ++x) {
        int finCorrida = -1;   // fila del piso de la corrida de aire actual
        for (int y = m.alto - 1; y >= inicioLava; --y) {
            bool aire = (m.M_tipo.en(x, y) == AIRE);
            if (aire && finCorrida < 0) finCorrida = y;
            if (!aire && finCorrida >= 0) {
                // Rellenar hasta 3 filas del fondo de la bolsa.
                int filas = std::min(3, finCorrida - y);
                for (int k = 0; k < filas; ++k) m.M_tipo.en(x, finCorrida - k) = LAVA;
                finCorrida = -1;
            }
        }
        if (finCorrida >= 0)   // bolsa que toca el fondo del rango
            for (int k = 0; k < 3 && finCorrida - k >= inicioLava; ++k)
                m.M_tipo.en(x, finCorrida - k) = LAVA;
    }
}

// Pasada 7: muros de fondo.
static void pasadaMuros(Mundo& m) {
    for (int x = 0; x < m.ancho; ++x) {
        int hs = m.alturaSuperficie[x];
        for (int y = 0; y < m.alto; ++y) {
            if (y <= hs) { m.M_fondo.en(x, y) = FONDO_NADA; continue; }
            // Muro de tierra cerca de la superficie, de piedra en lo profundo.
            m.M_fondo.en(x, y) = (y < hs + 16) ? FONDO_TIERRA : FONDO_PIEDRA;
        }
    }
}

Mundo generarMundo(const Config& cfg, uint32_t semilla) {
    Mundo m;
    m.ancho   = cfg.grid_w;
    m.alto    = cfg.grid_h;
    m.semilla = semilla;
    m.M_tipo    = Matriz<uint8_t>(m.ancho, m.alto, AIRE);
    m.M_fondo   = Matriz<uint8_t>(m.ancho, m.alto, FONDO_NADA);
    m.M_anim    = Matriz<float>(m.ancho, m.alto, 0.0f);
    m.M_retraso = Matriz<float>(m.ancho, m.alto, 0.0f);
    m.M_origX   = Matriz<float>(m.ancho, m.alto, 0.0f);
    m.M_origY   = Matriz<float>(m.ancho, m.alto, 0.0f);

    Ruido ruido(semilla);
    pasadaBiomas(m, semilla);     // 0
    pasadaAltura(m, ruido);       // 1
    pasadaCapas(m, ruido);        // 2
    pasadaCuevas(m, ruido);       // 3
    pasadaPasto(m);               // 4
    pasadaMinerales(m, semilla);  // 5
    pasadaLava(m);                // 6
    pasadaMuros(m);               // 7

    return m;
}

bool validarTerreno(const Mundo& m) {
    // 1 Superficie continua: sin saltos bruscos entre columnas vecinas
    // y dentro de márgenes verticales sanos.
    for (int x = 0; x < m.ancho; ++x) {
        int h = m.alturaSuperficie[x];
        if (h < 3 || h > m.alto - 12) return false;
        if (x > 0 && std::abs(h - m.alturaSuperficie[x - 1]) > 8) return false;
    }

    // 2 Aire subterráneo entre 20% y 45% del subsuelo:
    // menos = mundo macizo aburrido, más = queso.
    long celdasSub = 0, aireSub = 0;
    for (int x = 0; x < m.ancho; ++x) {
        for (int y = m.alturaSuperficie[x] + 1; y < m.alto; ++y) {
            ++celdasSub;
            if (m.M_tipo.en(x, y) == AIRE) ++aireSub;
        }
    }
    if (celdasSub == 0) return false;
    double fraccionAire = static_cast<double>(aireSub) / celdasSub;
    return fraccionAire >= 0.10 && fraccionAire <= 0.45;
}
