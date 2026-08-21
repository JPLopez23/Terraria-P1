#include "fuentes.hpp"
#include "ruido.hpp"

#include <algorithm>
#include <cmath>

 // hsvARgb : convierte color HSV a RGB lineal.
static void hsvARgb(float h, float s, float v, float& r, float& g, float& b) {
    float c = v * s;
    float hp = h / 60.0f;
    float x = c * (1.0f - std::fabs(std::fmod(hp, 2.0f) - 1.0f));
    float m = v - c;
    float rr = 0, gg = 0, bb = 0;
    if      (hp < 1) { rr = c; gg = x; }
    else if (hp < 2) { rr = x; gg = c; }
    else if (hp < 3) { gg = c; bb = x; }
    else if (hp < 4) { gg = x; bb = c; }
    else if (hp < 5) { rr = x; bb = c; }
    else             { rr = c; bb = x; }
    r = rr + m;  g = gg + m;  b = bb + m;
}

 // crearFuente : arma una Fuente con el color pseudoaleatorio de su clase.
static Fuente crearFuente(float x, float y, ClaseFuente clase, int indice,
                          uint32_t semilla, float radio) {
    Fuente f{};
    f.x = x;  f.y = y;
    f.radio = radio;
    f.clase = static_cast<uint8_t>(clase);
    uint32_t h = mezclarHash(semilla, 0x44444444u, static_cast<uint32_t>(indice));
    f.fase = hashAFloat(h) * 6.2831853f;

    // Colores pseudoaleatorios por fuente: requisito del enunciado:
    // matiz sorteado dentro de la gama característica de cada clase.
    float matiz, saturacion, valor;
    switch (clase) {
        case FUENTE_LAVA:      // rojo-naranja intenso, de alcance corto:
            // la lava ilumina fuerte pero local : y en el infierno hay
            matiz = 8.0f + hashAFloat(mezclarHash(h, 1u, 1u)) * 18.0f;
            saturacion = 0.95f;  valor = 1.0f;
            f.intensidadBase = 1.85f;
            f.radio = radio * 0.7f;
            break;
        case FUENTE_MINERAL:   // turquesa tenue y de brillo corto
            matiz = 165.0f + hashAFloat(mezclarHash(h, 2u, 2u)) * 30.0f;
            saturacion = 0.70f;  valor = 0.9f;
            f.intensidadBase = 0.85f;
            f.radio = radio * 0.6f;
            break;
        default:               // antorcha: naranja cálido
            matiz = 22.0f + hashAFloat(mezclarHash(h, 3u, 3u)) * 16.0f;
            saturacion = 0.80f;  valor = 1.0f;
            f.intensidadBase = 1.55f;
            break;
    }
    hsvARgb(matiz, saturacion, valor, f.r, f.g, f.b);
    f.intensidad = 0.0f;
    return f;
}

std::vector<Fuente> colocarFuentes(const Mundo& m, const Config& cfg) {
    std::vector<Fuente> fuentes;
    fuentes.reserve(cfg.n);
    const float radio = static_cast<float>(cfg.radio);

    // --- Automáticas: LAVA y MINERAL expuestos al aire ---
    std::vector<std::pair<int,int>> candLava, candMineral;
    for (int y = 1; y < m.alto - 1; ++y) {
        for (int x = 1; x < m.ancho - 1; ++x) {
            uint8_t t = m.M_tipo.en(x, y);
            if (t != LAVA && t != MINERAL) continue;
            bool expuesto = m.M_tipo.en(x, y - 1) == AIRE || m.M_tipo.en(x, y + 1) == AIRE ||
                            m.M_tipo.en(x - 1, y) == AIRE || m.M_tipo.en(x + 1, y) == AIRE;
            if (!expuesto) continue;
            (t == LAVA ? candLava : candMineral).emplace_back(x, y);
        }
    }

    int cuotaAuto = cfg.n / 3;
    auto submuestrear = [&](std::vector<std::pair<int,int>>& cand, ClaseFuente clase, int cuota) {
        if (cand.empty() || cuota <= 0) return;
        // Paso fijo sobre la lista con separación espacial mínima de 6 tiles.
        size_t paso = std::max<size_t>(1, cand.size() / cuota);
        int colocadas = 0;
        float ultimaX = -1e9f, ultimaY = -1e9f;
        for (size_t i = 0; i < cand.size() && colocadas < cuota; i += paso) {
            float fx = cand[i].first + 0.5f, fy = cand[i].second + 0.5f;
            if (std::fabs(fx - ultimaX) + std::fabs(fy - ultimaY) < 6.0f) continue;
            fuentes.push_back(crearFuente(fx, fy, clase,
                static_cast<int>(fuentes.size()), m.semilla, radio));
            ultimaX = fx;  ultimaY = fy;
            ++colocadas;
        }
    };
    submuestrear(candLava,    FUENTE_LAVA,    cuotaAuto / 2);
    submuestrear(candMineral, FUENTE_MINERAL, cuotaAuto - cuotaAuto / 2);

    // --- Antorchas sugeridas por las estructuras: casas y minas ---
    // Van primero: una casa o mina sin luz se ve abandonada de verdad.
    Matriz<uint8_t> ocupado(m.ancho, m.alto, 0);
    for (const auto& p : m.antorchasSugeridas) {
        if (static_cast<int>(fuentes.size()) >= cfg.n) break;
        if (!m.M_tipo.dentro(p.first, p.second)) continue;
        if (m.M_tipo.en(p.first, p.second) != AIRE) continue;   // otra pasada la tapó
        if (ocupado.en(p.first, p.second)) continue;
        fuentes.push_back(crearFuente(p.first + 0.5f, p.second + 0.5f, FUENTE_ANTORCHA,
            static_cast<int>(fuentes.size()), m.semilla, radio));
        for (int dy = -4; dy <= 4; ++dy)
            for (int dx = -4; dx <= 4; ++dx)
                if (ocupado.dentro(p.first + dx, p.second + dy))
                    ocupado.en(p.first + dx, p.second + dy) = 1;
    }

    // --- Antorchas: AIRE con un sólido adyacente, separación mínima ---
    struct Candidato { int x, y; uint32_t orden; };
    std::vector<Candidato> candidatos;
    candidatos.reserve(4096);
    for (int y = 1; y < m.alto - 1; ++y) {
        for (int x = 1; x < m.ancho - 1; ++x) {
            if (m.M_tipo.en(x, y) != AIRE) continue;
            bool solidoAdyacente = m.esSolido(x, y + 1) || m.esSolido(x - 1, y) ||
                                   m.esSolido(x + 1, y) || m.esSolido(x, y - 1);
            if (!solidoAdyacente) continue;
            // Las antorchas superficiales aburren: sesgo fuerte al subsuelo.
            bool subterranea = y > m.alturaSuperficie[x] + 2;
            if (!subterranea &&
                hashAFloat(mezclarHash(m.semilla, static_cast<uint32_t>(x),
                                       static_cast<uint32_t>(y))) > 0.12f)
                continue;
            candidatos.push_back({x, y,
                mezclarHash(m.semilla ^ 0xA7C4u, static_cast<uint32_t>(x),
                            static_cast<uint32_t>(y))});
        }
    }
    std::sort(candidatos.begin(), candidatos.end(),
              [](const Candidato& a, const Candidato& b) { return a.orden < b.orden; });

    // Separación mínima con rejilla de ocupación: colocar marca un cuadrado
    const int separaciones[4] = {7, 4, 2, 1};
    for (int ronda = 0; ronda < 4 && static_cast<int>(fuentes.size()) < cfg.n; ++ronda) {
        int sep = separaciones[ronda];
        for (const Candidato& c : candidatos) {
            if (static_cast<int>(fuentes.size()) >= cfg.n) break;
            if (ocupado.en(c.x, c.y)) continue;
            fuentes.push_back(crearFuente(c.x + 0.5f, c.y + 0.5f, FUENTE_ANTORCHA,
                static_cast<int>(fuentes.size()), m.semilla, radio));
            for (int dy = -sep; dy <= sep; ++dy)
                for (int dx = -sep; dx <= sep; ++dx)
                    if (ocupado.dentro(c.x + dx, c.y + dy))
                        ocupado.en(c.x + dx, c.y + dy) = 1;
        }
    }

    return fuentes;   // si size < cfg.n, el llamador reintenta con otra semilla
}

void actualizarParpadeo(std::vector<Fuente>& fuentes, double tiempo, float encendido) {
    int total = static_cast<int>(fuentes.size());
    for (int i = 0; i < total; ++i) {
        Fuente& f = fuentes[i];

        // Encendido escalonado: la fuente i prende cuando el frente
        // "encendido * total" la alcanza, con una rampa corta.
        float frente = encendido * (total + 4);
        float rampa = std::max(0.0f, std::min(1.0f, (frente - i) / 4.0f));

        // Parpadeo: dos senos desfasados + ruido temporal por hash : cada
        // antorcha titila distinto: Fuente:fase y nunca al unísono.
        float t = static_cast<float>(tiempo);
        float onda = 0.82f
                   + 0.10f * std::sin(t * 9.7f + f.fase * 3.1f)
                   + 0.08f * std::sin(t * 23.3f + f.fase * 7.7f);
        if (f.clase == FUENTE_MINERAL) onda = 0.9f + 0.1f * std::sin(t * 2.0f + f.fase); // brillo estable
        f.intensidad = f.intensidadBase * rampa * onda;
    }
}
