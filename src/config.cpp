#include "config.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <omp.h>

void imprimirUso(const char* nombrePrograma) {
    std::fprintf(stderr,
        "Uso: %s [opciones]\n"
        "  --n <int>           fuentes de luz a renderizar (0..1000000, default 150)\n"
        "                      0 = solo luz ambiental; sube N para estresar la maquina\n"
        "  --version <int>     0 = SECUENCIAL, 1 = PARALELA con OpenMP parallel for (default 1)\n"
        "  --threads <int>     hilos OpenMP de la version paralela (1..256, default: nucleos)\n"
        "  --w <int>           ancho de ventana en px (>=640, default 1280)\n"
        "  --h <int>           alto de ventana en px (>=480, default 720)\n"
        "  --grid <AxB>        tamano del mundo en tiles (>=64x64, default 400x240)\n"
        "  --radio <int>       alcance de la luz en tiles (1..256, default 24)\n"
        "  --muestras <int>    muestras de sombra suave K (1..16, default 4)\n"
        "  --escala-luz <int>  px por celda de lightmap {1,2,4,8} (default 2; 4-8 = mas FPS)\n"
        "  --seed <uint>       semilla del mundo (default: aleatoria)\n"
        "  --duration <float>  segundos antes de salir (default: infinito)\n"
        "  --csv <ruta>        volcar mediciones por frame a un CSV\n"
        "  --headless          sin ventana: mide solo el computo\n"
        "  --vsync             activar VSync (NUNCA al medir rendimiento)\n"
        "  --captura <ruta>    volcar un frame a BMP y salir (depuracion)\n"
        "  --captura-t <float> segundos simulados antes de la captura (default 3)\n"
        "  --test-luz          un frame determinista: imprime checksum del lightmap y sale\n"
        "  --medicion          carga fija reproducible para benchmark (mundo encendido, camara fija)\n"
        "  --help              esta ayuda\n"
        "  ESC cierra el programa.\n",
        nombrePrograma);
}

 // leerEntero : convierte texto a entero con validación estricta.
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

 // leerFlotante : convierte texto a double con validación estricta.
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
        } else if (std::strcmp(a, "--n") == 0) {
            const char* t = requiereValor(a); if (!t) return SALIDA_ERROR_USO;
            // 0 = solo luz ambiental; el tope alto existe para poder llevar
            // la máquina al límite: el costo crece ~linealmente con N.
            if (!leerEntero(t, a, 0, 1000000, &v)) return SALIDA_ERROR_VALOR;
            cfg.n = static_cast<int>(v);
        } else if (std::strcmp(a, "--version") == 0) {
            const char* t = requiereValor(a); if (!t) return SALIDA_ERROR_USO;
            if (!leerEntero(t, a, 0, 1, &v)) return SALIDA_ERROR_VALOR;
            cfg.version = static_cast<int>(v);
        } else if (std::strcmp(a, "--threads") == 0) {
            const char* t = requiereValor(a); if (!t) return SALIDA_ERROR_USO;
            if (!leerEntero(t, a, 1, 256, &v)) return SALIDA_ERROR_VALOR;
            cfg.threads = static_cast<int>(v);
        } else if (std::strcmp(a, "--w") == 0) {
            const char* t = requiereValor(a); if (!t) return SALIDA_ERROR_USO;
            if (!leerEntero(t, a, 640, 7680, &v)) return SALIDA_ERROR_VALOR;
            cfg.w = static_cast<int>(v);
        } else if (std::strcmp(a, "--h") == 0) {
            const char* t = requiereValor(a); if (!t) return SALIDA_ERROR_USO;
            if (!leerEntero(t, a, 480, 4320, &v)) return SALIDA_ERROR_VALOR;
            cfg.h = static_cast<int>(v);
        } else if (std::strcmp(a, "--grid") == 0) {
            const char* t = requiereValor(a); if (!t) return SALIDA_ERROR_USO;
            // Formato AxB validado a mano: dos enteros separados por 'x'.
            const char* sep = std::strchr(t, 'x');
            if (!sep || sep == t || *(sep + 1) == '\0') {
                std::fprintf(stderr, "Error: --grid espera formato AxB (ej. 400x240), recibio \"%s\".\n", t);
                return SALIDA_ERROR_VALOR;
            }
            std::string parteA(t, sep - t);
            long ga = 0, gb = 0;
            if (!leerEntero(parteA.c_str(), "--grid (ancho)", 64, 16384, &ga)) return SALIDA_ERROR_VALOR;
            if (!leerEntero(sep + 1,        "--grid (alto)",  64, 16384, &gb)) return SALIDA_ERROR_VALOR;
            cfg.grid_w = static_cast<int>(ga);
            cfg.grid_h = static_cast<int>(gb);
        } else if (std::strcmp(a, "--radio") == 0) {
            const char* t = requiereValor(a); if (!t) return SALIDA_ERROR_USO;
            if (!leerEntero(t, a, 1, 256, &v)) return SALIDA_ERROR_VALOR;
            cfg.radio = static_cast<int>(v);
        } else if (std::strcmp(a, "--muestras") == 0) {
            const char* t = requiereValor(a); if (!t) return SALIDA_ERROR_USO;
            if (!leerEntero(t, a, 1, 16, &v)) return SALIDA_ERROR_VALOR;
            cfg.muestras = static_cast<int>(v);
        } else if (std::strcmp(a, "--escala-luz") == 0) {
            const char* t = requiereValor(a); if (!t) return SALIDA_ERROR_USO;
            if (!leerEntero(t, a, 1, 8, &v)) return SALIDA_ERROR_VALOR;
            if (v != 1 && v != 2 && v != 4 && v != 8) {
                std::fprintf(stderr, "Error: --escala-luz debe ser 1, 2, 4 u 8.\n");
                return SALIDA_ERROR_VALOR;
            }
            cfg.escala_luz = static_cast<int>(v);
        } else if (std::strcmp(a, "--seed") == 0) {
            const char* t = requiereValor(a); if (!t) return SALIDA_ERROR_USO;
            errno = 0;
            char* fin = nullptr;
            unsigned long s = std::strtoul(t, &fin, 10);
            if (errno != 0 || fin == t || *fin != '\0') {
                std::fprintf(stderr, "Error: --seed espera un entero sin signo, recibio \"%s\".\n", t);
                return SALIDA_ERROR_VALOR;
            }
            cfg.seed = static_cast<uint32_t>(s);
            cfg.seed_fija = true;
        } else if (std::strcmp(a, "--duration") == 0) {
            const char* t = requiereValor(a); if (!t) return SALIDA_ERROR_USO;
            if (!leerFlotante(t, a, 0.0, &cfg.duration)) return SALIDA_ERROR_VALOR;
        } else if (std::strcmp(a, "--csv") == 0) {
            const char* t = requiereValor(a); if (!t) return SALIDA_ERROR_USO;
            cfg.csv = t;
        } else if (std::strcmp(a, "--captura") == 0) {
            const char* t = requiereValor(a); if (!t) return SALIDA_ERROR_USO;
            cfg.captura = t;
        } else if (std::strcmp(a, "--captura-t") == 0) {
            const char* t = requiereValor(a); if (!t) return SALIDA_ERROR_USO;
            if (!leerFlotante(t, a, 0.0, &cfg.captura_t)) return SALIDA_ERROR_VALOR;
        } else if (std::strcmp(a, "--medicion") == 0) {
            cfg.medicion = true;
        } else if (std::strcmp(a, "--test-luz") == 0) {
            cfg.test_luz = true;
            cfg.headless = true;   // el test es de cómputo puro, sin ventana
        } else if (std::strcmp(a, "--headless") == 0) {
            cfg.headless = true;
        } else if (std::strcmp(a, "--vsync") == 0) {
            cfg.vsync = true;
        } else {
            std::fprintf(stderr, "Error: flag desconocido \"%s\".\n", a);
            imprimirUso(argv[0]);
            return SALIDA_ERROR_USO;
        }
    }

    // Validaciones cruzadas y advertencias 
    int nucleos = omp_get_num_procs();
    if (cfg.threads == 0) cfg.threads = nucleos;
    // La versión secuencial corre en un solo hilo por definición: si el
    // usuario pidió hilos con --version 0, se le avisa que no aplican.
    if (cfg.version == 0 && cfg.threads != 1) {
        if (cfg.threads > 1)
            std::fprintf(stderr,
                "Nota: --version 0 es la version SECUENCIAL; --threads %d se ignora.\n",
                cfg.threads);
        cfg.threads = 1;
    }
    if (cfg.threads > nucleos) {
        std::fprintf(stderr,
            "Advertencia: --threads %d excede los %d procesadores logicos disponibles; "
            "habra sobresuscripcion.\n", cfg.threads, nucleos);
    }

    // El costo del kernel escala ~N * radio^3: advertir combinaciones absurdas
    // antes de dejar al usuario mirando un frame de varios segundos.
    double costoRelativo = static_cast<double>(cfg.n) * cfg.radio * cfg.radio * cfg.radio
                         * cfg.muestras / (150.0 * 24.0 * 24.0 * 24.0 * 4.0);
    if (costoRelativo > 40.0) {
        std::fprintf(stderr,
            "Advertencia: --n %d con --radio %d y --muestras %d implica ~%.0fx el costo "
            "de la configuracion por defecto; cada frame puede tardar segundos.\n",
            cfg.n, cfg.radio, cfg.muestras, costoRelativo);
    }

    // La ruta del CSV se verifica escribible ANTES de empezar a medir,
    // no al final cuando los datos ya se perderian.
    if (!cfg.csv.empty()) {
        std::FILE* prueba = std::fopen(cfg.csv.c_str(), "a");
        if (!prueba) {
            std::fprintf(stderr, "Error: no se puede escribir el CSV en \"%s\".\n", cfg.csv.c_str());
            return SALIDA_ERROR_ARCHIVO;
        }
        std::fclose(prueba);
    }

    return SALIDA_OK;
}
