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

La carga computacional real está en la iluminación: cada celda del *lightmap*
traza rayos hacia cada fuente cercana para saber si la ve o si hay roca en
medio. Ese kernel es lo único que cambia entre la versión secuencial y la
paralela — misma base de código, mismos flags de compilación, misma imagen.

## Estado

El repositorio se construye de forma incremental siguiendo `PLAN_FINAL.MD`.
El orden de trabajo no se negocia: **primero la versión secuencial completa y
medida, y solo después la paralela**. No aparece ningún `#pragma omp` hasta
que la versión secuencial esté terminada y con su línea base `T₁` medida.

- [x] Estructura del repositorio, Makefile y documentación de partida
- [x] Ventana SDL2, framebuffer y overlay de FPS
- [x] Generación procedural del mundo y render con paleta Terraria
- [x] Iluminación por ray tracing (secuencial)
- [ ] Ciclo de animación completo
- [ ] Instrumentación y medición de la línea base `T₁`
- [ ] Versión paralela con OpenMP y cálculo de speedup

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
| `--n` | 150 | **N fuentes de luz** (el parámetro del enunciado). `0` = solo luz ambiental; súbelo para llevar la máquina al límite |
| `--w` / `--h` | 1280×720 | Tamaño de ventana (mínimo 640×480) |
| `--grid` | 400x240 | Tamaño del mundo en tiles |
| `--radio` | 24 | Alcance de cada luz en tiles (costo ~cúbico: la perilla más agresiva) |
| `--muestras` | 4 | K muestras de sombra suave por fuente |
| `--escala-luz` | 2 | Píxeles por celda de lightmap {1,2,4,8} (2 = gradientes finos; 4–8 para más FPS; 1 para estresar) |
| `--seed` | aleatoria | Semilla del mundo (fija = mundo reproducible entre corridas) |
| `--duration` | ∞ | Segundos antes de salir |
| `--headless` | off | Sin ventana: aísla el cómputo del costo de SDL |
| `--vsync` | off | Sincronía con el refresco — **nunca al medir** |
| `--captura ruta.bmp` | — | Volcar un frame a BMP y salir (`--captura-t` fija el segundo) |

Programación defensiva: todos los flags se validan con `strtol/strtod`
verificando `errno` y el puntero final (nunca `atoi`); cada error tiene su
propio código de salida (2 uso, 3 valor, 5 SDL) y toda llamada SDL se verifica
con `SDL_GetError()`.

## Cómo subir (o bajar) los FPS

El costo del kernel de iluminación escala así — estas son las perillas para
calibrar:

| Perilla | Efecto en el costo | Para MÁS FPS | Para estresar la máquina |
|---|---|---|---|
| `--escala-luz` | cuadrático inverso | `4` u `8` | `1` |
| `--radio` | ~cúbico (la más agresiva) | `12`–`16` | `48`–`64` |
| `--muestras` | lineal | `1`–`2` | `8`–`16` |
| `--n` | ~lineal (más fuentes en pantalla) | `50` | `2000`+ (con `--grid` grande) |

## Documentos

- `Proyecto_1_-_Computacion_Paralela_y_Distribuida.md` — enunciado oficial del curso.
- `PLAN_FINAL.MD` — plan de diseño del equipo: modelo de datos en matrices
  planas, kernel de iluminación, análisis PCAM, mecanismos de sincronía,
  metodología de medición y reparto de trabajo.
