/**
 * mundo.cpp — las 10 pasadas de generación del mundo (ver mundo.hpp).
 *
 * Cada pasada recorre la matriz completa y modifica una sola cosa.
 * El orden es estricto: cada pasada asume que las anteriores ya corrieron.
 */
#include "mundo.hpp"
#include "ruido.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

// ---------------------------------------------------------------
// Pasada 0 — biomas: el mundo se parte en 3 zonas horizontales cuyo
// bioma se sortea por semilla (bosque, nieve, corrupción, desierto).
// El borde entre zonas se ditherea un par de columnas para que la
// transición no sea una línea perfecta.
// ---------------------------------------------------------------
static void pasadaBiomas(Mundo& m, uint32_t semilla) {
    m.bioma.assign(m.ancho, BIOMA_BOSQUE);

    // Sorteo de 3 biomas distintos: el bosque siempre sale (es el ancla
    // visual de Terraria), los otros dos se eligen del resto.
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

// ---------------------------------------------------------------
// Pasada 1 — altura de la superficie: ruido 1D fractal por columna.
// Montañas grandes (octava base) + detalle fino (octavas altas).
// ---------------------------------------------------------------
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

// ---------------------------------------------------------------
// Pasada 2 — capas: aire, tierra y piedra según la profundidad
// relativa a la superficie, con grosor de tierra variable por ruido.
// ---------------------------------------------------------------
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

// ---------------------------------------------------------------
// Pasada 3 — cuevas: se usa el VALOR ABSOLUTO del ruido 2D y se
// conserva lo cercano a cero. Eso talla "crestas" continuas y
// serpenteantes en vez de manchas sueltas. El umbral crece con la
// profundidad: cuevas más grandes abajo.
// ---------------------------------------------------------------
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

// ---------------------------------------------------------------
// Pasada 4 — pasto: todo TIERRA con AIRE justo encima se vuelve PASTO.
// ---------------------------------------------------------------
static void pasadaPasto(Mundo& m) {
    for (int x = 0; x < m.ancho; ++x)
        for (int y = 1; y < m.alto; ++y)
            if (m.M_tipo.en(x, y) == TIERRA && m.M_tipo.en(x, y - 1) == AIRE)
                m.M_tipo.en(x, y) = PASTO;
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

    return m;
}
