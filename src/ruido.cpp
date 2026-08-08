/**
 * ruido.cpp — implementación del ruido Perlin (ver ruido.hpp).
 */
#include "ruido.hpp"

#include <cmath>

Ruido::Ruido(uint32_t semilla) {
    // Tabla identidad barajada con Fisher-Yates alimentado por el hash:
    // la misma semilla produce siempre el mismo mundo.
    uint8_t base[256];
    for (int i = 0; i < 256; ++i) base[i] = static_cast<uint8_t>(i);
    for (int i = 255; i > 0; --i) {
        uint32_t j = mezclarHash(static_cast<uint32_t>(i), semilla, 0xA5A5A5A5u) % (i + 1);
        uint8_t tmp = base[i]; base[i] = base[j]; base[j] = tmp;
    }
    for (int i = 0; i < 512; ++i) perm[i] = base[i & 255];
}

float Ruido::gradiente2(int h, float dx, float dy) {
    // 8 direcciones de gradiente — suficiente para terreno.
    switch (h & 7) {
        case 0: return  dx + dy;
        case 1: return  dx - dy;
        case 2: return -dx + dy;
        case 3: return -dx - dy;
        case 4: return  dx;
        case 5: return -dx;
        case 6: return  dy;
        default: return -dy;
    }
}

float Ruido::perlin1(float x) const {
    int   x0 = static_cast<int>(std::floor(x));
    float fx = x - x0;
    int   a  = perm[x0 & 255];
    int   b  = perm[(x0 + 1) & 255];
    float u  = suavizar(fx);
    return interpolar(gradiente1(a, fx), gradiente1(b, fx - 1.0f), u) * 2.0f;
}

float Ruido::perlin2(float x, float y) const {
    int   x0 = static_cast<int>(std::floor(x));
    int   y0 = static_cast<int>(std::floor(y));
    float fx = x - x0;
    float fy = y - y0;

    int aa = perm[perm[x0 & 255] + (y0 & 255)];
    int ab = perm[perm[x0 & 255] + ((y0 + 1) & 255)];
    int ba = perm[perm[(x0 + 1) & 255] + (y0 & 255)];
    int bb = perm[perm[(x0 + 1) & 255] + ((y0 + 1) & 255)];

    float u = suavizar(fx);
    float v = suavizar(fy);

    float g00 = gradiente2(aa, fx,        fy);
    float g10 = gradiente2(ba, fx - 1.0f, fy);
    float g01 = gradiente2(ab, fx,        fy - 1.0f);
    float g11 = gradiente2(bb, fx - 1.0f, fy - 1.0f);

    return interpolar(interpolar(g00, g10, u), interpolar(g01, g11, u), v) * 1.41421356f;
}

float Ruido::fractal1(float x, int octavas, float persistencia) const {
    float suma = 0.0f, amplitud = 1.0f, frecuencia = 1.0f, pesoTotal = 0.0f;
    for (int o = 0; o < octavas; ++o) {
        suma      += perlin1(x * frecuencia + o * 37.13f) * amplitud;
        pesoTotal += amplitud;
        amplitud  *= persistencia;
        frecuencia *= 2.0f;
    }
    return suma / pesoTotal;
}

float Ruido::fractal2(float x, float y, int octavas, float persistencia) const {
    float suma = 0.0f, amplitud = 1.0f, frecuencia = 1.0f, pesoTotal = 0.0f;
    for (int o = 0; o < octavas; ++o) {
        suma      += perlin2(x * frecuencia + o * 19.7f, y * frecuencia + o * 11.3f) * amplitud;
        pesoTotal += amplitud;
        amplitud  *= persistencia;
        frecuencia *= 2.0f;
    }
    return suma / pesoTotal;
}
