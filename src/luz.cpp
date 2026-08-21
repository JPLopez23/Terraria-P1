#include "luz.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <omp.h>

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
