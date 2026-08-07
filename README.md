# Terraria Forge

**Proyecto #1 — Computación Paralela y Distribuida · Universidad del Valle de Guatemala · Semestre 2, 2026**

Screensaver que genera un mundo 2D de tiles estilo Terraria, lo ensambla bloque
por bloque, lo ilumina con **ray tracing de sombras** desde cada fuente de luz,
lo recorre con la cámara, lo desarma con caída parabólica y vuelve a empezar
con una semilla nueva. Sin input del usuario — solo `ESC` cierra el programa.

La carga computacional real está en la iluminación: cada celda del *lightmap*
traza rayos hacia cada fuente cercana para saber si la ve o si hay roca en
medio. Ese kernel es el que después se reparte entre hilos con OpenMP.

## Compilación

Requiere un compilador C++17 con OpenMP.

### Linux / WSL
```bash
make
./terraria-forge
```

### Windows (MinGW-W64)
```bash
mingw32-make
./terraria-forge.exe
```

## Documentos

- `Proyecto_1_-_Computacion_Paralela_y_Distribuida.md` — enunciado oficial del curso.
