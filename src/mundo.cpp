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

// Pasada 7b: casas de madera en.
static void pasadaCasas(Mundo& m, uint32_t semilla) {
    int numCasas = std::max(2, m.ancho / 130);
    int casasColocadas = 0;
    for (int intento = 0; intento < numCasas * 8 && casasColocadas < numCasas; ++intento) {
        uint32_t h = mezclarHash(semilla, 0x66666666u, static_cast<uint32_t>(intento));
        int anchoCasa = 11 + static_cast<int>(mezclarHash(h, 1u, 1u) % 5);
        int altoCasa  = 6  + static_cast<int>(mezclarHash(h, 2u, 2u) % 2);
        int x0 = 12 + static_cast<int>(h % std::max(1u, static_cast<uint32_t>(m.ancho - anchoCasa - 24)));

        // Sitio plano: la superficie no varía más de 2 tiles bajo la casa.
        int ysMin = m.alto, ysMax = 0;
        for (int x = x0; x < x0 + anchoCasa; ++x) {
            ysMin = std::min(ysMin, m.alturaSuperficie[x]);
            ysMax = std::max(ysMax, m.alturaSuperficie[x]);
        }
        if (ysMax - ysMin > 2) continue;

        int yPiso = ysMin - 1;               // fila del piso, sobre el punto más alto
        if (yPiso - altoCasa < 2) continue;

        // Caja de la casa: piso, paredes y techo de MADERA, interior AIRE
        // con muro de fondo de madera; puerta de 3 tiles en el lado derecho.
        for (int y = yPiso - altoCasa; y <= yPiso; ++y) {
            for (int x = x0; x < x0 + anchoCasa; ++x) {
                bool borde  = (x == x0 || x == x0 + anchoCasa - 1 ||
                               y == yPiso || y == yPiso - altoCasa);
                bool puerta = (x == x0 + anchoCasa - 1) &&
                              (y >= yPiso - 3 && y <= yPiso - 1);
                m.M_tipo.en(x, y)  = (borde && !puerta) ? MADERA : AIRE;
                m.M_fondo.en(x, y) = FONDO_MADERA;
            }
        }
        // Aleros del techo: una teja extra a cada lado.
        int yTecho = yPiso - altoCasa;
        if (m.M_tipo.dentro(x0 - 1, yTecho))            m.M_tipo.en(x0 - 1, yTecho) = MADERA;
        if (m.M_tipo.dentro(x0 + anchoCasa, yTecho))    m.M_tipo.en(x0 + anchoCasa, yTecho) = MADERA;

        // Cimiento: rellenar el hueco entre el piso y el terreno real.
        for (int x = x0; x < x0 + anchoCasa; ++x)
            for (int y = yPiso + 1; y < m.alto && m.M_tipo.en(x, y) == AIRE; ++y)
                m.M_tipo.en(x, y) = TIERRA;

        // Antorchas interiores sugeridas: las coloca la pasada 10.
        m.antorchasSugeridas.emplace_back(x0 + 2, yPiso - 1);
        if (anchoCasa >= 13)
            m.antorchasSugeridas.emplace_back(x0 + anchoCasa - 4, yPiso - 1);
        ++casasColocadas;
    }
}

// Pasada 8: árboles sobre PASTO, con.
static void pasadaArboles(Mundo& m, uint32_t semilla) {
    int ultimoArbol = -100;
    for (int x = 2; x < m.ancho - 2; ++x) {
        int ys = m.alturaSuperficie[x];
        if (m.M_tipo.enOr(x, ys, AIRE) != PASTO) continue;
        if (m.M_tipo.enOr(x, ys - 1, PIEDRA) != AIRE) continue;   // hay una casa encima
        if (x - ultimoArbol < 9) continue;
        uint32_t h = mezclarHash(semilla, 0x22222222u, static_cast<uint32_t>(x));

        // En el desierto no crecen árboles: crecen cactus cuerpo de HOJA,
        // que el render pinta verde cactus en ese bioma.
        if (m.bioma[x] == BIOMA_DESIERTO) {
            if (hashAFloat(h) > 0.30f) continue;
            int alturaCactus = 3 + static_cast<int>(mezclarHash(h, 9u, 9u) % 4);
            for (int k = 1; k <= alturaCactus; ++k)
                if (m.M_tipo.enOr(x, ys - k, PIEDRA) == AIRE) m.M_tipo.en(x, ys - k) = HOJA;
            // Un brazo lateral a media altura, como los cactus clásicos.
            int ladoBrazo = (mezclarHash(h, 8u, 8u) & 1u) ? 1 : -1;
            int yBrazo = ys - alturaCactus / 2 - 1;
            if (m.M_tipo.enOr(x + ladoBrazo, yBrazo, PIEDRA) == AIRE)
                m.M_tipo.en(x + ladoBrazo, yBrazo) = HOJA;
            ultimoArbol = x;
            continue;
        }
        if (hashAFloat(h) > 0.42f) continue;

        int alturaTronco = 5 + static_cast<int>(mezclarHash(h, 1u, 1u) % 4);
        // Tronco: solo donde hay aire, para no perforar montañas.
        for (int k = 1; k <= alturaTronco; ++k)
            if (m.M_tipo.enOr(x, ys - k, PIEDRA) == AIRE) m.M_tipo.en(x, ys - k) = MADERA;
        // Copa: blob irregular alrededor de la punta del tronco.
        int cy = ys - alturaTronco;
        int radioCopa = 3 + static_cast<int>(mezclarHash(h, 2u, 2u) % 2);
        for (int dy = -radioCopa - 1; dy <= radioCopa; ++dy) {
            for (int dx = -radioCopa; dx <= radioCopa; ++dx) {
                float d = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                float irregular = hashAFloat(mezclarHash(h,
                    static_cast<uint32_t>(dx + 64), static_cast<uint32_t>(dy + 64))) * 1.2f;
                if (d < radioCopa + 0.4f - irregular * 0.9f &&
                    m.M_tipo.enOr(x + dx, cy + dy, PIEDRA) == AIRE)
                    m.M_tipo.en(x + dx, cy + dy) = HOJA;
            }
        }
        ultimoArbol = x;
    }
}

// Pasada 9: estructura de ladrillo.
static void pasadaEstructuras(Mundo& m, uint32_t semilla) {
    int numEstructuras = std::max(1, m.ancho / 200);
    for (int e = 0; e < numEstructuras; ++e) {
        uint32_t h = mezclarHash(semilla, 0x33333333u, static_cast<uint32_t>(e));
        int ancho = 22 + static_cast<int>(mezclarHash(h, 1u, 1u) % 12);
        int alto  = 10 + static_cast<int>(mezclarHash(h, 2u, 2u) % 6);
        int x0 = 20 + static_cast<int>(h % std::max(1u, static_cast<uint32_t>(m.ancho - ancho - 40)));
        int hs = m.alturaSuperficie[std::min(m.ancho - 1, x0 + ancho / 2)];
        int y0 = hs + 24 + static_cast<int>(mezclarHash(h, 3u, 3u) % std::max(1, m.alto - hs - alto - 40));
        if (y0 + alto >= m.alto - 2) continue;

        for (int y = y0; y < y0 + alto; ++y) {
            for (int x = x0; x < x0 + ancho; ++x) {
                bool borde = (x == x0 || x == x0 + ancho - 1 || y == y0 || y == y0 + alto - 1);
                // Puertas: huecos de 3 tiles en el centro de los muros laterales.
                bool puerta = (x == x0 || x == x0 + ancho - 1) &&
                              (y > y0 + alto - 5 && y < y0 + alto - 1);
                m.M_tipo.en(x, y)  = (borde && !puerta) ? LADRILLO : AIRE;
                m.M_fondo.en(x, y) = FONDO_LADRILLO;
            }
        }
    }
}

// Pasada 8b: islas flotantes.
static void pasadaIslas(Mundo& m, uint32_t semilla) {
    int numIslas = std::max(2, m.ancho / 160);
    for (int isla = 0; isla < numIslas; ++isla) {
        uint32_t h = mezclarHash(semilla, 0x15AA15u, static_cast<uint32_t>(isla));
        int radioX = 7 + static_cast<int>(mezclarHash(h, 1u, 1u) % 6);
        int radioY = 3 + static_cast<int>(mezclarHash(h, 2u, 2u) % 2);
        int cx = 20 + static_cast<int>(h % std::max(1u, static_cast<uint32_t>(m.ancho - 40)));
        int cy = 8 + static_cast<int>(mezclarHash(h, 3u, 3u) % 10);

        // Muy pegada a una montaña alta no se ve "flotante": exigir aire.
        int hs = m.alturaSuperficie[std::max(0, std::min(m.ancho - 1, cx))];
        if (cy + radioY + 4 >= hs) continue;

        // Blob elíptico de tierra con base irregular: más gruesa al centro.
        for (int dx = -radioX; dx <= radioX; ++dx) {
            for (int dy = -radioY; dy <= radioY + 2; ++dy) {
                float nx = static_cast<float>(dx) / radioX;
                float ny = static_cast<float>(dy) / (dy < 0 ? radioY : radioY + 2.0f);
                float irregular = hashAFloat(mezclarHash(h,
                    static_cast<uint32_t>(dx + 64), static_cast<uint32_t>(dy + 64))) * 0.35f;
                if (nx * nx + ny * ny < 1.0f - irregular &&
                    m.M_tipo.dentro(cx + dx, cy + dy))
                    m.M_tipo.en(cx + dx, cy + dy) = TIERRA;
            }
        }

        // Ruina de ladrillo o arbolito encima, alternando por isla.
        if (mezclarHash(h, 4u, 4u) & 1u) {
            // Ruina: tres columnas de ladrillo y el dintel caído.
            for (int k = 0; k < 3; ++k) {
                int rx = cx - 2 + k * 2;
                for (int ry = cy - radioY - 3; ry < cy - radioY; ++ry)
                    if (m.M_tipo.dentro(rx, ry) && m.M_tipo.en(rx, ry) == AIRE)
                        m.M_tipo.en(rx, ry) = LADRILLO;
            }
        } else {
            int alturaTronco = 4 + static_cast<int>(mezclarHash(h, 5u, 5u) % 3);
            for (int k = 1; k <= alturaTronco; ++k)
                if (m.M_tipo.dentro(cx, cy - radioY - k) &&
                    m.M_tipo.en(cx, cy - radioY - k) == AIRE)
                    m.M_tipo.en(cx, cy - radioY - k) = MADERA;
            for (int dy = -2; dy <= 1; ++dy)
                for (int dx = -2; dx <= 2; ++dx)
                    if (std::abs(dx) + std::abs(dy) <= 3 &&
                        m.M_tipo.dentro(cx + dx, cy - radioY - alturaTronco + dy) &&
                        m.M_tipo.en(cx + dx, cy - radioY - alturaTronco + dy) == AIRE)
                        m.M_tipo.en(cx + dx, cy - radioY - alturaTronco + dy) = HOJA;
        }

        // Antorcha en la orilla de la isla: faro nocturno del cielo.
        m.antorchasSugeridas.emplace_back(cx + radioX - 2, cy - radioY - 1);
    }
}

// Pasada 9b: minas abandonadas.
static void pasadaMinas(Mundo& m, uint32_t semilla) {
    int numMinas = std::max(2, m.ancho / 150);
    for (int mina = 0; mina < numMinas; ++mina) {
        uint32_t h = mezclarHash(semilla, 0x99999999u, static_cast<uint32_t>(mina));
        int largo = 40 + static_cast<int>(mezclarHash(h, 1u, 1u) % 45);
        int x = 12 + static_cast<int>(h % std::max(1u, static_cast<uint32_t>(m.ancho - 24)));
        int dir = (mezclarHash(h, 2u, 2u) & 1u) ? 1 : -1;

        int hs = m.alturaSuperficie[std::max(0, std::min(m.ancho - 1, x))];
        int rango = std::max(1, m.alto - hs - 55);
        int y = hs + 30 + static_cast<int>(mezclarHash(h, 3u, 3u) % rango);
        if (y + 6 >= m.alto) continue;

        for (int paso = 0; paso < largo; ++paso, x += dir) {
            if (x < 2 || x >= m.ancho - 2) break;

            // La galería ondula suavemente para no verse artificial.
            if (paso % 9 == 8) {
                y += static_cast<int>(mezclarHash(h, static_cast<uint32_t>(paso), 4u) % 3) - 1;
                y = std::max(hs + 25, std::min(m.alto - 7, y));
            }

            // Túnel de 3 de alto con fondo de madera: tablones del socavón.
            for (int dy = 0; dy < 3; ++dy) {
                if (m.M_tipo.en(x, y + dy) != LADRILLO) m.M_tipo.en(x, y + dy) = AIRE;
                m.M_fondo.en(x, y + dy) = FONDO_MADERA;
            }

            // Marco de madera cada 8 pasos: dos postes y viga superior.
            if (paso % 8 == 4) {
                for (int dy = 0; dy < 3; ++dy) m.M_tipo.en(x, y + dy) = MADERA;
                if (m.M_tipo.dentro(x, y - 1))       m.M_tipo.en(x, y - 1) = MADERA;
                if (m.M_tipo.dentro(x - dir, y - 1)) m.M_tipo.en(x - dir, y - 1) = MADERA;
            }

            // Antorcha colgada a media galería, cada ~14 pasos.
            if (paso % 14 == 7) m.antorchasSugeridas.emplace_back(x, y + 1);
        }
    }
}

// La pasada 10 : colocación de las N fuentes de luz : vive en fuentes.cpp,
// porque necesita la struct Fuente y sus colores.

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
    pasadaCasas(m, semilla);      // 7b: antes de los árboles: bloquea sus columnas
    pasadaArboles(m, semilla);    // 8
    pasadaIslas(m, semilla);      // 8b
    pasadaEstructuras(m, semilla);// 9
    pasadaMinas(m, semilla);      // 9b
    pasadaPasto(m);               // repaso: las cuevas/estructuras expusieron tierra nueva

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
