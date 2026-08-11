# Terraria Forge

**Proyecto #1 — Computación Paralela y Distribuida · Universidad del Valle de Guatemala · Semestre 2, 2026**

Screensaver que genera un mundo 2D de tiles estilo Terraria, lo ensambla bloque
por bloque, lo ilumina con **ray tracing de sombras** desde cada fuente de luz,
lo recorre con la cámara (cielo → cuevas → infierno), lo desarma con caída
parabólica y vuelve a empezar con una semilla nueva. Sin input del usuario —
solo `ESC` cierra el programa.

El mundo sortea **biomas** por semilla (bosque, nieve, corrupción, desierto),
y genera cuevas, vetas de mineral, lava, árboles y cactus, casas de madera,
minas abandonadas con vigas, islas flotantes con ruinas y un infierno de roca
ardiente en el fondo.

## Compilación

Requiere un compilador C++17 con OpenMP y SDL2.

### Linux / WSL
```bash
sudo apt install build-essential libsdl2-dev   # (una vez)
make
./terraria-forge
```

### Windows (MinGW-W64)
```bash
# 1) Descargar SDL2 (una vez): https://github.com/libsdl-org/SDL/releases
#    SDL2-devel-2.30.x-mingw.tar.gz → extraer y renombrar a third_party/SDL2
#    (debe existir third_party/SDL2/x86_64-w64-mingw32/)
mingw32-make
./terraria-forge.exe
```
El Makefile copia `SDL2.dll` junto al ejecutable automáticamente.

### macOS
```bash
brew install sdl2 libomp
make
```


## Uso

```bash
./terraria-forge --w 1280 --h 720 --grid 400x240 --seed 42
```

| Flag | Default | Qué controla |
|---|---|---|
| `--w` / `--h` | 1280×720 | Tamaño de ventana (mínimo 640×480) |
| `--grid` | 400x240 | Tamaño del mundo en tiles |
| `--seed` | aleatoria | Semilla del mundo (fija = mundo reproducible entre corridas) |
| `--duration` | ∞ | Segundos antes de salir |
| `--headless` | off | Sin ventana: aísla el cómputo del costo de SDL |
| `--vsync` | off | Sincronía con el refresco — **nunca al medir** |
| `--captura ruta.bmp` | — | Volcar un frame a BMP y salir (`--captura-t` fija el segundo) |

Programación defensiva: todos los flags se validan con `strtol/strtod`
verificando `errno` y el puntero final (nunca `atoi`); cada error tiene su
propio código de salida (2 uso, 3 valor, 5 SDL) y toda llamada SDL se verifica
con `SDL_GetError()`.
