/**
 * main.cpp — Terraria Forge: screensaver de mundo 2D con iluminación por
 * ray tracing de sombras, en versión secuencial y paralela con OpenMP.
 *
 * Universidad del Valle de Guatemala · Computación Paralela y Distribuida.
 *
 * En este punto: se genera un mundo con la semilla dada y se dibuja en
 * pantalla. La iluminación y el ciclo de animación llegan después.
 */
#include "config.hpp"
#include "mundo.hpp"
#include "camara.hpp"
#include "render.hpp"
#include "ruido.hpp"

#include <cstdint>
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>
#include <vector>
#include <omp.h>

int main(int argc, char** argv) {
    Config cfg;
    int codigo = parsearArgs(argc, argv, cfg);
    if (codigo != SALIDA_OK) return codigo;

    // Semilla aleatoria salvo que el usuario haya fijado una con --seed
    // (fijarla es lo que hace reproducibles las mediciones más adelante).
    if (!cfg.seed_fija)
        cfg.seed = mezclarHash(static_cast<uint32_t>(std::time(nullptr)),
                               static_cast<uint32_t>(std::clock()), 0x5EEDu);

    std::printf("Terraria Forge | mundo %dx%d tiles | semilla %u%s\n",
                cfg.grid_w, cfg.grid_h, cfg.seed, cfg.headless ? " | headless" : "");

    // Ventana solo si no es headless — el modo headless aísla el costo de
    // SDL y mide únicamente el cómputo.
    std::unique_ptr<Pantalla> pantalla;
    if (!cfg.headless) {
        try {
            pantalla.reset(new Pantalla(cfg));
        } catch (const std::exception& e) {
            std::fprintf(stderr, "Error al inicializar SDL: %s\n", e.what());
            return SALIDA_ERROR_SDL;
        }
    }

    std::vector<uint32_t> framebuffer(static_cast<size_t>(cfg.w) * cfg.h, 0);

    // Por ahora el mundo se genera una sola vez, antes de entrar al ciclo.
    Mundo mundo = generarMundo(cfg, cfg.seed);
    Camara camara;
    camara.reiniciar(mundo, cfg);

    const double tInicio = omp_get_wtime();
    double msVentana = 0.0;      // acumulador de la ventana deslizante de FPS
    int    framesVentana = 0;
    double fps = 0.0;
    long   frames = 0;
    bool   salir = false;
    bool   capturaHecha = false;

    while (!salir) {
        const double tFrame0 = omp_get_wtime();
        const double tiempoGlobal = tFrame0 - tInicio;

        if (pantalla && pantalla->procesarEventos()) salir = true;
        if (cfg.duration > 0.0 && tiempoGlobal >= cfg.duration) salir = true;

        componerFrame(mundo, camara, tiempoGlobal, cfg, framebuffer.data());

        char hud[64];
        std::snprintf(hud, sizeof(hud), "FPS %.1f", fps);
        dibujarTexto(framebuffer.data(), cfg.w, cfg.h, 10, 10, 2, hud, 0xFFFFFFFFu);

        // Presentación: SOLO el hilo maestro toca SDL (no es thread-safe).
        if (pantalla) pantalla->presentar(framebuffer.data());

        // FPS por ventana deslizante de ~500 ms: estable pero reactivo.
        msVentana += (omp_get_wtime() - tFrame0) * 1000.0;
        ++framesVentana;
        if (msVentana >= 500.0) {
            fps = framesVentana * 1000.0 / msVentana;
            msVentana = 0.0;
            framesVentana = 0;
            if (pantalla) {
                char titulo[96];
                std::snprintf(titulo, sizeof(titulo), "Terraria Forge - FPS %.1f", fps);
                pantalla->ponerTitulo(titulo);
            }
        }
        ++frames;

        // Captura de depuración: volcar un frame a BMP y salir.
        if (!cfg.captura.empty() && !capturaHecha && tiempoGlobal >= cfg.captura_t) {
            capturaHecha = true;
            if (volcarBMP(cfg.captura, framebuffer.data(), cfg.w, cfg.h))
                std::printf("Captura guardada en %s\n", cfg.captura.c_str());
            else
                std::fprintf(stderr, "No se pudo escribir la captura %s\n", cfg.captura.c_str());
            salir = true;
        }
    }

    std::printf("Frames totales: %ld | FPS final: %.1f\n", frames, fps);
    return SALIDA_OK;
}
