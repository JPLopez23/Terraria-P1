/**
 * matriz.hpp — indexado plano row-major y plantilla de matriz 2D.
 *
 * Todo el estado del programa vive en matrices planas (un solo bloque
 * contiguo de memoria) indexadas igual. Un vector de vectores guardaría
 * cada fila en un lugar distinto del heap y desperdiciaría la caché;
 * un vector plano lo lee el prefetcher de corrido y permite recorrer
 * todo con un solo lazo de 0 a ancho*alto — la forma más simple de
 * repartir trabajo entre hilos con OpenMP.
 */
#pragma once

#include <vector>
#include <cstdint>

// convierte coordenadas 2D a índice plano row-major.
inline int idx(int x, int y, int ancho) { return y * ancho + x; }

// matriz 2D sobre un vector plano contiguo
template <typename T>
struct Matriz {
    int ancho = 0;
    int alto  = 0;
    std::vector<T> datos;

    Matriz() = default;
    Matriz(int ancho_, int alto_, T valorInicial = T{})
        : ancho(ancho_), alto(alto_),
          datos(static_cast<size_t>(ancho_) * alto_, valorInicial) {}

    // acceso directo, sin chequeo de límites
    T&       en(int x, int y)       { return datos[idx(x, y, ancho)]; }
    const T& en(int x, int y) const { return datos[idx(x, y, ancho)]; }

    bool dentro(int x, int y) const {
        return x >= 0 && x < ancho && y >= 0 && y < alto;
    }

    // como en(), pero devuelve porDefecto si (x,y) queda fuera
    T enOr(int x, int y, T porDefecto) const {
        return dentro(x, y) ? datos[idx(x, y, ancho)] : porDefecto;
    }

    void llenar(T valor) { datos.assign(datos.size(), valor); }
};
