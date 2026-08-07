/**
 * config.cpp — parseo defensivo de la línea de comandos (ver config.hpp).
 */
#include "config.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

void imprimirUso(const char* nombrePrograma) {
    std::fprintf(stderr,
        "Uso: %s [opciones]\n"
        "  --w <int>           ancho de ventana en px (>=640, default 1280)\n"
        "  --h <int>           alto de ventana en px (>=480, default 720)\n"
        "  --duration <float>  segundos antes de salir (default: infinito)\n"
        "  --vsync             activar VSync (NUNCA al medir rendimiento)\n"
        "  --help              esta ayuda\n"
        "  ESC cierra el programa.\n",
        nombrePrograma);
}

// convierte texto a entero con validación estricta.
static bool leerEntero(const char* texto, const char* flag,
                       long minimo, long maximo, long* destino) {
    errno = 0;
    char* fin = nullptr;
    long v = std::strtol(texto, &fin, 10);
    if (errno != 0 || fin == texto || *fin != '\0') {
        std::fprintf(stderr, "Error: %s espera un entero, recibio \"%s\".\n", flag, texto);
        return false;
    }
    if (v < minimo || v > maximo) {
        std::fprintf(stderr, "Error: %s fuera de rango [%ld..%ld]: %ld.\n", flag, minimo, maximo, v);
        return false;
    }
    *destino = v;
    return true;
}

// convierte texto a double con validación estricta.
static bool leerFlotante(const char* texto, const char* flag,
                         double minimoExclusivo, double* destino) {
    errno = 0;
    char* fin = nullptr;
    double v = std::strtod(texto, &fin);
    if (errno != 0 || fin == texto || *fin != '\0') {
        std::fprintf(stderr, "Error: %s espera un numero, recibio \"%s\".\n", flag, texto);
        return false;
    }
    if (v <= minimoExclusivo) {
        std::fprintf(stderr, "Error: %s debe ser > %g: %g.\n", flag, minimoExclusivo, v);
        return false;
    }
    *destino = v;
    return true;
}

int parsearArgs(int argc, char** argv, Config& cfg) {
    // Recorre pares --flag valor; los flags booleanos no llevan valor.
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];

        auto requiereValor = [&](const char* flag) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "Error: %s requiere un valor.\n", flag);
                return nullptr;
            }
            return argv[++i];
        };

        long v = 0;
        if (std::strcmp(a, "--help") == 0 || std::strcmp(a, "-h") == 0) {
            imprimirUso(argv[0]);
            return SALIDA_ERROR_USO;
        } else if (std::strcmp(a, "--w") == 0) {
            const char* t = requiereValor(a); if (!t) return SALIDA_ERROR_USO;
            if (!leerEntero(t, a, 640, 7680, &v)) return SALIDA_ERROR_VALOR;
            cfg.w = static_cast<int>(v);
        } else if (std::strcmp(a, "--h") == 0) {
            const char* t = requiereValor(a); if (!t) return SALIDA_ERROR_USO;
            if (!leerEntero(t, a, 480, 4320, &v)) return SALIDA_ERROR_VALOR;
            cfg.h = static_cast<int>(v);
        } else if (std::strcmp(a, "--duration") == 0) {
            const char* t = requiereValor(a); if (!t) return SALIDA_ERROR_USO;
            if (!leerFlotante(t, a, 0.0, &cfg.duration)) return SALIDA_ERROR_VALOR;
        } else if (std::strcmp(a, "--vsync") == 0) {
            cfg.vsync = true;
        } else {
            std::fprintf(stderr, "Error: flag desconocido \"%s\".\n", a);
            imprimirUso(argv[0]);
            return SALIDA_ERROR_USO;
        }
    }

    return SALIDA_OK;
}
