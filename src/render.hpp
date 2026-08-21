#pragma once

#include "mundo.hpp"
#include "fuentes.hpp"
#include "luz.hpp"
#include "camara.hpp"
#include "config.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

 // Pantalla : envoltorio RAII de SDL: ventana, renderer y textura.
class Pantalla {
public:
    // Constructor.
    explicit Pantalla(const Config& cfg);
    ~Pantalla();

    Pantalla(const Pantalla&) = delete;             // un solo dueño de los
    Pantalla& operator=(const Pantalla&) = delete;  // recursos SDL

    // procesarEventos : vacía la cola de eventos SDL.
    bool procesarEventos();

    // presentar : sube el framebuffer a la textura y lo muestra.
    void presentar(const uint32_t* fb);

    // ponerTitulo : actualiza el título de la ventana: FPS, versión....
    void ponerTitulo(const std::string& titulo);

private:
    SDL_Window*   ventana  = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture*  textura  = nullptr;
    int ancho = 0, alto = 0;
};

 // componerFrame : dibuja el mundo completo al framebuffer.
void componerFrame(const Mundo& m, const Lightmap& L, const std::vector<Fuente>& fuentes,
                   const Camara& cam, double tiempo, const Config& cfg, uint32_t* fb);

 // dibujarTexto : texto con la fuente bitmap 5×7 embebida: overlay de FPS.
void dibujarTexto(uint32_t* fb, int w, int h, int x, int y, int escala,
                  const std::string& texto, uint32_t color);

 // volcarBMP : guarda el framebuffer como BMP de 24 bits: depuración visual.
bool volcarBMP(const std::string& ruta, const uint32_t* fb, int w, int h);
