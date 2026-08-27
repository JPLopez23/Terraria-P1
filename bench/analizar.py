#!/usr/bin/env python3
import csv
import glob
import os
import statistics as st
from collections import defaultdict

RUTA = os.path.join("bench", "resultados", "*.csv")

luz = defaultdict(list)     # n -> lista de ms de iluminacion
frame = defaultdict(list)   # n -> lista de ms de frame completo

for ruta in glob.glob(RUTA):
    with open(ruta, newline="") as f:
        for fila in csv.DictReader(f):
            n = int(fila["n"])
            luz[n].append(float(fila["ms_luz"]))
            frame[n].append(float(fila["ms_frame"]))

if not luz:
    raise SystemExit("No hay CSV en bench/resultados/. Corre antes bench/correr.sh")


def resumen(valores):
    v = sorted(valores)
    return {
        "mediana": st.median(v),
        "media": st.mean(v),
        "desv": st.pstdev(v) if len(v) > 1 else 0.0,
        "p99": v[min(len(v) - 1, int(len(v) * 0.99))],
        "muestras": len(v),
    }


filas = []
for n in sorted(luz):
    rl = resumen(luz[n])
    rf = resumen(frame[n])
    fps = 1000.0 / rf["mediana"] if rf["mediana"] > 0 else 0.0
    filas.append({
        "n": n,
        "luz_mediana_ms": round(rl["mediana"], 3),
        "luz_media_ms": round(rl["media"], 3),
        "luz_desv_ms": round(rl["desv"], 3),
        "luz_p99_ms": round(rl["p99"], 3),
        "frame_mediana_ms": round(rf["mediana"], 3),
        "fps_mediana": round(fps, 1),
        "muestras": rl["muestras"],
    })

cab = ["n", "luz_mediana_ms", "luz_p99_ms", "fps_mediana", "muestras"]
anchos = {c: max(len(c), 11) for c in cab}
print(" ".join(c.rjust(anchos[c]) for c in cab))
for fila in filas:
    print(" ".join(str(fila[c]).rjust(anchos[c]) for c in cab))

with open(os.path.join("bench", "resumen.csv"), "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=list(filas[0].keys()))
    w.writeheader()
    w.writerows(filas)
print("\nEscrito bench/resumen.csv")
