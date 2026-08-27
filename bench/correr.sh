#!/usr/bin/env bash
set -u

MODO="${1:-rapido}"

BIN=./terraria-forge
[ -f ./terraria-forge.exe ] && BIN=./terraria-forge.exe
if [ ! -f "$BIN" ]; then
    echo "No existe el ejecutable. Compila primero (make / mingw32-make)." >&2
    exit 1
fi

SALIDA=bench/resultados
mkdir -p "$SALIDA"
SEED="${SEED:-42}"

if [ "$MODO" = "completo" ]; then
    NS="${NS:-50 100 200 400 800 1600}"
    CORRIDAS="${CORRIDAS:-30}"
    DUR="${DUR:-10}"
else
    NS="${NS:-100 400 1600}"
    CORRIDAS="${CORRIDAS:-3}"
    DUR="${DUR:-6}"
fi

total=0
for n in $NS; do
  for r in $(seq 1 "$CORRIDAS"); do
    csv="$SALIDA/n${n}_secuencial_r${r}.csv"
    if [ -f "$csv" ]; then continue; fi   # reanudable: no repite lo hecho
    echo "[bench] N=$n SECUENCIAL corrida=$r/$CORRIDAS"
    "$BIN" --headless --medicion --seed "$SEED" --n "$n" \
           --duration "$DUR" --csv "$csv" > /dev/null
    total=$((total + 1))
  done
done

echo "Corridas nuevas: $total. Analiza con: python bench/analizar.py"
