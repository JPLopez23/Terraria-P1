/**
 * main.cpp — Terraria Forge: screensaver de mundo 2D con iluminación por
 * ray tracing de sombras, en versión secuencial y paralela con OpenMP.
 *
 * Universidad del Valle de Guatemala · Computación Paralela y Distribuida.
 *
 * En este punto: ventana SDL2, framebuffer propio, overlay de FPS y salida
 * con ESC. El mundo y la luz llegan en los siguientes commits.
 */
#include "config.hpp"
#include "render.hpp"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include <omp.h>

int main(int argc, char** argv) {
    Config cfg;
    int codigo = parsearArgs(argc, argv, cfg);
    if (codigo != SALIDA_OK) return codigo;

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

        // Degradado de prueba: confirma que el framebuffer llega a la ventana
        // con los canales en el orden correcto (ARGB8888).
        for (int py = 0; py < cfg.h; ++py) {
            for (int px = 0; px < cfg.w; ++px) {
                uint32_t r = static_cast<uint32_t>(px * 255 / cfg.w);
                uint32_t g = static_cast<uint32_t>(py * 255 / cfg.h);
                framebuffer[static_cast<size_t>(py) * cfg.w + px] =
                    0xFF000000u | (r << 16) | (g << 8) | 90u;
            }
        }

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
