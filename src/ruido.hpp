/**
 * ruido.hpp — ruido Perlin 1D/2D con suma fractal por octavas,
 * y utilidades de hash determinista para variación visual por tile.
 *
 * Referencia: Perlin, K. (1985). "An Image Synthesizer", SIGGRAPH '85.
 */
#pragma once

#include <cstdint>

// hash entero rápido y determinista (avalancha de bits).
inline uint32_t mezclarHash(uint32_t a, uint32_t b, uint32_t c) {
    uint32_t h = a * 0x9E3779B9u ^ b * 0x85EBCA6Bu ^ c * 0xC2B2AE35u;
    h ^= h >> 16;  h *= 0x7FEB352Du;
    h ^= h >> 15;  h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

// proyecta un hash entero al rango [0,1).
inline float hashAFloat(uint32_t h) {
    return (h & 0xFFFFFFu) / 16777216.0f;
}

/**
 * Ruido — generador Perlin con tabla de permutación propia por semilla.
 * Dos instancias con la misma semilla producen exactamente el mismo campo.
 */
class Ruido {
public:
    explicit Ruido(uint32_t semilla);

    // ruido de gradiente 1D.
    float perlin1(float x) const;

    // ruido de gradiente 2D.
    float perlin2(float x, float y) const;

    // suma de octavas de perlin1 (montañas grandes + detalle).
    float fractal1(float x, int octavas, float persistencia) const;

    // suma de octavas de perlin2.
    float fractal2(float x, float y, int octavas, float persistencia) const;

private:
    uint8_t perm[512];   // tabla de permutación duplicada para evitar módulo

    static float suavizar(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
    static float interpolar(float a, float b, float t) { return a + t * (b - a); }
    static float gradiente1(int h, float dx) { return (h & 1) ? -dx : dx; }
    static float gradiente2(int h, float dx, float dy);
};
