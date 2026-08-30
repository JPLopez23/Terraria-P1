#pragma once

#include "config.hpp"
#include "luz.hpp"

#include <cstdio>
#include <string>

/** Tiempos de un frame, en milisegundos, por fase del pipeline. */
struct MedidorFrame {
    double ms_frame   = 0.0;   // frame completo
    double ms_luz     = 0.0;   // calcularIluminacion: el kernel
    double ms_anim    = 0.0;   // worldgen/animación/cámara
    double ms_comp    = 0.0;   // composición del framebuffer
    double ms_present = 0.0;   // SDL: 0 en --headless
};

 // Reloj : acumula FPS por ventana deslizante y escribe el CSV de mediciones.
class Reloj {
public:
    static constexpr long FRAMES_CALENTAMIENTO = 120;

    // Constructor.
    explicit Reloj(const Config& cfg) {
        if (!cfg.csv.empty()) {
            csv = std::fopen(cfg.csv.c_str(), "w");
            if (csv) {
                std::fprintf(csv,
                    "frame,version,modo,threads,n,radio,muestras,escala_luz,seed,"
                    "fase,ms_frame,ms_luz,ms_anim,ms_comp,ms_present,fps,energia,celdas_iluminadas\n");
            }
        }
    }

    ~Reloj() { if (csv) std::fclose(csv); }

    Reloj(const Reloj&) = delete;             // el FILE* tiene un solo dueño
    Reloj& operator=(const Reloj&) = delete;

    // registrar : cierra la contabilidad de un frame.
    void registrar(const Config& cfg, const char* fase,
                   const MedidorFrame& med, const EstadisticasLuz& est,
                   double tiempoGlobal) {
        ++frame;

        // FPS por ventana deslizante de ~500 ms: estable pero reactivo.
        msVentana += med.ms_frame;
        ++framesVentana;
        if (msVentana >= 500.0) {
            fpsActual = framesVentana * 1000.0 / msVentana;
            msVentana = 0.0;
            framesVentana = 0;
        }

        if (csv && (frame > FRAMES_CALENTAMIENTO || tiempoGlobal > 3.0)) {
            std::fprintf(csv,
                "%ld,%d,%s,%d,%d,%d,%d,%d,%u,%s,%.4f,%.4f,%.4f,%.4f,%.4f,%.2f,%.3f,%ld\n",
                frame, cfg.version, cfg.version == 0 ? "secuencial" : "paralela",
                cfg.threads, cfg.n, cfg.radio, cfg.muestras,
                cfg.escala_luz, cfg.seed,
                fase, med.ms_frame, med.ms_luz, med.ms_anim, med.ms_comp,
                med.ms_present, fpsActual, est.energia, est.celdasIluminadas);
        }
    }

    /** fps : último FPS calculado por la ventana deslizante. */
    double fps() const { return fpsActual; }

    /** numFrame : frames transcurridos desde el arranque. */
    long numFrame() const { return frame; }

private:
    std::FILE* csv = nullptr;
    long   frame = 0;
    double fpsActual = 0.0;
    double msVentana = 0.0;
    int    framesVentana = 0;
};
