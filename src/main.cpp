#include "config.hpp"
#include "mundo.hpp"
#include "fuentes.hpp"
#include "luz.hpp"
#include "camara.hpp"
#include "render.hpp"
#include "ruido.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>
#include <vector>
#include <omp.h>

 // generarMundoValidado : worldgen con reintento de semilla: nunca falla.
static uint32_t generarMundoValidado(const Config& cfg, uint32_t semilla,
                                     Mundo& mundo, std::vector<Fuente>& fuentes) {
    // El terreno inválido se reintenta muchas veces: es barato y raro;
    int mejorCuenta = -1;
    Mundo mejorMundo;
    std::vector<Fuente> mejorFuentes;
    uint32_t mejorSemilla = semilla;
    int intentosFuentes = 0;

    for (int intento = 0; intento < 64; ++intento) {
        mundo = generarMundo(cfg, semilla);
        if (validarTerreno(mundo)) {
            fuentes = colocarFuentes(mundo, cfg);
            int cuenta = static_cast<int>(fuentes.size());
            if (cuenta >= cfg.n) return semilla;
            if (cuenta > mejorCuenta) {
                mejorCuenta = cuenta;
                mejorMundo = mundo;
                mejorFuentes = fuentes;
                mejorSemilla = semilla;
            }
            if (++intentosFuentes >= 6) break;
            std::fprintf(stderr,
                "Semilla %u: solo %d de %d fuentes; reintentando con otra semilla.\n",
                semilla, cuenta, cfg.n);
        }
        semilla = mezclarHash(semilla, 0xBADA55u, static_cast<uint32_t>(intento + 1));
    }

    // Nunca fallar: el screensaver corre con las fuentes que sí caben.
    if (mejorCuenta >= 0) {
        mundo = mejorMundo;
        fuentes = mejorFuentes;
        std::fprintf(stderr,
            "Advertencia: el mundo solo aloja %d de las %d fuentes pedidas.\n",
            mejorCuenta, cfg.n);
    } else {
        // Ni un terreno pasó la validación en 64 semillas no debería
        // ocurrir: se usa el último mundo generado tal cual.
        fuentes = colocarFuentes(mundo, cfg);
    }
    return mejorSemilla;
}

int main(int argc, char** argv) {
    Config cfg;
    int codigo = parsearArgs(argc, argv, cfg);
    if (codigo != SALIDA_OK) return codigo;

    // Semilla aleatoria salvo que el usuario haya fijado una con --seed
    // fijarla es lo que hace reproducibles las mediciones más adelante.
    if (!cfg.seed_fija)
        cfg.seed = mezclarHash(static_cast<uint32_t>(std::time(nullptr)),
                               static_cast<uint32_t>(std::clock()), 0x5EEDu);

    // Ventana solo si no es headless : el modo headless aísla el costo de
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

    // Lightmap alineado a pantalla: más fino que los tiles: §2.5 del plan.
    Lightmap L;
    L.redimensionar(cfg.w / cfg.escala_luz, cfg.h / cfg.escala_luz);
    L.escala = static_cast<float>(cfg.escala_luz) / TILE_PX;   // tiles por celda

    std::vector<uint32_t> framebuffer(static_cast<size_t>(cfg.w) * cfg.h, 0);

    Mundo mundo;
    std::vector<Fuente> fuentes;
    uint32_t semillaCiclo = generarMundoValidado(cfg, cfg.seed, mundo, fuentes);
    std::printf("Mundo con semilla %u: %zu fuentes de luz.\n",
                semillaCiclo, fuentes.size());

    Camara camara;
    camara.reiniciar(mundo, cfg);

    const double tInicio = omp_get_wtime();
    double tPrev = tInicio;
    double msVentana = 0.0;      // acumulador de la ventana deslizante de FPS
    int    framesVentana = 0;
    double fps = 0.0;
    double ultimoLog = 0.0;
    long   frames = 0;
    bool   salir = false;
    bool   capturaHecha = false;

    while (!salir) {
        const double tFrame0 = omp_get_wtime();
        double dt = tFrame0 - tPrev;
        tPrev = tFrame0;
        if (dt > 0.1) dt = 0.1;              // evitar saltos tras el worldgen
        const double tiempoGlobal = tFrame0 - tInicio;

        if (pantalla && pantalla->procesarEventos()) salir = true;
        if (cfg.duration > 0.0 && tiempoGlobal >= cfg.duration) salir = true;

        actualizarParpadeo(fuentes, tiempoGlobal, 1.0f);
        camara.actualizar(static_cast<float>(dt), mundo);

        // El lightmap sigue a la cámara, alineado al mismo píxel entero que
        // usa el render: si no coincidieran, la luz "nadaría" sobre los tiles.
        L.origenX = std::floor(camara.x * TILE_PX) / TILE_PX;
        L.origenY = std::floor(camara.y * TILE_PX) / TILE_PX;

        // El kernel: iluminación por ray tracing: ms_luz 
        const double tLuz0 = omp_get_wtime();
        EstadisticasLuz est = calcularIluminacion(mundo, fuentes, L, cfg);
        const double msLuz = (omp_get_wtime() - tLuz0) * 1000.0;

        componerFrame(mundo, L, camara, tiempoGlobal, cfg, framebuffer.data());

        char hud[64];
        std::snprintf(hud, sizeof(hud), "FPS %.1f | N %d", fps, cfg.n);
        dibujarTexto(framebuffer.data(), cfg.w, cfg.h, 10, 10, 2, hud, 0xFFFFFFFFu);

        // Presentación: SOLO el hilo maestro toca SDL: no es thread-safe.
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

        if (tiempoGlobal - ultimoLog >= 1.0) {
            ultimoLog = tiempoGlobal;
            std::printf("FPS %6.1f | luz %7.2f ms | energia %.1f | celdas iluminadas %ld\n",
                        fps, msLuz, est.energia, est.celdasIluminadas);
            std::fflush(stdout);
        }

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
