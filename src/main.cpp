#include "config.hpp"
#include "mundo.hpp"
#include "fuentes.hpp"
#include "luz.hpp"
#include "animacion.hpp"
#include "camara.hpp"
#include "render.hpp"
#include "reloj.hpp"
#include "ruido.hpp"

#include <cmath>
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

    if (!cfg.seed_fija)
        cfg.seed = mezclarHash(static_cast<uint32_t>(std::time(nullptr)),
                               static_cast<uint32_t>(std::clock()), 0x5EEDu);

    std::printf("Terraria Forge | N=%d | semilla %u%s\n",
                cfg.n, cfg.seed, cfg.headless ? " | headless" : "");

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

    //  Modo de prueba: un frame determinista y salir 
    if (cfg.test_luz) {
        Mundo mundoTest;
        std::vector<Fuente> fuentesTest;
        generarMundoValidado(cfg, cfg.seed, mundoTest, fuentesTest);
        fijarAnimCompleta(mundoTest);
        actualizarParpadeo(fuentesTest, 1.234, 1.0f);   // tiempo fijo → intensidades fijas

        Camara camTest;
        camTest.reiniciar(mundoTest, cfg);
        // La vista de prueba baja al subsuelo, donde hay antorchas y sombras.
        camTest.y = std::min(static_cast<float>(mundoTest.alto - camTest.visH),
                             camTest.y + camTest.visH * 0.8f);

        Lightmap Ltest;
        Ltest.redimensionar(cfg.w / cfg.escala_luz, cfg.h / cfg.escala_luz);
        Ltest.escala  = static_cast<float>(cfg.escala_luz) / TILE_PX;
        Ltest.origenX = std::floor(camTest.x * TILE_PX) / TILE_PX;
        Ltest.origenY = std::floor(camTest.y * TILE_PX) / TILE_PX;

        double t0 = omp_get_wtime();
        EstadisticasLuz e = calcularIluminacion(mundoTest, fuentesTest, Ltest, cfg);
        double ms = (omp_get_wtime() - t0) * 1000.0;

        // Checksum ponderado por posición: detecta luz correcta en el lugar
        // equivocado, cosa que la energía total sola no ve.
        double ponderado = 0.0;
        for (size_t i = 0; i < Ltest.M_luzR.size(); ++i)
            ponderado += (Ltest.M_luzR[i] * 2.0 + Ltest.M_luzG[i] * 3.0 + Ltest.M_luzB[i] * 5.0)
                       * ((i % 8191) + 1);
        std::printf("TEST energia=%.6f ponderado=%.6e celdas=%ld ms=%.2f\n",
                    e.energia, ponderado, e.celdasIluminadas, ms);
        return SALIDA_OK;
    }

    // Lightmap alineado a pantalla: más fino que los tiles: §2.5 del plan.
    Lightmap L;
    L.redimensionar(cfg.w / cfg.escala_luz, cfg.h / cfg.escala_luz);
    L.escala = static_cast<float>(cfg.escala_luz) / TILE_PX;   // tiles por celda

    std::vector<uint32_t> framebuffer(static_cast<size_t>(cfg.w) * cfg.h, 0);

    Mundo mundo;
    std::vector<Fuente> fuentes;
    Camara camara;

    Fase  fase  = Fase::GENERANDO;
    float tFase = 0.0f;
    uint32_t semillaCiclo = cfg.seed;

    const double tInicio = omp_get_wtime();
    double tPrev = tInicio;
    double ultimoLog = 0.0;
    double msVentana = 0.0;
    int    framesVentana = 0;
    double fps = 0.0;
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

        // Máquina de estados + animación 
        switch (fase) {
            case Fase::GENERANDO: {
                semillaCiclo = generarMundoValidado(cfg, semillaCiclo, mundo, fuentes);
                inicializarAnimacion(mundo);
                camara.reiniciar(mundo, cfg);
                std::printf("Mundo con semilla %u: %zu fuentes de luz.\n",
                            semillaCiclo, fuentes.size());
                if (cfg.medicion) {
                    // Modo benchmark: mundo ya ensamblado y encendido, cámara
                    fijarAnimCompleta(mundo);
                    camara.y = std::min(static_cast<float>(mundo.alto - camara.visH),
                                        camara.y + camara.visH * 0.8f);
                    fase = Fase::RECORRIENDO;
                } else {
                    fase = Fase::ENSAMBLANDO;
                }
                tFase = 0.0f;
                break;
            }
            case Fase::ENSAMBLANDO:
                actualizarEnsamblaje(mundo, tFase);
                if (tFase >= DUR_ENSAMBLANDO) {
                    fijarAnimCompleta(mundo);
                    fase = Fase::ENCENDIENDO;  tFase = 0.0f;
                }
                break;
            case Fase::ENCENDIENDO:
                if (tFase >= DUR_ENCENDIENDO) { fase = Fase::RECORRIENDO;  tFase = 0.0f; }
                break;
            case Fase::RECORRIENDO:
                // En modo medición la fase nunca termina: carga constante.
                if (!cfg.medicion && tFase >= DUR_RECORRIENDO) {
                    fase = Fase::DESARMANDO;  tFase = 0.0f;
                }
                break;
            case Fase::DESARMANDO:
                actualizarDesarme(mundo, tFase);
                if (tFase >= DUR_DESARMANDO + 1.0f) {
                    semillaCiclo = mezclarHash(semillaCiclo, 0xC1C10u,
                                               static_cast<uint32_t>(frames));
                    fase = Fase::GENERANDO;  tFase = 0.0f;
                }
                break;
        }

        // Progreso de encendido de las antorchas: 0..1 según la fase.
        float encendido = 0.0f;
        if      (fase == Fase::ENCENDIENDO) encendido = tFase / DUR_ENCENDIENDO;
        else if (fase == Fase::RECORRIENDO) encendido = 1.0f;
        else if (fase == Fase::DESARMANDO)  encendido = std::max(0.0f, 1.0f - tFase / DUR_DESARMANDO);

        if (cfg.medicion) {
            // Parpadeo congelado y cámara quieta: el trabajo por frame es
            // idéntico entre corridas.
            actualizarParpadeo(fuentes, 1.234, 1.0f);
        } else {
            actualizarParpadeo(fuentes, tiempoGlobal, encendido);
            camara.actualizar(fase, tFase, static_cast<float>(dt), mundo);
        }

        // El lightmap sigue a la cámara, alineado al mismo píxel entero que
        // usa el render: si no coincidieran, la luz "nadaría" sobre los tiles.
        L.origenX = std::floor(camara.x * TILE_PX) / TILE_PX;
        L.origenY = std::floor(camara.y * TILE_PX) / TILE_PX;

        // El kernel: iluminación por ray tracing 
        const double tLuz0 = omp_get_wtime();
        EstadisticasLuz est = calcularIluminacion(mundo, fuentes, L, cfg);
        const double msLuz = (omp_get_wtime() - tLuz0) * 1000.0;

        componerFrame(mundo, L, fuentes, camara, fase, tFase, tiempoGlobal,
                      cfg, framebuffer.data());

        char hud[96];
        std::snprintf(hud, sizeof(hud), "FPS %.1f | N %d | %s", fps, cfg.n, nombreFase(fase));
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
            std::printf("FPS %6.1f | luz %7.2f ms | %s\n", fps, msLuz, nombreFase(fase));
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

        tFase += static_cast<float>(dt);
    }

    std::printf("Frames totales: %ld | FPS final: %.1f\n", frames, fps);
    return SALIDA_OK;
}
