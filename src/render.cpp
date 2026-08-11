/**
 * render.cpp — composición del frame con estética Terraria (ver render.hpp).
 *
 * Los colores están calcados de la paleta clásica de Terraria (tierra
 * 151,107,75 · pasto 28,216,94 · piedra gris...) y cada tile se pinta con
 * variación determinista por posición — motas, granulado, vetas — en
 * texeles de 2×2 px, el "pixel art chunky" característico del juego.
 */
#include "render.hpp"
#include "ruido.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

// ----------------------------------------------------------------
// Utilidades de color
// ----------------------------------------------------------------

/** ColorF — color de trabajo en punto flotante 0..255 por canal. */
struct ColorF { float r, g, b; };

static inline uint32_t empaquetarARGB(float r, float g, float b) {
    int ir = static_cast<int>(r); if (ir > 255) ir = 255; if (ir < 0) ir = 0;
    int ig = static_cast<int>(g); if (ig > 255) ig = 255; if (ig < 0) ig = 0;
    int ib = static_cast<int>(b); if (ib > 255) ib = 255; if (ib < 0) ib = 0;
    return 0xFF000000u | (static_cast<uint32_t>(ir) << 16)
                       | (static_cast<uint32_t>(ig) << 8)
                       |  static_cast<uint32_t>(ib);
}

// ----------------------------------------------------------------
// Textura procedural por tile — el corazón del "se ve como Terraria"
// ----------------------------------------------------------------

// Paletas por bioma para los tiles "vivos" (los demás tipos comparten color
// en todo el mundo, igual que en Terraria).
//                                        bosque           nieve            corrupción       desierto
static const float TIERRA_RGB[NUM_BIOMAS][3] = {{151,107, 75}, {174,184,204}, {109, 80,102}, {211,180,125}};
static const float PASTO_RGB [NUM_BIOMAS][3] = {{ 28,216, 94}, {235,245,255}, {150, 72,208}, {224,202,144}};
static const float PIEDRA_RGB[NUM_BIOMAS][3] = {{128,128,132}, {126,140,160}, { 98, 88,116}, {166,146,116}};
static const float HOJA_RGB  [NUM_BIOMAS][3] = {{ 44,142, 66}, {200,216,235}, {120, 62,168}, { 72,158, 74}};

// color base de un píxel dentro de un tile.
static ColorF texturaTile(uint8_t tipo, int wpx, int wpy, uint32_t semilla, float tiempo,
                          uint8_t bioma, bool infierno) {
    // Texel de 2×2 px: el grano del pixel art de Terraria.
    int txl = wpx >> 1, tyl = wpy >> 1;
    float v  = hashAFloat(mezclarHash(static_cast<uint32_t>(txl),
                                      static_cast<uint32_t>(tyl), semilla));
    float v2 = hashAFloat(mezclarHash(static_cast<uint32_t>(txl) * 3u + 7u,
                                      static_cast<uint32_t>(tyl), semilla ^ 0x5A5Au));
    int subY = wpy & 7;   // fila dentro del tile (0 arriba)

    // Infierno: la roca del fondo del mundo es piedra ardiente con brasas
    // incrustadas que brillan solas.
    if (infierno && (tipo == TIERRA || tipo == PIEDRA || tipo == PASTO)) {
        if (v2 > 0.90f) {
            float brasa = 0.75f + 0.25f * std::sin(tiempo * 3.0f + v * 6.28f);
            return {255.0f * brasa, 120.0f * brasa, 30.0f * brasa};
        }
        float f = 0.78f + v * 0.32f;
        return {112.0f * f, 47.0f * f, 40.0f * f};
    }

    switch (tipo) {
        case TIERRA: {
            // Tierra del bioma con motas más claras y grumos oscuros.
            float f = 0.82f + v * 0.30f;
            if (v2 > 0.87f) f *= 0.72f;               // grumo oscuro
            return {TIERRA_RGB[bioma][0] * f, TIERRA_RGB[bioma][1] * f, TIERRA_RGB[bioma][2] * f};
        }
        case PASTO: {
            // Cuerpo de tierra con capa superior del pasto del bioma
            // (nieve blanca, hierba corrupta púrpura, arena...).
            if (subY < 3 || (subY == 3 && v > 0.45f)) {
                float f = 0.80f + v * 0.35f;
                return {PASTO_RGB[bioma][0] * f, PASTO_RGB[bioma][1] * f, PASTO_RGB[bioma][2] * f};
            }
            float f = 0.82f + v * 0.30f;
            return {TIERRA_RGB[bioma][0] * f, TIERRA_RGB[bioma][1] * f, TIERRA_RGB[bioma][2] * f};
        }
        case PIEDRA: {
            // Gris del bioma con granulado y grietas ocasionales.
            float f = 0.80f + v * 0.32f;
            if (v2 > 0.93f) f *= 0.62f;               // grieta
            return {PIEDRA_RGB[bioma][0] * f, PIEDRA_RGB[bioma][1] * f, PIEDRA_RGB[bioma][2] * f};
        }
        case MINERAL: {
            // Piedra con gemas turquesa incrustadas que destellan.
            if (v > 0.62f) {
                float brillo = 0.85f + 0.35f * v2;
                return {66.0f * brillo, 224.0f * brillo, 198.0f * brillo};
            }
            float f = 0.78f + v * 0.28f;
            return {120.0f * f, 124.0f * f, 128.0f * f};
        }
        case LADRILLO: {
            // Aparejo de ladrillos 8×4 px con juntas de mortero oscuras.
            int fila = wpy >> 2;
            int cx = (wpx + ((fila & 1) ? 4 : 0)) & 7;
            bool junta = ((wpy & 3) == 0) || (cx == 0);
            if (junta) return {74.0f, 66.0f, 62.0f};
            float f = 0.86f + v * 0.20f;
            return {138.0f * f, 116.0f * f, 108.0f * f};
        }
        case MADERA: {
            // Veta vertical: franjas por columna de texel.
            float franja = ((txl % 3) == 0) ? 0.78f : 1.0f;
            float f = (0.85f + v * 0.20f) * franja;
            return {105.0f * f, 82.0f * f, 63.0f * f};
        }
        case HOJA: {
            // Follaje del bioma moteado, con huecos oscuros entre hojas
            // (en el desierto este tile es el cuerpo del cactus).
            float f = 0.75f + v * 0.45f;
            if (v2 > 0.88f) f *= 0.55f;
            return {HOJA_RGB[bioma][0] * f, HOJA_RGB[bioma][1] * f, HOJA_RGB[bioma][2] * f};
        }
        case LAVA: {
            // Lava incandescente con oleaje lento: brilla sola (no depende
            // del lightmap para verse — es emisora).
            float onda = 0.80f + 0.20f * std::sin(tiempo * 2.1f + wpx * 0.09f + v * 6.28f);
            float f = onda * (0.85f + v * 0.30f);
            return {253.0f * f, (110.0f + 60.0f * v2) * f, 24.0f * f};
        }
        default:
            return {0.0f, 0.0f, 0.0f};
    }
}

/** colorFondo — color del muro de fondo (más oscuro que el tile equivalente). */
static ColorF colorFondo(uint8_t fondo, int wpx, int wpy, uint32_t semilla,
                         uint8_t bioma, bool infierno) {
    int txl = wpx >> 1, tyl = wpy >> 1;
    float v = hashAFloat(mezclarHash(static_cast<uint32_t>(txl) ^ 0xF0F0u,
                                     static_cast<uint32_t>(tyl), semilla));
    float f = 0.85f + v * 0.25f;

    // En el infierno todos los muros arden en rojo oscuro.
    if (infierno && (fondo == FONDO_TIERRA || fondo == FONDO_PIEDRA))
        return {56.0f * f, 26.0f * f, 22.0f * f};

    switch (fondo) {
        case FONDO_TIERRA: {
            // El muro de tierra hereda un tinte del bioma de su columna.
            ColorF base = {70.0f, 50.0f, 38.0f};
            if (bioma == BIOMA_NIEVE)      base = {56.0f, 62.0f, 78.0f};
            if (bioma == BIOMA_CORRUPCION) base = {54.0f, 40.0f, 58.0f};
            if (bioma == BIOMA_DESIERTO)   base = {96.0f, 80.0f, 54.0f};
            return {base.r * f, base.g * f, base.b * f};
        }
        case FONDO_PIEDRA:   return {52.0f * f, 52.0f * f, 60.0f * f};
        case FONDO_LADRILLO: return {60.0f * f, 50.0f * f, 46.0f * f};
        case FONDO_MADERA: {
            // Tablones verticales: franja oscura entre tabla y tabla.
            float tabla = ((wpx >> 2) % 2 == 0) ? 1.0f : 0.82f;
            return {86.0f * f * tabla, 62.0f * f * tabla, 40.0f * f * tabla};
        }
        default:             return {0.0f, 0.0f, 0.0f};
    }
}

// cielo nocturno: degradado, campo de estrellas fijo por semilla (con titileo) y una luna con cráteres.
static ColorF colorCielo(int wpx, int wpy, int altoMundoPx, uint32_t semilla, float tiempo) {
    // Degradado vertical: azul muy oscuro arriba → azul horizonte abajo.
    float t = std::max(0.0f, std::min(1.0f, static_cast<float>(wpy) / (altoMundoPx * 0.45f)));
    ColorF c = {6.0f + 22.0f * t, 8.0f + 30.0f * t, 26.0f + 58.0f * t};

    // Luna: disco fijo en el mundo con sombra de cráteres.
    const float lunaX = altoMundoPx * 1.1f, lunaY = altoMundoPx * 0.07f, radioLuna = 26.0f;
    float dxl = wpx - lunaX, dyl = wpy - lunaY;
    float dl = std::sqrt(dxl * dxl + dyl * dyl);
    if (dl < radioLuna) {
        float borde = std::min(1.0f, (radioLuna - dl) / 3.0f);
        float crater = hashAFloat(mezclarHash(static_cast<uint32_t>(wpx) >> 2,
                                              static_cast<uint32_t>(wpy) >> 2, 0xC0FFEEu));
        float f = (crater > 0.8f) ? 0.78f : 1.0f;
        return {c.r + (225.0f * f - c.r) * borde,
                c.g + (228.0f * f - c.g) * borde,
                c.b + (205.0f * f - c.b) * borde};
    }

    // Estrellas: una por celda de 8×8 px con probabilidad baja, titilando.
    uint32_t celda = mezclarHash(static_cast<uint32_t>(wpx) >> 3,
                                 static_cast<uint32_t>(wpy) >> 3, semilla ^ 0x57A25u);
    if ((celda & 0x3F) == 0) {                       // ~1.5% de las celdas
        int sx = static_cast<int>((celda >> 8) & 7), sy = static_cast<int>((celda >> 11) & 7);
        if ((wpx & 7) == sx && (wpy & 7) == sy) {
            float titileo = 0.55f + 0.45f * std::sin(tiempo * 2.0f + (celda & 0xFF));
            float brillo = 120.0f + 135.0f * hashAFloat(celda) * titileo;
            return {brillo, brillo, brillo * 0.92f};
        }
    }
    return c;
}

// ----------------------------------------------------------------
// Composición principal
// ----------------------------------------------------------------

void componerFrame(const Mundo& m, const Camara& cam, double tiempo,
                   const Config& cfg, uint32_t* fb) {
    const int w = cfg.w, h = cfg.h;
    const float tiempoF = static_cast<float>(tiempo);
    const int altoMundoPx = m.alto * TILE_PX;

    // La cámara se redondea a píxel entero para que el pixel art no vibre.
    const int camPxX = static_cast<int>(std::floor(cam.x * TILE_PX));
    const int camPxY = static_cast<int>(std::floor(cam.y * TILE_PX));

    // ---- Pasada 1: fondo + tiles sólidos, píxel por píxel ----
    for (int py = 0; py < h; ++py) {
        int wpy = camPxY + py;
        int ty  = wpy >> 3;              // wpy / TILE_PX
        for (int px = 0; px < w; ++px) {
            int wpx = camPxX + px;
            int tx  = wpx >> 3;

            uint8_t tipo  = m.M_tipo.enOr(tx, ty, AIRE);
            uint8_t fondo = m.M_fondo.enOr(tx, ty, FONDO_NADA);
            uint8_t bioma = m.bioma[std::max(0, std::min(m.ancho - 1, tx))];
            bool infierno = ty > static_cast<int>(m.alto * FRACCION_INFIERNO);

            ColorF c;
            if (tipo != AIRE) {
                c = texturaTile(tipo, wpx, wpy, m.semilla, tiempoF, bioma, infierno);

                // Contorno oscuro en cada cara expuesta al aire — el borde
                // negro característico de los bloques de Terraria.
                int subX = wpx & 7, subY = wpy & 7;
                bool contorno =
                    (subX == 0 && m.M_tipo.enOr(tx - 1, ty, AIRE) == AIRE) ||
                    (subX == 7 && m.M_tipo.enOr(tx + 1, ty, AIRE) == AIRE) ||
                    (subY == 0 && m.M_tipo.enOr(tx, ty - 1, AIRE) == AIRE) ||
                    (subY == 7 && m.M_tipo.enOr(tx, ty + 1, AIRE) == AIRE);
                if (contorno && tipo != LAVA) { c.r *= 0.38f; c.g *= 0.38f; c.b *= 0.38f; }
            } else if (fondo != FONDO_NADA) {
                c = colorFondo(fondo, wpx, wpy, m.semilla, bioma, infierno);
            } else {
                c = colorCielo(wpx, wpy, altoMundoPx, m.semilla, tiempoF);
                // Briznas de pasto que desbordan sobre el aire, como en el juego.
                int subY = wpy & 7;
                if (subY >= 6 && m.M_tipo.enOr(tx, ty + 1, AIRE) == PASTO) {
                    uint32_t hb = mezclarHash(static_cast<uint32_t>(wpx), 0x9E37u, m.semilla);
                    if ((hb & 3u) != 0u || subY == 7) {
                        float f = 0.75f + hashAFloat(hb) * 0.4f;
                        c = {PASTO_RGB[bioma][0] * f, PASTO_RGB[bioma][1] * f,
                             PASTO_RGB[bioma][2] * f};
                    }
                }
            }

            fb[py * w + px] = empaquetarARGB(c.r, c.g, c.b);
        }
    }
}

// ----------------------------------------------------------------
// Fuente bitmap 5×7 para el overlay de FPS (sin dependencia de SDL_ttf)
// ----------------------------------------------------------------

// Cada glifo son 7 filas de 5 bits (bit 4 = columna izquierda).
static const uint8_t GLIFOS_5X7[][7] = {
    /* 0 */ {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
    /* 1 */ {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    /* 2 */ {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},
    /* 3 */ {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},
    /* 4 */ {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
    /* 5 */ {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
    /* 6 */ {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
    /* 7 */ {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    /* 8 */ {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
    /* 9 */ {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
    /* A */ {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
    /* B */ {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
    /* C */ {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
    /* D */ {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C},
    /* E */ {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
    /* F */ {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
    /* G */ {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},
    /* H */ {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
    /* I */ {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
    /* J */ {0x07,0x02,0x02,0x02,0x02,0x12,0x0C},
    /* K */ {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
    /* L */ {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    /* M */ {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},
    /* N */ {0x11,0x11,0x19,0x15,0x13,0x11,0x11},
    /* O */ {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
    /* P */ {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
    /* Q */ {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
    /* R */ {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    /* S */ {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},
    /* T */ {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
    /* U */ {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
    /* V */ {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
    /* W */ {0x11,0x11,0x11,0x15,0x15,0x15,0x0A},
    /* X */ {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
    /* Y */ {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
    /* Z */ {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
    /* : */ {0x00,0x04,0x00,0x00,0x00,0x04,0x00},
    /* . */ {0x00,0x00,0x00,0x00,0x00,0x00,0x04},
    /* | */ {0x04,0x04,0x04,0x04,0x04,0x04,0x04},
    /* - */ {0x00,0x00,0x00,0x0E,0x00,0x00,0x00},
    /* / */ {0x01,0x02,0x02,0x04,0x08,0x08,0x10},
    /* esp */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
};

/** indiceGlifo — posición del carácter en GLIFOS_5X7 (espacio si no existe). */
static int indiceGlifo(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return 10 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 10 + (c - 'a');
    switch (c) {
        case ':': return 36;
        case '.': return 37;
        case '|': return 38;
        case '-': return 39;
        case '/': return 40;
        default:  return 41;
    }
}

void dibujarTexto(uint32_t* fb, int w, int h, int x, int y, int escala,
                  const std::string& texto, uint32_t color) {
    int cx = x;
    for (char c : texto) {
        const uint8_t* glifo = GLIFOS_5X7[indiceGlifo(c)];
        for (int fila = 0; fila < 7; ++fila) {
            for (int col = 0; col < 5; ++col) {
                if (!(glifo[fila] & (1 << (4 - col)))) continue;
                for (int sy = 0; sy < escala; ++sy) {
                    for (int sx = 0; sx < escala; ++sx) {
                        int px = cx + col * escala + sx;
                        int py = y + fila * escala + sy;
                        // Sombra 1px abajo-derecha para legibilidad.
                        if (px + escala < w && py + escala < h)
                            fb[(py + escala) * w + (px + escala)] = 0xFF000000u;
                        if (px >= 0 && px < w && py >= 0 && py < h)
                            fb[py * w + px] = color;
                    }
                }
            }
        }
        cx += 6 * escala;
    }
}

bool volcarBMP(const std::string& ruta, const uint32_t* fb, int w, int h) {
    // BMP de 24 bits sin compresión, filas alineadas a 4 bytes y de abajo
    // hacia arriba — el formato más simple que abre cualquier visor.
    const int relleno = (4 - (w * 3) % 4) % 4;
    const uint32_t tamImagen = static_cast<uint32_t>((w * 3 + relleno) * h);
    const uint32_t tamArchivo = 54 + tamImagen;

    std::FILE* f = std::fopen(ruta.c_str(), "wb");
    if (!f) return false;

    uint8_t cabecera[54] = {0};
    cabecera[0] = 'B'; cabecera[1] = 'M';
    std::memcpy(cabecera + 2,  &tamArchivo, 4);
    uint32_t offsetDatos = 54;           std::memcpy(cabecera + 10, &offsetDatos, 4);
    uint32_t tamInfo = 40;               std::memcpy(cabecera + 14, &tamInfo, 4);
    std::memcpy(cabecera + 18, &w, 4);
    std::memcpy(cabecera + 22, &h, 4);
    uint16_t planos = 1;                 std::memcpy(cabecera + 26, &planos, 2);
    uint16_t bpp = 24;                   std::memcpy(cabecera + 28, &bpp, 2);
    std::memcpy(cabecera + 34, &tamImagen, 4);
    if (std::fwrite(cabecera, 1, 54, f) != 54) { std::fclose(f); return false; }

    std::vector<uint8_t> fila(static_cast<size_t>(w) * 3 + relleno, 0);
    for (int y = h - 1; y >= 0; --y) {
        for (int x = 0; x < w; ++x) {
            uint32_t p = fb[y * w + x];
            fila[x * 3 + 0] = static_cast<uint8_t>(p & 0xFF);          // B
            fila[x * 3 + 1] = static_cast<uint8_t>((p >> 8) & 0xFF);   // G
            fila[x * 3 + 2] = static_cast<uint8_t>((p >> 16) & 0xFF);  // R
        }
        if (std::fwrite(fila.data(), 1, fila.size(), f) != fila.size()) {
            std::fclose(f);
            return false;
        }
    }
    std::fclose(f);
    return true;
}

// ----------------------------------------------------------------
// Pantalla — RAII sobre SDL
// ----------------------------------------------------------------

Pantalla::Pantalla(const Config& cfg) : ancho(cfg.w), alto(cfg.h) {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        throw std::runtime_error(std::string("SDL_Init: ") + SDL_GetError());

    ventana = SDL_CreateWindow("Terraria Forge",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               ancho, alto, SDL_WINDOW_SHOWN);
    if (!ventana) {
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_CreateWindow: ") + SDL_GetError());
    }

    // VSync solo si se pidió: al medir rendimiento DEBE estar apagado, o
    // todas las versiones marcarían 60 FPS y no se demostraría nada.
    uint32_t flags = SDL_RENDERER_ACCELERATED | (cfg.vsync ? SDL_RENDERER_PRESENTVSYNC : 0);
    renderer = SDL_CreateRenderer(ventana, -1, flags);
    if (!renderer) {
        SDL_DestroyWindow(ventana);
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_CreateRenderer: ") + SDL_GetError());
    }

    textura = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, ancho, alto);
    if (!textura) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(ventana);
        SDL_Quit();
        throw std::runtime_error(std::string("SDL_CreateTexture: ") + SDL_GetError());
    }
}

Pantalla::~Pantalla() {
    if (textura)  SDL_DestroyTexture(textura);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (ventana)  SDL_DestroyWindow(ventana);
    SDL_Quit();
}

bool Pantalla::procesarEventos() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) return true;
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) return true;
    }
    return false;
}

void Pantalla::presentar(const uint32_t* fb) {
    // Solo el hilo maestro pasa por aquí (SDL no es thread-safe para render).
    if (SDL_UpdateTexture(textura, nullptr, fb, ancho * static_cast<int>(sizeof(uint32_t))) != 0)
        std::fprintf(stderr, "SDL_UpdateTexture: %s\n", SDL_GetError());
    if (SDL_RenderCopy(renderer, textura, nullptr, nullptr) != 0)
        std::fprintf(stderr, "SDL_RenderCopy: %s\n", SDL_GetError());
    SDL_RenderPresent(renderer);
}

void Pantalla::ponerTitulo(const std::string& titulo) {
    SDL_SetWindowTitle(ventana, titulo.c_str());
}
