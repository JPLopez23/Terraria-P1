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
./terraria-forge --n 400 --w 1280 --h 720 --grid 400x240 \
                 --radio 24 --muestras 4 --escala-luz 2 --seed 42
```

| Flag | Default | Qué controla |
|---|---|---|
| `--n` | 150 | N fuentes de luz (el parámetro del enunciado). `0` = solo luz ambiental; súbelo para llevar la máquina al límite |
| `--w` / `--h` | 1280×720 | Tamaño de ventana (mínimo 640×480) |
| `--grid` | 400x240 | Tamaño del mundo en tiles |
| `--radio` | 24 | Alcance de cada luz en tiles (costo ~cúbico: la perilla más agresiva) |
| `--muestras` | 4 | K muestras de sombra suave por fuente |
| `--escala-luz` | 2 | Píxeles por celda de lightmap {1,2,4,8} (2 = gradientes finos; 4–8 para más FPS; 1 para estresar) |
| `--seed` | aleatoria | Semilla del mundo (fija = mundo reproducible entre corridas) |
| `--duration` | ∞ | Segundos antes de salir |
| `--headless` | off | Sin ventana: aísla el cómputo del costo de SDL |
| `--vsync` | off | Sincronía con el refresco — nunca al medir |
| `--captura ruta.bmp` | — | Volcar un frame a BMP y salir (`--captura-t` fija el segundo) |

Screensaver que genera un mundo 2D de tiles estilo Terraria, lo ensambla bloque
por bloque, lo ilumina con ray tracing de sombras desde cada fuente de luz,
lo recorre con la cámara (cielo → cuevas → infierno), lo desarma con caída
parabólica y vuelve a empezar con una semilla nueva. Sin input del usuario —
solo `ESC` cierra el programa.

El mundo sortea biomas por semilla (bosque, nieve, corrupción, desierto),
y genera cuevas, vetas de mineral, lava, árboles y cactus, casas de madera,
minas abandonadas con vigas, islas flotantes con ruinas y un infierno de roca
ardiente en el fondo.
