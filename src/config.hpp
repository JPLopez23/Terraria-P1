#pragma once

#include <cstdint>
#include <string>

/** Códigos de salida distintos por tipo de error: programación defensiva. */
enum CodigoSalida {
    SALIDA_OK              = 0,
    SALIDA_ERROR_USO       = 2,   // flag desconocido o sin valor
    SALIDA_ERROR_VALOR     = 3,   // valor no numérico o fuera de rango
    SALIDA_ERROR_ARCHIVO   = 4,   // ruta de CSV no escribible
    SALIDA_ERROR_SDL       = 5    // fallo al inicializar SDL
};
/** Config : todos los parámetros del programa. Sin valores hard-coded en el resto del código. */
struct Config {
    int         n           = 150;      // --n: cantidad de fuentes de luz: el parámetro N del enunciado
    int         w           = 1280;     // --w: ancho de ventana en píxeles: mínimo 640
    int         h           = 720;      // --h: alto de ventana en píxeles: mínimo 480
    int         grid_w      = 400;      // --grid AxB: ancho del mundo en tiles
    int         grid_h      = 240;      // --grid AxB: alto del mundo en tiles
    int         radio       = 24;       // --radio: alcance de cada fuente en tiles
    int         muestras    = 4;        // --muestras: K muestras de sombra suave por fuente
    int         escala_luz  = 2;        // --escala-luz: píxeles por celda de lightmap {1,2,4,8}
                                        // 2 = gradientes finos, ~45-60 FPS en paralelo;
                                        // 4 u 8 para más FPS, 1 para estresar el kernel
    uint32_t    seed        = 0;        // --seed: semilla del mundo: 0 = aleatoria
    bool        seed_fija   = false;    // true si el usuario pasó --seed
    double      duration    = 0.0;      // --duration: segundos antes de salir: 0 = infinito
    std::string csv         = "";       // --csv: ruta del archivo de mediciones
    bool        headless    = false;    // --headless: sin ventana, mide solo cómputo
    bool        vsync       = false;    // --vsync: sincronizar con el refresco: ¡apagado al medir!
    std::string captura     = "";       // --captura: volcar un frame a BMP y salir: depuración visual
    double      captura_t   = 3.0;      // --captura-t: segundos de simulación antes de capturar
    bool        test_luz    = false;    // --test-luz: un frame determinista, imprime checksum y sale
                                        // verifica que la versión paralela produzca la misma
                                        // imagen que la secuencial
    bool        medicion    = false;    // --medicion: carga fija y reproducible para el benchmark:
                                        // mundo ensamblado y encendido desde el frame 0, cámara
};
 // parsearArgs : lee y valida los argumentos del programa.
int parsearArgs(int argc, char** argv, Config& cfg);

 // imprimirUso : muestra la ayuda de la línea de comandos.
void imprimirUso(const char* nombrePrograma);
