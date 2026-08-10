/**
 * mundo.hpp — el mundo de tiles: tipos, matrices y generación procedural.
 *
 * Todo el mundo son matrices planas de grid_w × grid_h (ver matriz.hpp),
 * generadas en pasadas sucesivas: cada una asume que las anteriores ya
 * corrieron (por ahora: biomas, altura, capas, cuevas y pasto).
 */
#pragma once

#include "matriz.hpp"
#include "config.hpp"

#include <cstdint>
#include <vector>

/** Tipos de tile — el valor indexa directamente la tabla OPACIDAD y la paleta. */
enum TipoTile : uint8_t {
    AIRE = 0, TIERRA, PASTO, PIEDRA, MINERAL, LADRILLO, MADERA, HOJA, LAVA,
    NUM_TIPOS_TILE
};

/** Biomas por columna — cada mundo sortea 3 zonas horizontales por semilla. */
enum Bioma : uint8_t {
    BIOMA_BOSQUE = 0, BIOMA_NIEVE, BIOMA_CORRUPCION, BIOMA_DESIERTO,
    NUM_BIOMAS
};

/** Fila a partir de la cual el mundo se vuelve infierno (fracción del alto). */
constexpr float FRACCION_INFIERNO = 0.86f;

/** Muros de fondo — lo que hace que una cueva se vea excavada y no un agujero al cielo. */
enum TipoFondo : uint8_t {
    FONDO_NADA = 0, FONDO_TIERRA, FONDO_PIEDRA, FONDO_LADRILLO, FONDO_MADERA,
    NUM_TIPOS_FONDO
};

/** Cuánta luz absorbe cada tipo de tile: 0.0 transparente, 1.0 bloquea todo. */
constexpr float OPACIDAD[NUM_TIPOS_TILE] = {
    0.00f,  // AIRE
    0.55f,  // TIERRA
    0.50f,  // PASTO
    0.72f,  // PIEDRA
    0.45f,  // MINERAL   (translúcido, se ve el brillo a través)
    0.65f,  // LADRILLO
    0.40f,  // MADERA
    0.22f,  // HOJA      (deja pasar luz moteada entre las hojas)
    0.10f,  // LAVA      (casi transparente: emite, no bloquea)
};

/**
 * Mundo — todas las matrices del mundo, del mismo tamaño e indexadas igual.
 * M_tipo/M_fondo describen la geometría; M_anim/M_retraso/M_origX/M_origY
 * describen la animación de ensamblaje de cada tile.
 */
struct Mundo {
    int ancho = 0;   // tiles a lo ancho (grid_w)
    int alto  = 0;   // tiles a lo alto (grid_h)
    uint32_t semilla = 0;

    Matriz<uint8_t> M_tipo;     // tipo de tile (TipoTile)
    Matriz<uint8_t> M_fondo;    // muro de fondo (TipoFondo, 0 = cielo abierto)
    Matriz<float>   M_anim;     // progreso de ensamblaje: 0 fuera de pantalla, 1 en posición
    Matriz<float>   M_retraso;  // cuándo le toca a este tile empezar a volar (ola)
    Matriz<float>   M_origX;    // desde dónde entra volando (coordenada de tile)
    Matriz<float>   M_origY;

    std::vector<int> alturaSuperficie;  // fila de superficie por columna (se consulta mucho)
    std::vector<uint8_t> bioma;         // Bioma por columna (3 zonas horizontales)

    // Lugares donde las estructuras (casas, minas) quieren una antorcha:
    // la pasada 10 los usa primero, para que toda casa y mina quede iluminada.
    std::vector<std::pair<int,int>> antorchasSugeridas;

    /** esSolido — true si el tile bloquea el paso (no AIRE ni LAVA). */
    bool esSolido(int x, int y) const {
        uint8_t t = M_tipo.enOr(x, y, AIRE);
        return t != AIRE && t != LAVA;
    }
};

// corre las 10 pasadas de worldgen con una semilla dada.
Mundo generarMundo(const Config& cfg, uint32_t semilla);
