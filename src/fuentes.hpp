#pragma once

#include "mundo.hpp"
#include "config.hpp"

#include <vector>

/** Qué objeto emite la luz : cambia el color y cómo se dibuja el sprite. */
enum ClaseFuente : uint8_t { FUENTE_ANTORCHA = 0, FUENTE_LAVA, FUENTE_MINERAL };

 // Fuente : una fuente de luz puntual.
struct Fuente {
    float x, y;            // posición en coordenadas de tile
    float r, g, b;         // color
    float intensidad;      // modulada por el parpadeo y el encendido: la usa el kernel
    float intensidadBase;  // intensidad nominal de diseño
    float radio;           // alcance en tiles
    float fase;            // desfase del parpadeo, para que no titilen al unísono
    uint8_t clase;         // ClaseFuente
};

 // colocarFuentes: pasada 10 del worldgen..
std::vector<Fuente> colocarFuentes(const Mundo& m, const Config& cfg);

 // actualizarParpadeo : modula la intensidad de cada fuente en el tiempo.
void actualizarParpadeo(std::vector<Fuente>& fuentes, double tiempo, float encendido);
