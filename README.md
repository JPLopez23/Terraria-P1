# Terraria Forge

Screensaver de un mundo 2D estilo Terraria: genera el mundo por semilla, lo ensambla
bloque por bloque, lo ilumina con ray tracing de sombras desde cada fuente de luz,
lo recorre con la cámara (cielo → cuevas → infierno) y lo desarma con caída parabólica,
en bucle infinito. Solo `ESC` cierra. El costo real está en la iluminación: el mismo
kernel corre secuencial (`--version 0`) o en paralelo con OpenMP (`--version 1`).

## Compilar

```bash
# Linux / WSL
sudo apt install build-essential libsdl2-dev
make

# Windows (MinGW-W64 + SDL2 en third_party/)
mingw32-make

#Correr
./terraria-forge                                   # normal, ventana
./terraria-forge --version 0                        # secuencial
./terraria-forge --version 1 --threads 8            # paralelo
./terraria-forge --n 400 --seed 42 --grid 400x240   # mundo fijo, más carga

#Medir
./terraria-forge --headless --medicion --seed 42 --duration 5 --version 0
./terraria-forge --headless --medicion --seed 42 --duration 5 --version 1 --threads 8
./terraria-forge --test-luz --seed 42 --version 0   
./terraria-forge --test-luz --seed 42 --version 1 --threads 8

#Flags
--n fuentes de luz (150) · --w/--h ventana (1280×720) · --grid mundo (400x240) ·
--radio alcance de luz (24) · --muestras sombra suave (4) · --escala-luz {1,2,4,8} (2) ·
--seed semilla · --duration segundos · --headless sin ventana · --vsync off

