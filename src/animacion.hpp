#pragma once

#include "mundo.hpp"

/** Máquina de estados del screensaver: ver plan §6.1. */
enum class Fase { GENERANDO, ENSAMBLANDO, ENCENDIENDO, RECORRIENDO, DESARMANDO };

/** Duraciones de cada fase en segundos. */
constexpr float DUR_ENSAMBLANDO = 6.0f;
constexpr float DUR_ENCENDIENDO = 3.0f;
constexpr float DUR_RECORRIENDO = 14.0f;   // el tour cielo → cuevas → infierno
constexpr float DUR_DESARMANDO  = 5.0f;

/** Física del desarme: caída parabólica y = v0 t + ½ g t². */
constexpr float GRAVEDAD_TILES   = 55.0f;  // tiles/s²
constexpr float VEL_INICIAL_CAIDA = -6.0f; // tiles/s: salto corto hacia arriba
constexpr float ESPERA_DESARME   = 3.0f;   // s: dispersión de inicios de caída

 // easeOutCubic : arranque rápido, llegada suave: tiles aterrizando.
inline float easeOutCubic(float t) {
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}

 // easeInOutCubic : aceleración y frenado suaves: viaje de cámara.
inline float easeInOutCubic(float t) {
    return (t < 0.5f) ? 4.0f * t * t * t
                      : 1.0f - 4.0f * (1.0f - t) * (1.0f - t) * (1.0f - t);
}

 // nombreFase : nombre legible para el HUD y el CSV.
const char* nombreFase(Fase fase);

 // inicializarAnimacion : prepara la ola de ensamblaje de un mundo nuevo.
void inicializarAnimacion(Mundo& m);

 // actualizarEnsamblaje : avanza la ola de ensamblaje.
void actualizarEnsamblaje(Mundo& m, float tFase);

 // fijarAnimCompleta : deja todos los tiles asentados: M_anim = 1.
void fijarAnimCompleta(Mundo& m);

 // actualizarDesarme : los tiles dejan de bloquear luz mientras caen.
void actualizarDesarme(Mundo& m, float tFase);

 // tiempoCaida : segundos que lleva cayendo un tile durante el desarme.
inline float tiempoCaida(float retraso, float tFase) {
    float t = tFase - retraso * ESPERA_DESARME;
    return (t > 0.0f) ? t : 0.0f;
}
