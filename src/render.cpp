/**
 * render.cpp — framebuffer, presentación con SDL y utilidades de dibujo
 * (ver render.hpp).
 *
 * La composición del mundo (paleta, texturas y luz) se agrega sobre este
 * mismo archivo en los siguientes commits.
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
