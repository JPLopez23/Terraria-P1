/**
 * render.hpp — framebuffer, paleta estilo Terraria, composición y SDL.
 *
 * Regla arquitectónica que no se negocia: los hilos (cuando los haya)
 * escriben en el std::vector<uint32_t> del framebuffer, y SOLO el hilo
 * maestro llama a SDL_UpdateTexture + SDL_RenderCopy + SDL_RenderPresent.
 * SDL2 no es thread-safe para render.
 */
#pragma once

#include "config.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

/**
 * Pantalla — envoltorio RAII de SDL: ventana, renderer y textura.
 * El constructor verifica el retorno de CADA llamada SDL con SDL_GetError()
 * y lanza std::runtime_error si algo falla; el destructor libera todo en
 * orden inverso aun ante una excepción o un return temprano.
 */
class Pantalla {
public:
    explicit Pantalla(const Config& cfg);
    ~Pantalla();

    Pantalla(const Pantalla&) = delete;             // un solo dueño de los
    Pantalla& operator=(const Pantalla&) = delete;  // recursos SDL

    // vacía la cola de eventos SDL.
    bool procesarEventos();

    // sube el framebuffer a la textura y lo muestra.
    void presentar(const uint32_t* fb);

    // actualiza el título de la ventana (FPS, versión...).
    void ponerTitulo(const std::string& titulo);

private:
    SDL_Window*   ventana  = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture*  textura  = nullptr;
    int ancho = 0, alto = 0;
};

// texto con la fuente bitmap 5×7 embebida (overlay de FPS).
void dibujarTexto(uint32_t* fb, int w, int h, int x, int y, int escala,
                  const std::string& texto, uint32_t color);

// guarda el framebuffer como BMP de 24 bits (depuración visual).
bool volcarBMP(const std::string& ruta, const uint32_t* fb, int w, int h);
