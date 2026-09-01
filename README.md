# Terraria Forge

Screensaver que genera un mundo 2D de tiles estilo Terraria, lo ensambla bloque
por bloque, lo ilumina con ray tracing de sombras desde cada fuente de luz,
lo recorre con la cámara (cielo → cuevas → infierno), lo desarma con caída
parabólica y vuelve a empezar con una semilla nueva. Sin input del usuario, solo ESC cierra el programa.

El mundo sortea biomas por semilla (bosque, nieve, corrupción, desierto),
y genera cuevas, vetas de mineral, lava, árboles y cactus, casas de madera,
minas abandonadas con vigas, islas flotantes con ruinas y un infierno de roca
ardiente en el fondo.

La carga computacional real está en la iluminación: cada celda del lightmap
traza rayos hacia cada fuente cercana para saber si la ve o si hay roca en
medio. Ese kernel es lo único que cambia entre la versión secuencial y la
paralela.

## Secuencial o paralelo

```bash
./terraria-forge --version 0                # SECUENCIAL (1 hilo, sin OpenMP)
./terraria-forge --version 1 --threads 8    # PARALELO (OpenMP parallel for)
```

| Versión | Qué hace el kernel de iluminación |
|---|---|
| **0 — secuencial** | Recorre las filas del lightmap una por una, en un solo hilo. Sin ningún #pragma. Es la línea base T₁ de todos los speedups |
| **1 — paralela** | El mismo lazo, repartido entre hilos con #pragma omp parallel for sobre las filas del lightmap |


## Cómo subir (o bajar) los FPS


| Perilla | Efecto en el costo | Para MÁS FPS | Para estresar la máquina |
|---|---|---|---|
| `--version` + `--threads` | el paralelismo mismo | `--version 1 --threads <núcleos>` | `--version 0` |
| `--escala-luz` | cuadrático inverso | `4` u `8` | `1` |
| `--radio` | ~cúbico (la más agresiva) | `12`–`16` | `48`–`64` |
| `--muestras` | lineal | `1`–`2` | `8`–`16` |
| `--n` | ~lineal (más fuentes en pantalla) | `50` | `2000`+ (con `--grid` grande) |


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
./terraria-forge --n 400 --version 1 --threads 8 --w 1280 --h 720 \
                 --grid 400x240 --radio 24 --muestras 4 --escala-luz 2 \
                 --seed 42 --duration 30 --csv resultados.csv --headless
```

| Flag | Default | Qué controla |
|---|---|---|
| `--n` | 150 | **N fuentes de luz** (el parámetro del enunciado). `0` = solo luz ambiental; súbelo (hasta 1,000,000) para llevar la máquina al límite. Si el mundo por defecto no aloja tantas, agranda `--grid` |
| `--version` | 1 | **0 = secuencial · 1 = paralela** (`parallel for`) |
| `--threads` | núcleos | Hilos OpenMP (solo aplica a la versión paralela) |
| `--w` / `--h` | 1280×720 | Tamaño de ventana (mínimo 640×480) |
| `--grid` | 400x240 | Tamaño del mundo en tiles |
| `--radio` | 24 | Alcance de cada luz en tiles (costo ~cúbico: la perilla más agresiva) |
| `--muestras` | 4 | K muestras de sombra suave por fuente |
| `--escala-luz` | 2 | Píxeles por celda de lightmap {1,2,4,8} (2 = gradientes finos; 4–8 para más FPS; 1 para estresar) |
| `--seed` | aleatoria | Semilla del mundo (fija = mundo reproducible entre corridas) |
| `--duration` | ∞ | Segundos antes de salir |
| `--csv` | — | Mediciones por frame a CSV (se valida escribible antes de medir) |
| `--headless` | off | Sin ventana: aísla el cómputo del costo de SDL |
| `--vsync` | off | Sincronía con el refresco — **nunca al medir** |
| `--medicion` | off | Carga fija reproducible (benchmark): mundo encendido, cámara fija |
| `--test-luz` | off | Un frame determinista: imprime el checksum del lightmap y sale (prueba de que ambas versiones dan la misma imagen) |
| `--captura ruta.bmp` | — | Volcar un frame a BMP y salir (`--captura-t` fija el segundo) |

### Resultados 

12 procesadores lógicos · `--seed 42 --escala-luz 2 --medicion` · 3 corridas por
configuración, mediana del tiempo de iluminación por frame:

| N | Modo | Hilos | Luz (ms) | FPS | Speedup | Eficiencia |
|---|---|---|---|---|---|---|
| 150 | secuencial | 1 | 147 | 6.1 | 1.00 | — |
| 150 | paralela | 2 | 79 | 11.1 | 1.86 | 0.93 |
| 150 | paralela | 4 | 40 | 21.7 | 3.68 | 0.92 |
| 150 | paralela | 8 | 23 | 37.0 | 6.39 | 0.80 |
| 150 | paralela | 12 | 17 | 50.0 | 8.65 | 0.72 |
| 400 | secuencial | 1 | 346 | 2.8 | 1.00 | — |
| 400 | paralela | 12 | 41 | 22.7 | 8.44 | 0.70 |

La fracción serial estimada por la ley de Amdahl es ≈ 0.04 (4%), consistente
con el costo de la composición y la presentación del frame.
