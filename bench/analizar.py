#!/usr/bin/env python3
import csv
import glob
import os
import statistics as st
from collections import defaultdict

RUTA = os.path.join("bench", "resultados", "*.csv")

luz = defaultdict(list)     # n, version, hilos -> ms de iluminación
frame = defaultdict(list)   # idem -> ms de frame completo

for ruta in glob.glob(RUTA):
    with open(ruta, newline="") as f:
        for fila in csv.DictReader(f):
            clave = (int(fila["n"]), int(fila["version"]), int(fila["threads"]))
            luz[clave].append(float(fila["ms_luz"]))
            frame[clave].append(float(fila["ms_frame"]))

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


# Línea base T1 por N: la mediana de la versión secuencial.
base = {n: st.median(v) for (n, ver, h), v in luz.items() if ver == 0}

filas = []
for clave in sorted(luz):
    n, ver, h = clave
    modo = "secuencial" if ver == 0 else "paralela"
    rl = resumen(luz[clave])
    rf = resumen(frame[clave])
    speedup = base[n] / rl["mediana"] if n in base and rl["mediana"] > 0 else float("nan")
    eficiencia = speedup / h if h > 0 else float("nan")
    if h > 1 and speedup == speedup and speedup > 0:
        f_amdahl = (1.0 / speedup - 1.0 / h) / (1.0 - 1.0 / h)
    else:
        f_amdahl = float("nan")
    fps = 1000.0 / rf["mediana"] if rf["mediana"] > 0 else 0.0
    filas.append({
        "n": n, "modo": modo, "hilos": h,
        "luz_mediana_ms": round(rl["mediana"], 3),
        "luz_media_ms": round(rl["media"], 3),
        "luz_desv_ms": round(rl["desv"], 3),
        "luz_p99_ms": round(rl["p99"], 3),
        "frame_mediana_ms": round(rf["mediana"], 3),
        "fps_mediana": round(fps, 1),
        "speedup": round(speedup, 3),
        "eficiencia": round(eficiencia, 3),
        "f_serial_amdahl": round(f_amdahl, 4) if f_amdahl == f_amdahl else "",
        "muestras": rl["muestras"],
    })

# tabla en consola 
cab = ["n", "modo", "hilos", "luz_mediana_ms", "luz_p99_ms",
       "fps_mediana", "speedup", "eficiencia", "f_serial_amdahl", "muestras"]
anchos = {c: max(len(c), 11) for c in cab}
print(" ".join(c.rjust(anchos[c]) for c in cab))
for fila in filas:
    print(" ".join(str(fila[c]).rjust(anchos[c]) for c in cab))

# resumen CSV 
with open(os.path.join("bench", "resumen.csv"), "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=list(filas[0].keys()))
    w.writeheader()
    w.writerows(filas)
print("\nEscrito bench/resumen.csv")

# gráficas: opcionales: requieren matplotlib 
try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, (ejeS, ejeE) = plt.subplots(1, 2, figsize=(12, 5))
    maxH = 1
    for n in sorted({f["n"] for f in filas}):
        pts = sorted((f["hilos"], f["speedup"], f["eficiencia"])
                     for f in filas if f["n"] == n and f["modo"] == "paralela")
        if not pts:
            continue
        hs = [p[0] for p in pts]
        maxH = max(maxH, max(hs))
        ejeS.plot(hs, [p[1] for p in pts], marker="o", label=f"N={n}")
        ejeE.plot(hs, [p[2] for p in pts], marker="o", label=f"N={n}")
    ejeS.plot([1, maxH], [1, maxH], "k--", alpha=0.4, label="ideal")
    ejeS.set_title("Speedup vs hilos (paralela / secuencial)")
    ejeS.set_xlabel("hilos"); ejeS.set_ylabel("speedup"); ejeS.legend(); ejeS.grid(True)
    ejeE.axhline(1.0, color="k", ls="--", alpha=0.4, label="ideal")
    ejeE.set_title("Eficiencia vs hilos")
    ejeE.set_xlabel("hilos"); ejeE.set_ylabel("eficiencia"); ejeE.legend(); ejeE.grid(True)
    fig.tight_layout()
    salida = os.path.join("bench", "speedup.png")
    fig.savefig(salida, dpi=120)
    print(f"Grafica: {salida}")
except ImportError:
    print("(matplotlib no esta instalado: solo tabla y resumen.csv, sin graficas)")
