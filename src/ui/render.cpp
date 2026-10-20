// Renderizado 2D/3D con gint (fx-CG50, VRAM 396x224). Solo usa dclear/drect/dpixel/dtext/dprint/dupdate.
#include "render.h"
#include "../core/content.h"
#include <gint/display.h>
#include <string.h>

namespace od {

static const uint16_t BG     = C_RGB(1, 1, 3);
static const uint16_t BOARDC = C_RGB(2, 3, 5);
static const uint16_t DOT    = C_RGB(4, 6, 9);
static const uint16_t TXT    = C_RGB(27, 30, 31);
static const uint16_t DIM    = C_RGB(12, 15, 20);
static const uint16_t GOOD   = C_RGB(12, 31, 16);
static const uint16_t GOLD   = C_RGB(31, 27, 3);
static const uint16_t RED    = C_RGB(31, 6, 8);
static const uint16_t SIDE   = C_RGB(2, 3, 7);
static const uint16_t PURPLE = C_RGB(20, 12, 31);
static const uint16_t CYAN   = C_RGB(6, 26, 31);

static const int8_t WAVE[16] = {0, 1, 2, 3, 3, 2, 1, 0, 0, -1, -2, -3, -3, -2, -1, 0};
static const int8_t DXS[4] = {0, 1, 0, -1};
static const int8_t DYS[4] = {-1, 0, 1, 0};

static int tw(const char *s) { int n = 0; while (*s++) n += 6; return n; }
static void ctext(int cx, int y, uint16_t c, const char *s) { dtext(cx - tw(s) / 2, y, c, s); }
static unsigned hsh(unsigned a) { a ^= a << 13; a ^= a >> 17; a ^= a << 5; return a * 2654435761u; }

uint16_t hueColor(int h, int v) {
    h = ((h % 192) + 192) % 192;
    int seg = h / 32, f = h % 32, r, g, b;
    int up = f * v / 31, down = (31 - f) * v / 31;
    switch (seg) {
    case 0: r = v; g = up; b = 0; break;
    case 1: r = down; g = v; b = 0; break;
    case 2: r = 0; g = v; b = up; break;
    case 3: r = 0; g = down; b = v; break;
    case 4: r = up; g = 0; b = v; break;
    default: r = v; g = 0; b = down; break;
    }
    return C_RGB(r, g, b);
}
// Oscurece un color: k/32 (0 = negro).
static uint16_t shade(uint16_t c, int k) {
    int r = (c >> 11) & 31, g = (c >> 6) & 31, b = c & 31;
    return C_RGB(r * k / 32, g * k / 32, b * k / 32);
}

// ---------------------------------------------------------------- Efectos
void Fx::clear() {
    for (auto &q : p) q.life = 0;
    for (auto &r : rg) r.life = 0;
    for (auto &t : ft) t.life = 0;
    flash = 0; shake = 0; ox = 0; oy = 0;
}
void Fx::emit(int px, int py, int vx, int vy, int life, uint16_t color, uint8_t kind) {
    for (auto &q : p) if (!q.life) {
        q.x = (int16_t)(px * 16); q.y = (int16_t)(py * 16); q.vx = (int16_t)vx; q.vy = (int16_t)vy;
        q.life = (uint8_t)life; q.color = color; q.kind = kind; return;
    }
}
void Fx::burst(int px, int py, uint16_t color, int count, int spread, uint8_t kind) {
    for (int i = 0; i < count; i++)
        emit(px, py, (int)rng.range((uint32_t)(2 * spread + 1)) - spread, (int)rng.range((uint32_t)(2 * spread + 1)) - spread,
             10 + (int)rng.range(12), color, kind);
}
void Fx::ring(int px, int py, uint16_t color) {
    for (auto &r : rg) if (!r.life) { r.x = (int16_t)px; r.y = (int16_t)py; r.r = 2; r.life = 12; r.color = color; return; }
}
void Fx::text(int px, int py, uint16_t color, const char *s) {
    for (auto &t : ft) if (!t.life) {
        t.x = (int16_t)px; t.y = (int16_t)py; t.life = 26; t.color = color;
        int i = 0; while (s[i] && i < 7) { t.s[i] = s[i]; i++; }
        t.s[i] = 0; return;
    }
}
void Fx::update() {
    for (auto &q : p) if (q.life) { q.x += q.vx; q.y += q.vy; q.life--; }
    for (auto &r : rg) if (r.life) { r.r += 3; r.life--; }
    for (auto &t : ft) if (t.life) { t.y -= 1; t.life--; }
    if (shake > 0) {
        int s = shake > 8 ? 3 : (shake > 4 ? 2 : 1);
        ox = (int)rng.range((uint32_t)(2 * s + 1)) - s;
        oy = (int)rng.range((uint32_t)(2 * s + 1)) - s;
        shake--;
    } else { ox = 0; oy = 0; }
    if (flash > 0) flash--;
}
static void drawParticle(const Particle &q) {
    int px = q.x / 16, py = q.y / 16;
    switch (q.kind) {
    case 1: dpixel(px, py, q.color); dpixel(px + 2, py, q.color); drect(px, py + 1, px + 2, py + 1, q.color); dpixel(px + 1, py + 2, q.color); break;
    case 2: dpixel(px + 1, py + 1, C_WHITE); dpixel(px + 1, py, q.color); dpixel(px + 1, py + 2, q.color); dpixel(px, py + 1, q.color); dpixel(px + 2, py + 1, q.color); break;
    case 3: dpixel(px + 1, py, q.color); dpixel(px, py + 1, q.color); dpixel(px + 2, py + 1, q.color); dpixel(px + 1, py + 2, q.color); break;
    default: drect(px, py, px + 1, py + 1, q.color); break;
    }
}
static void ringDraw(int cx, int cy, int r, uint16_t c) {
    int x = r, y = 0, e = 1 - r;
    while (x >= y) {
        dpixel(cx + x, cy + y, c); dpixel(cx - x, cy + y, c); dpixel(cx + x, cy - y, c); dpixel(cx - x, cy - y, c);
        dpixel(cx + y, cy + x, c); dpixel(cx - y, cy + x, c); dpixel(cx + y, cy - x, c); dpixel(cx - y, cy - x, c);
        y++;
        if (e < 0) e += 2 * y + 1; else { x--; e += 2 * (y - x) + 1; }
    }
}

// Aspecto de cada estela (compartido por el juego y la tienda).
void trailParams(int trail, int frame, uint16_t &color, uint8_t &kind) {
    switch (trail) {
    case 1: color = GOLD; kind = 0; break;
    case 2: color = CYAN; kind = 3; break;
    case 3: color = hueColor(frame * 9, 31); kind = 0; break;
    case 4: color = ((frame / 2) & 1) ? C_RGB(31, 22, 3) : C_RGB(31, 8, 2); kind = 0; break;
    case 5: color = C_RGB(31, 9, 18); kind = 1; break;
    case 6: color = ((frame / 2) & 1) ? C_RGB(31, 31, 10) : C_RGB(8, 20, 31); kind = 2; break;
    case 7: color = hueColor(frame * 5 + 40, 31); kind = 2; break;
    default: color = DIM; kind = 0; break;
    }
}

// ---------------------------------------------------------------- Serpiente
static uint16_t glitchCol(int i, int frame) {
    static const uint16_t c[5] = {C_RGB(31, 2, 20), C_RGB(2, 31, 22), C_RGB(31, 31, 4), C_RGB(8, 4, 31), C_RGB(31, 31, 31)};
    return c[(hsh((unsigned)(i * 31 + frame / 2)) >> 13) % 5];
}

static uint16_t skinBody(int skin, int i, int n, int frame) {
    int t = n > 1 ? (i * 31) / (n - 1) : 31;
    int alt = (i & 1) ? 3 : 0;
    switch (skin) {
    case 1: { int ph = (i * 4 + frame * 2) & 63, a = ph < 32 ? ph : 63 - ph;
              return C_RGB(6 + a * 18 / 31 + alt, 10 + (31 - a) * 18 / 31, 26 + alt / 2); }
    case 2: { int fl = ((frame / 2 + i) % 3) * 2; return C_RGB(18 + t * 13 / 31, 2 + t * 18 / 31 + fl + alt, t > 26 ? 6 : 0); }
    case 3: return C_RGB(10 + t * 12 / 31 + alt, 20 + t * 10 / 31, 31);
    case 4: { int pos = (frame * 2) % (n + 8) - 4, d = i - pos; if (d < 0) d = -d;
              if (d <= 1) return C_RGB(31, 31, 20);
              return C_RGB(24 + alt / 2, 16 + t / 4, 2); }
    case 5: { if ((i * 7 + frame / 2) % 11 == 0) return C_WHITE;
              return C_RGB(8 + t / 3 + alt, 2, 16 + t / 2); }
    case 6: return hueColor(i * 10 + frame * 4, 31);
    case 7: return glitchCol(i, frame);
    case 8: return C_RGB(21 + alt, 26 + alt / 2, 31);
    case 9: return C_RGB(13 + alt, 15 + alt, 20);
    case 10: return C_RGB(3, 3, 5);
    case 11: return (i & 1) ? C_RGB(31, 29, 4) : C_RGB(6, 14, 31);
    case 12: return C_RGB(22 + alt, 4, 4 + t / 10);
    case 13: return i == 0 ? C_WHITE : ((i % 3 == 0) ? C_RGB(15, 6, 1) : C_RGB(30, 16, 3));
    case 14: return C_RGB(13 + alt, 5, 21 + alt);
    default: return C_RGB(2 + t / 9 + alt / 2, 11 + t * 13 / 31 + alt, 5 + t / 8);
    }
}
static uint16_t skinHead(int skin, int frame) {
    static const uint16_t H[SKIN_COUNT] = {
        C_RGB(18, 31, 20), C_RGB(24, 31, 31), C_RGB(31, 26, 8), C_RGB(28, 31, 31), C_RGB(31, 30, 14),
        C_RGB(28, 16, 31), 0, 0, C_RGB(26, 29, 31), C_RGB(18, 20, 24), C_RGB(28, 28, 26),
        C_RGB(31, 31, 12), C_RGB(28, 8, 6), C_RGB(31, 19, 6), C_RGB(20, 8, 28)};
    if (skin == 6) return hueColor(frame * 5, 31);
    if (skin == 7) return glitchCol(99, frame);
    return H[skin];
}

static void sq(int x, int y, int inset, uint16_t c) {
    int a = inset, b = 9 - inset;
    drect(x + a + 1, y + a, x + b - 1, y + b, c);
    drect(x + a, y + a + 1, x + b, y + b - 1, c);
}
static void sqDither(int x, int y, int inset, uint16_t c, int phase) {
    int a = inset, b = 9 - inset;
    for (int k = a; k <= b; k++) {
        if ((k + phase) & 1) continue;
        if (k == a || k == b) drect(x + a + 1, y + k, x + b - 1, y + k, c);
        else drect(x + a, y + k, x + b, y + k, c);
    }
}

static void drawHead(int x, int y, Dir d, int skin, uint16_t hc, int frame, Mood mood, int pdx, int pdy, bool mouth) {
    if (skin == 8) sqDither(x, y, 0, hc, frame / 5); else sq(x, y, 0, hc);
    if (skin == 9) { drect(x + 1, y + 1, x + 8, y + 8, hc); }
    static const int EX[4][2] = {{1, 6}, {5, 5}, {1, 6}, {2, 2}};
    static const int EY[4][2] = {{2, 2}, {1, 6}, {5, 5}, {1, 6}};
    bool blink = (frame % 110) < 4;
    if (skin == 9 && mood != MOOD_DEAD) {                // ROBOT: visor con luz que escanea
        drect(x + 1, y + 3, x + 8, y + 6, C_BLACK);
        int sc = (frame / 3) % 12; if (sc > 6) sc = 12 - sc;
        drect(x + 1 + sc, y + 4, x + 2 + sc, y + 5, mood == MOOD_FRENZY ? C_RGB(31, 31, 6) : RED);
        dpixel(x + 5, y - 1, C_RGB(20, 22, 26)); dpixel(x + 5, y - 2, ((frame / 6) & 1) ? RED : C_RGB(8, 4, 4));
    } else for (int e = 0; e < 2; e++) {
        int ex = x + EX[d][e], ey = y + EY[d][e];
        if (mood == MOOD_DEAD) {
            uint16_t k = C_BLACK;
            dpixel(ex, ey, k); dpixel(ex + 2, ey, k); dpixel(ex + 1, ey + 1, k); dpixel(ex, ey + 2, k); dpixel(ex + 2, ey + 2, k);
        } else if (mood == MOOD_HAPPY) {
            dpixel(ex, ey + 2, C_BLACK); dpixel(ex + 1, ey + 1, C_BLACK); dpixel(ex + 2, ey + 2, C_BLACK);
        } else if (blink) {
            drect(ex, ey + 1, ex + 2, ey + 1, C_BLACK);
        } else if (skin == 10 || skin == 8) {            // calavera / fantasma: cuencas huecas
            drect(ex, ey, ex + 2, ey + 2, C_BLACK);
            dpixel(ex + 1 + pdx, ey + 1 + pdy, mood == MOOD_FRENZY ? C_RGB(31, 31, 6) : (skin == 10 ? RED : C_WHITE));
        } else if (skin == 13) {                         // gato: ojos amarillos con pupila vertical
            drect(ex, ey, ex + 2, ey + 2, C_RGB(31, 28, 6));
            drect(ex + 1, ey, ex + 1, ey + 2, C_BLACK);
        } else if (mood == MOOD_FRENZY) {
            drect(ex, ey, ex + 2, ey + 2, C_RGB(31, 4, 4));
            dpixel(ex + 1 + pdx, ey + 1 + pdy, C_WHITE);
            drect(ex, ey - 1, ex + 2, ey - 1, C_BLACK);
        } else {
            drect(ex, ey, ex + 2, ey + 2, C_WHITE);
            dpixel(ex + 1 + pdx, ey + 1 + pdy, C_BLACK);
        }
    }
    // Accesorios propios de cada skin
    if (skin == 12) {                                    // dragon: cuernos
        dpixel(x + 1, y - 1, C_RGB(31, 28, 20)); dpixel(x + 1, y - 2, C_RGB(31, 28, 20));
        dpixel(x + 8, y - 1, C_RGB(31, 28, 20)); dpixel(x + 8, y - 2, C_RGB(31, 28, 20));
    } else if (skin == 13) {                             // gato: orejas y bigotes
        dpixel(x, y - 1, hc); dpixel(x + 1, y - 1, hc); dpixel(x + 1, y - 2, hc); dpixel(x + 1, y, C_RGB(31, 14, 18));
        dpixel(x + 9, y - 1, hc); dpixel(x + 8, y - 1, hc); dpixel(x + 8, y - 2, hc); dpixel(x + 8, y, C_RGB(31, 14, 18));
        dpixel(x - 2, y + 4, C_WHITE); dpixel(x - 2, y + 6, C_WHITE); dpixel(x + 11, y + 4, C_WHITE); dpixel(x + 11, y + 6, C_WHITE);
    } else if (skin == 14 && mood != MOOD_DEAD) {        // argos: tercer ojo
        drect(x + 4, y + 4, x + 5, y + 5, C_WHITE); dpixel(x + 4 + (pdx > 0), y + 4 + (pdy > 0), C_BLACK);
    } else if (skin == 11) {                             // electrica: chispas alrededor
        unsigned r = hsh((unsigned)(frame / 2));
        if (r & 1) dpixel(x - 1 + (int)((r >> 3) % 12), y - 1 + (int)((r >> 7) % 2) * 11, C_WHITE);
    } else if (skin == 10) {                             // calavera: dientes
        switch (d) {
        case RIGHT: for (int k = 2; k < 8; k += 2) dpixel(x + 9, y + k, C_BLACK); break;
        case LEFT:  for (int k = 2; k < 8; k += 2) dpixel(x, y + k, C_BLACK); break;
        case UP:    for (int k = 2; k < 8; k += 2) dpixel(x + k, y, C_BLACK); break;
        default:    for (int k = 2; k < 8; k += 2) dpixel(x + k, y + 9, C_BLACK); break;
        }
    }
    uint16_t MOUTH = C_RGB(14, 2, 4), TONG = C_RGB(31, 5, 9);
    if (mouth || mood == MOOD_HAPPY) {
        switch (d) {
        case RIGHT: drect(x + 8, y + 3, x + 9, y + 6, MOUTH); break;
        case LEFT:  drect(x, y + 3, x + 1, y + 6, MOUTH); break;
        case UP:    drect(x + 3, y, x + 6, y + 1, MOUTH); break;
        default:    drect(x + 3, y + 8, x + 6, y + 9, MOUTH); break;
        }
        if (skin == 12 && mouth) {                        // dragon: aliento de fuego
            int fl = (frame / 2) & 1;
            uint16_t f1 = C_RGB(31, 20, 3), f2 = C_RGB(31, 8, 2);
            switch (d) {
            case RIGHT: drect(x + 10, y + 3, x + 13 + fl, y + 6, f2); drect(x + 10, y + 4, x + 12, y + 5, f1); break;
            case LEFT:  drect(x - 4 - fl, y + 3, x - 1, y + 6, f2); drect(x - 3, y + 4, x - 1, y + 5, f1); break;
            case UP:    drect(x + 3, y - 4 - fl, x + 6, y - 1, f2); drect(x + 4, y - 3, x + 5, y - 1, f1); break;
            default:    drect(x + 3, y + 10, x + 6, y + 13 + fl, f2); drect(x + 4, y + 10, x + 5, y + 12, f1); break;
            }
        }
    } else if (mood != MOOD_DEAD) {
        int ph = frame % 50;
        if (ph < 14) {
            int len = (ph < 7 ? ph / 2 : (14 - ph) / 2) + 1;
            switch (d) {
            case RIGHT: drect(x + 10, y + 4, x + 9 + len, y + 5, TONG);
                        if (len >= 3) { dpixel(x + 9 + len, y + 3, TONG); dpixel(x + 9 + len, y + 6, TONG); } break;
            case LEFT:  drect(x - len, y + 4, x - 1, y + 5, TONG);
                        if (len >= 3) { dpixel(x - len, y + 3, TONG); dpixel(x - len, y + 6, TONG); } break;
            case UP:    drect(x + 4, y - len, x + 5, y - 1, TONG);
                        if (len >= 3) { dpixel(x + 3, y - len, TONG); dpixel(x + 6, y - len, TONG); } break;
            default:    drect(x + 4, y + 10, x + 5, y + 9 + len, TONG);
                        if (len >= 3) { dpixel(x + 3, y + 9 + len, TONG); dpixel(x + 6, y + 9 + len, TONG); } break;
            }
        }
    }
}

static void drawSeg(int skin, int i, int n, int x, int y, int inset, uint16_t col, int frame, int pdx, int pdy, const Cell *c, bool rainbow) {
    if (rainbow) { sq(x, y, inset, col); return; }
    switch (skin) {
    case 1:  sq(x, y, 0, shade(col, 10)); sq(x, y, 1, col); break;                      // neon: halo
    case 8: { int wob = WAVE[(i * 2 + frame / 2) & 15] / 3; sqDither(x + wob, y, inset, col, frame / 5 + i); break; }
    case 9:  drect(x + 1, y + 1, x + 8, y + 8, col); drect(x + 1, y + 5, x + 8, y + 5, shade(col, 18));
             drect(x + 1, y + 1, x + 8, y + 1, C_RGB(24, 26, 30));
             if (i % 3 == 0) { dpixel(x + 4, y + 3, ((frame / 8 + i) & 1) ? RED : GOOD); }   // robot: LEDs
             break;
    case 10: sq(x, y, 1, col); drect(x + 3, y + 1, x + 6, y + 8, C_RGB(28, 28, 26)); drect(x + 1, y + 4, x + 8, y + 5, C_RGB(26, 26, 24)); break;   // huesos
    case 11: sq(x, y, inset, col);                                                       // electrica: arcos
             if ((frame + i * 3) % 7 < 2 && i < n - 1) {
                 unsigned r = hsh((unsigned)(frame * 7 + i));
                 int dx = c[i + 1].x - c[i].x, dy = c[i + 1].y - c[i].y;
                 for (int k = 0; k < 3; k++) dpixel(x + 5 + dx * (4 + k * 2) + (int)((r >> (k * 3)) % 3) - 1, y + 5 + dy * (4 + k * 2) + (int)((r >> (k * 3 + 1)) % 3) - 1, C_WHITE);
             } break;
    case 12: sq(x, y, inset, col); drect(x + 3, y + 3, x + 6, y + 6, C_RGB(10, 24, 6));  // dragon: vientre y pinchos
             if ((i & 1) == 0) { dpixel(x + 4, y - 1, C_RGB(31, 24, 8)); dpixel(x + 5, y - 1, C_RGB(31, 24, 8)); } break;
    case 14: sq(x, y, inset, col);                                                       // argos: ojos por el cuerpo
             if (i & 1) {
                 bool bl = ((frame + i * 9) % 90) < 4;
                 if (bl) drect(x + 3, y + 4, x + 6, y + 4, C_BLACK);
                 else { drect(x + 3, y + 3, x + 6, y + 6, C_WHITE); dpixel(x + 4 + (pdx > 0), y + 4 + (pdy > 0), C_BLACK); dpixel(x + 5 + (pdx > 0), y + 5 + (pdy > 0), C_BLACK); }
             } break;
    case 13: sq(x, y, inset, col); if (i == 0) dpixel(x + 4, y + 4, C_RGB(31, 14, 18)); break;
    default: sq(x, y, inset, col); break;
    }
}

static void drawSnake(const Cell *c, int n, int ox, int oy, int skin, int frame, Dir hd, Mood mood,
                      int pdx, int pdy, bool mouth) {
    bool rainbow = (mood == MOOD_FRENZY);
    for (int i = 0; i < n - 1; i++) {
        int x = ox + c[i].x * CELL, y = oy + c[i].y * CELL;
        uint16_t col = rainbow ? hueColor(i * 9 + frame * 6, 31) : skinBody(skin, i, n, frame);
        if (mood == MOOD_DEAD) col = C_RGB(10, 12, 14);
        int inset = i == 0 ? 3 : (i == 1 ? 2 : 1);
        drawSeg(mood == MOOD_DEAD ? 0 : skin, i, n, x, y, inset, col, frame, pdx, pdy, c, rainbow);
        int dx = c[i + 1].x - c[i].x, dy = c[i + 1].y - c[i].y;
        uint16_t bc = (skin == 10 && !rainbow) ? C_RGB(20, 20, 18) : (skin == 9 && !rainbow ? shade(col, 22) : col);
        bool ghost = (skin == 8 && !rainbow && mood != MOOD_DEAD);
        if (ghost) continue;
        if ((dx == 1 || dx == -1) && dy == 0) {
            int lo = (dx == 1) ? x : x - CELL;
            drect(lo + 8, y + (skin == 10 ? 4 : 2), lo + CELL + 1, y + (skin == 10 ? 5 : 7), bc);
        } else if (dx == 0 && (dy == 1 || dy == -1)) {
            int lo = (dy == 1) ? y : y - CELL;
            drect(x + (skin == 10 ? 4 : 2), lo + 8, x + (skin == 10 ? 5 : 7), lo + CELL + 1, bc);
        }
    }
    uint16_t hc = rainbow ? hueColor(frame * 8, 31) : skinHead(skin, frame);
    if (mood == MOOD_DEAD) hc = C_RGB(14, 16, 18);
    drawHead(ox + c[n - 1].x * CELL, oy + c[n - 1].y * CELL, hd, mood == MOOD_DEAD ? 0 : skin, hc, frame, mood, pdx, pdy, mouth);
}

// Serpiente de muestra (tienda, menu, titulo): ruta fija de celdas adyacentes.
static void drawPreview(int px, int py, int skin, int frame) {
    static const int8_t P[12][2] = {{0,2},{1,2},{2,2},{2,1},{3,1},{4,1},{4,2},{5,2},{6,2},{6,1},{7,1},{8,1}};
    Cell c[12];
    for (int i = 0; i < 12; i++) { c[i].x = P[i][0]; c[i].y = P[i][1]; }
    int look = ((frame / 40) % 3) - 1;
    bool mouth = (frame % 160) > 120;
    drawSnake(c, 12, px, py, skin, frame, RIGHT, MOOD_NORMAL, look, 0, mouth);
    // Particulas ambiente propias de la skin (en el juego las emite main.cpp)
    for (int k = 0; k < 6; k++) {
        unsigned r = hsh((unsigned)(k * 977 + frame / 3 * 13));
        int bx = px + 5 + (int)(r % 80), life = (int)((frame + k * 7) % 20), by;
        switch (skin) {
        case 2: by = py + 12 - life; dpixel(bx, by, life < 10 ? C_RGB(31, 20, 3) : C_RGB(31, 6, 2)); break;
        case 3: by = py + 4 + life; dpixel(bx, by, C_WHITE); break;
        case 4: if (life < 8) dpixel(bx, py + 8 + (int)((r >> 5) % 14), GOLD); break;
        case 5: if (life < 10) dpixel(bx, py + (int)((r >> 5) % 30), PURPLE); break;
        case 8: by = py + 12 - life; dpixel(bx, by, C_RGB(18, 22, 31)); break;
        case 11: if (life < 4) dpixel(bx, py + (int)((r >> 5) % 30), C_RGB(31, 31, 10)); break;
        default: break;
        }
    }
}

// ---------------------------------------------------------------- Objetos
static void drawFood(int x, int y, int frame) {
    y += WAVE[(frame / 2) & 15] / 2;
    sq(x, y, 1, RED);
    drect(x + 3, y + 2, x + 4, y + 3, C_RGB(31, 22, 22));
    drect(x + 5, y, x + 6, y + 1, C_RGB(8, 28, 6));
}
static void drawGolden(int x, int y, int frame, bool lowTime) {
    if (lowTime && ((frame / 3) & 1)) return;
    int pulse = (frame / 3) & 1;
    sq(x, y, pulse, GOLD);
    drect(x + 3, y + 3, x + 6, y + 6, C_RGB(31, 31, 18));
    static const int8_t S[4][2] = {{-1, 4}, {4, -1}, {10, 4}, {4, 10}};
    int k = (frame / 3) & 3;
    dpixel(x + S[k][0], y + S[k][1], C_WHITE);
    dpixel(x + S[(k + 2) & 3][0], y + S[(k + 2) & 3][1], C_RGB(31, 31, 12));
}
static void drawSpecial(int x, int y, int type, int frame, bool lowTime) {
    if (lowTime && ((frame / 3) & 1)) return;
    if (type == SP_ICE) {                          // cristal de hielo
        drect(x + 4, y + 1, x + 5, y + 8, CYAN); drect(x + 1, y + 4, x + 8, y + 5, CYAN);
        drect(x + 3, y + 3, x + 6, y + 6, C_RGB(22, 30, 31));
        dpixel(x + 2, y + 2, C_WHITE); dpixel(x + 7, y + 7, C_WHITE);
    } else if (type == SP_ROTTEN) {                // podrida: verde sucio con moscas
        sq(x, y, 1, C_RGB(10, 14, 3)); drect(x + 3, y + 3, x + 5, y + 5, C_RGB(6, 9, 2));
        int k = (frame / 2) % 8;
        dpixel(x + 1 + k, y - 1 + (k & 1), C_BLACK); dpixel(x + 8 - k, y + 10 - (k & 1), C_BLACK);
    } else {                                       // arcoiris
        sq(x, y, 1, hueColor(frame * 10, 31)); drect(x + 3, y + 3, x + 6, y + 6, hueColor(frame * 10 + 60, 31));
        if ((frame / 2) & 1) dpixel(x + 4, y - 1, C_WHITE);
    }
}
static void drawRock(int x, int y, bool temp, int frame) {
    if (temp && ((frame / 3) & 1)) { sq(x, y, 1, C_RGB(18, 8, 8)); return; }
    sq(x, y, 0, temp ? C_RGB(14, 9, 9) : C_RGB(10, 11, 13));
    drect(x + 2, y + 1, x + 7, y + 1, C_RGB(17, 18, 21));
    drect(x + 2, y + 8, x + 8, y + 8, C_RGB(4, 4, 6));
    dpixel(x + 4, y + 4, C_RGB(5, 5, 7)); dpixel(x + 5, y + 5, C_RGB(5, 5, 7));
}
static void drawBerry(int x, int y, int frame, int i) {      // fruta de la lluvia
    int b = WAVE[(frame / 2 + i * 3) & 15] / 3;
    sq(x + 1, y + 1 + b, 2, C_RGB(10, 14, 31));
    dpixel(x + 3, y + 3 + b, C_WHITE);
}

static void drawBoss(const Boss &b, int ox, int oy, int frame, Cell head) {
    int px = ox + b.x * CELL, py = oy + b.y * CELL;
    if (b.warn > 0 && ((frame / 2) & 1)) { drect(px, py, px + 29, py + 29, C_RGB(14, 14, 18)); return; }
    int ex = head.x * CELL + 5 > px + 15 ? 1 : (head.x * CELL + 5 < px + 15 ? -1 : 0);
    int ey = head.y * CELL + 5 > py + 15 ? 1 : (head.y * CELL + 5 < py + 15 ? -1 : 0);
    bool low = b.hp * 2 <= b.maxhp;
    int hpw = b.hp * 28 / b.maxhp; if (hpw < 1) hpw = 1;
    drect(px, py - 5, px + 29, py - 3, C_RGB(6, 2, 2)); drect(px, py - 5, px + hpw, py - 3, low ? RED : GOOD);
    if (b.type == 0) {                             // CENTINELA: torre metalica con ojo
        drect(px, py + 3, px + 29, py + 29, C_RGB(12, 13, 17)); drect(px + 2, py + 1, px + 27, py + 3, C_RGB(18, 19, 24));
        drect(px, py + 3, px + 29, py + 4, C_RGB(22, 23, 28)); drect(px, py + 28, px + 29, py + 29, C_RGB(5, 5, 8));
        for (int k = 0; k < 4; k++) { dpixel(px + 2 + k * 8, py + 6, C_RGB(24, 24, 28)); dpixel(px + 2 + k * 8, py + 26, C_RGB(24, 24, 28)); }
        drect(px + 8, py + 8, px + 21, py + 21, C_WHITE);
        drect(px + 12 + ex * 3, py + 12 + ey * 3, px + 17 + ex * 3, py + 17 + ey * 3, low ? RED : C_RGB(31, 14, 4));
        drect(px + 14 + ex * 3, py + 14 + ey * 3, px + 15 + ex * 3, py + 15 + ey * 3, C_BLACK);
        dpixel(px + 14, py - 1, C_RGB(22, 24, 28)); dpixel(px + 14, py - 2, ((frame / 5) & 1) ? RED : C_RGB(8, 3, 3));
        drect(px + 11, py + 23, px + 18, py + 29, b.cd < 4 ? C_RGB(31, 20, 4) : C_BLACK);   // canon
    } else if (b.type == 1) {                      // ARANA: patas que se mueven
        drect(px + 6, py + 6, px + 23, py + 23, C_RGB(10, 4, 16)); drect(px + 8, py + 4, px + 21, py + 25, C_RGB(12, 5, 19));
        for (int k = 0; k < 4; k++) {
            int w = WAVE[(frame + k * 4) & 15] / 2, yy = py + 7 + k * 5;
            drect(px + 1 + w / 2, yy, px + 6, yy + 1, C_RGB(8, 3, 12)); drect(px - 1 + w, yy + 1, px + 2 + w, yy + 4, C_RGB(8, 3, 12));
            drect(px + 23, yy, px + 28 - w / 2, yy + 1, C_RGB(8, 3, 12)); drect(px + 27 - w, yy + 1, px + 30 - w, yy + 4, C_RGB(8, 3, 12));
        }
        for (int k = 0; k < 4; k++) drect(px + 9 + k * 4, py + 10 + ey, px + 10 + k * 4, py + 11 + ey, low ? RED : C_RGB(31, 6, 24));
        drect(px + 11, py + 19, px + 12, py + 22, C_WHITE); drect(px + 17, py + 19, px + 18, py + 22, C_WHITE);
    } else {                                       // REY: corona y capa
        drect(px + 2, py + 8, px + 27, py + 29, C_RGB(18, 4, 8)); drect(px + 5, py + 6, px + 24, py + 24, C_RGB(14, 8, 24));
        for (int k = 0; k < 3; k++) { drect(px + 6 + k * 8, py + 1, px + 9 + k * 8, py + 6, GOLD); dpixel(px + 7 + k * 8, py, C_WHITE); }
        drect(px + 5, py + 5, px + 24, py + 6, GOLD);
        drect(px + 9, py + 12, px + 12, py + 15, C_RGB(31, 28, 4)); drect(px + 17, py + 12, px + 20, py + 15, C_RGB(31, 28, 4));
        dpixel(px + 10 + ex, py + 13 + ey, C_BLACK); dpixel(px + 18 + ex, py + 13 + ey, C_BLACK);
        drect(px + 11, py + 19, px + 18, py + 20, low ? RED : C_RGB(20, 4, 6));
        if (b.moveCd <= 8 && ((frame / 2) & 1)) { drect(px - 1, py - 1, px + 30, py, C_WHITE); drect(px - 1, py + 29, px + 30, py + 30, C_WHITE); }
    }
}
static void drawBullet(int x, int y, int frame) {
    int cx = x + 5, cy = y + 5;
    uint16_t c = ((frame / 2) & 1) ? C_RGB(31, 24, 4) : RED;
    drect(cx - 1, cy - 2, cx + 1, cy + 2, c); drect(cx - 2, cy - 1, cx + 2, cy + 1, c);
    dpixel(cx, cy, C_WHITE);
}

// ---------------------------------------------------------------- Vista 3D (perspectiva real)
static void clipRect(int x1, int y1, int x2, int y2, uint16_t c) {
    const int VX1 = BOARD_X, VY1 = BOARD_Y, VX2 = BOARD_X + BW * CELL - 1, VY2 = BOARD_Y + BH * CELL - 1;
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    if (x2 < VX1 || x1 > VX2 || y2 < VY1 || y1 > VY2) return;
    if (x1 < VX1) x1 = VX1;
    if (x2 > VX2) x2 = VX2;
    if (y1 < VY1) y1 = VY1;
    if (y2 > VY2) y2 = VY2;
    drect(x1, y1, x2, y2, c);
}

struct BoxR { int xa, xb, ytF, yt, yg; };
static const int X0_3D = BOARD_X + 145, Y0_3D = BOARD_Y + 40;
static const float F_3D = 70.0f, CH_3D = 3.2f, BACK_3D = 3.0f;

// Caja en perspectiva: cara frontal + cara superior (mas clara).
static BoxR box3d(int dl, float z, float H, float half, uint16_t top, uint16_t front) {
    float nz = z - half, fz = z + half;
    if (nz < 0.35f) nz = 0.35f;
    float iN = F_3D / nz, iF = F_3D / fz;
    BoxR r;
    r.xa = X0_3D + (int)((dl - half) * iN); r.xb = X0_3D + (int)((dl + half) * iN);
    r.yg = Y0_3D + (int)(CH_3D * iN); r.yt = Y0_3D + (int)((CH_3D - H) * iN);
    int xaF = X0_3D + (int)((dl - half) * iF), xbF = X0_3D + (int)((dl + half) * iF);
    r.ytF = Y0_3D + (int)((CH_3D - H) * iF);
    clipRect(r.xa, r.yt, r.xb, r.yg, front);
    clipRect(r.xa < xaF ? r.xa : xaF, r.ytF, r.xb > xbF ? r.xb : xbF, r.yt, top);
    return r;
}

static void draw3D(const View &v) {
    const Game &g = *v.g; const SaveData &s = *v.s; int frame = v.frame;
    bool frenzy = g.frenzyLeft > 0;
    for (int b = 0; b < 5; b++) clipRect(BOARD_X, BOARD_Y + b * 8, BOARD_X + 289, BOARD_Y + b * 8 + 7, C_RGB(1 + b, 1 + b / 2, 4 + b * 2));
    for (int i = 0; i < 14; i++) dpixel(BOARD_X + (i * 71 + frame / 3) % 290, BOARD_Y + (i * 13) % 40, ((frame / 6 + i) & 3) ? DIM : TXT);
    clipRect(BOARD_X, Y0_3D, BOARD_X + 289, BOARD_Y + 199, C_RGB(0, 0, 1));

    static int16_t idxMap[BW * BH];
    memset(idxMap, 0xFF, sizeof idxMap);
    for (int i = 0; i < g.len; i++) { Cell c = g.cellFromTail(i); idxMap[c.y * BW + c.x] = (int16_t)i; }

    Cell h = g.head();
    int fx = DXS[g.dir], fy = DYS[g.dir], rx = -fy, ry = fx;
    static const int8_t ORD[17] = {-8, 8, -7, 7, -6, 6, -5, 5, -4, 4, -3, 3, -2, 2, -1, 1, 0};
    for (int dz = 13; dz >= -1; dz--) {
        float z = dz + BACK_3D;
        if (z < 0.6f) continue;
        if (g.fogOn() && z > 6.5f) continue;
        float inv = F_3D / z, invN = F_3D / (z - 0.5f), invF = F_3D / (z + 0.5f);
        int k = 32 - (int)(z * 1.7f); if (k < 6) k = 6;
        if (g.fogOn()) k = k * 2 / 3;
        for (int oi = 0; oi < 17; oi++) {
            int dl = ORD[oi];
            int gx = h.x + fx * dz + rx * dl, gy = h.y + fy * dz + ry * dl;
            bool inside = gx >= 0 && gy >= 0 && gx < BW && gy < BH;
            int xa = X0_3D + (int)((dl - 0.5f) * inv), xb = X0_3D + (int)((dl + 0.5f) * inv);
            int yN = Y0_3D + (int)(CH_3D * invN), yF = Y0_3D + (int)(CH_3D * invF);
            if (!inside) {
                if (gx >= -1 && gx <= BW && gy >= -1 && gy <= BH)
                    box3d(dl, z, 1.1f, 0.5f, shade(C_RGB(28, 10, 10), k), shade(C_RGB(18, 5, 6), k));
                continue;
            }
            uint16_t floorC = ((gx + gy) & 1) ? C_RGB(3, 5, 10) : C_RGB(5, 8, 15);
            if (frenzy) floorC = hueColor(gx * 12 + gy * 7 + frame * 6, 10);
            clipRect(xa, yF, xb - 1, yN, shade(floorC, k));
            int idx = idxMap[gy * BW + gx];
            if (gx == h.x && gy == h.y) {
                uint16_t hc = v.mood == MOOD_DEAD ? C_RGB(14, 16, 18) : (frenzy ? hueColor(frame * 8, 31) : skinHead(s.skin, frame));
                BoxR r = box3d(dl, z, 0.9f, 0.42f, shade(hc, k), shade(hc, k * 2 / 3));
                int tw2 = r.xb - r.xa, ey = (r.ytF + r.yt) / 2;
                if (tw2 >= 6) {                               // la cara se ve desde arriba: ojos y lengua
                    int lx = r.xa + tw2 / 4, rx2 = r.xb - tw2 / 4;
                    if (v.mood == MOOD_DEAD || (frame % 110) < 4) { drect(lx - 1, ey, lx + 1, ey, C_BLACK); drect(rx2 - 1, ey, rx2 + 1, ey, C_BLACK); }
                    else {
                        drect(lx - 1, ey - 1, lx, ey, C_WHITE); drect(rx2, ey - 1, rx2 + 1, ey, C_WHITE);
                        dpixel(lx, ey - 1, C_BLACK); dpixel(rx2 + 1, ey - 1, C_BLACK);
                    }
                    int ph = frame % 50;
                    if (ph < 14 && v.mood != MOOD_DEAD) { int len = (ph < 7 ? ph / 2 : (14 - ph) / 2) + 1; drect((r.xa + r.xb) / 2, r.ytF - len, (r.xa + r.xb) / 2 + 1, r.ytF - 1, C_RGB(31, 5, 9)); }
                }
            } else if (idx >= 0) {
                uint16_t c = v.mood == MOOD_DEAD ? C_RGB(10, 12, 14) : (frenzy ? hueColor(idx * 9 + frame * 6, 31) : skinBody(s.skin, idx, g.len, frame));
                box3d(dl, z, 0.55f, 0.42f, shade(c, k), shade(c, k * 2 / 3));
            } else if (g.rockAt(gx, gy)) {
                bool tmp = g.tempRockAt(gx, gy) && ((frame / 3) & 1);
                box3d(dl, z, 0.95f, 0.45f, shade(tmp ? C_RGB(22, 10, 10) : C_RGB(17, 18, 21), k), shade(C_RGB(9, 10, 13), k));
            } else if (gx == g.food.x && gy == g.food.y) {
                float bob = 0.45f + 0.05f * WAVE[(frame / 2) & 15] / 3;
                box3d(dl, z, bob, 0.32f, shade(C_RGB(31, 14, 14), k), shade(RED, k));
            } else if (g.goldenActive && gx == g.golden.x && gy == g.golden.y && !(g.goldenLeft < 14 && ((frame / 3) & 1))) {
                box3d(dl, z, 0.6f, 0.36f, shade(C_RGB(31, 31, 18), k), shade(GOLD, k));
            } else if (g.sp.active && gx == g.sp.c.x && gy == g.sp.c.y) {
                uint16_t c = g.sp.type == SP_ICE ? CYAN : (g.sp.type == SP_ROTTEN ? C_RGB(10, 14, 3) : hueColor(frame * 10, 31));
                box3d(dl, z, 0.5f, 0.32f, shade(c, k), shade(c, k * 2 / 3));
            } else {
                for (int b = 0; b < g.bonusN; b++)
                    if (g.bonus[b].x == gx && g.bonus[b].y == gy) box3d(dl, z, 0.35f, 0.25f, shade(C_RGB(14, 18, 31), k), shade(C_RGB(8, 10, 24), k));
            }
        }
    }
}

// ---------------------------------------------------------------- Tablero 2D
static void draw2D(const View &v) {
    const Game &g = *v.g; const Fx &fx = *v.fx; const SaveData &s = *v.s; int frame = v.frame;
    bool frenzy = g.frenzyLeft > 0;
    int ox = BOARD_X + fx.ox, oy = BOARD_Y + fx.oy;
    int bw = BW * CELL, bh = BH * CELL;

    uint16_t bgc = g.wrapping() ? C_RGB(1, 4, 7) : BOARDC;
    if (g.mirrorOn()) bgc = C_RGB(4, 2, 8);
    if (g.turboOn()) bgc = C_RGB(7, 2, 2);
    if (frenzy) bgc = hueColor(frame * 3, 5);
    drect(BOARD_X, BOARD_Y, BOARD_X + bw - 1, BOARD_Y + bh - 1, bgc);
    for (int i = 0; i < 22; i++) {
        int x = (i * 53 + frame * (1 + i % 3) / (g.turboOn() ? 1 : 2)) % bw, y = (i * 37) % bh;
        dpixel(BOARD_X + x, BOARD_Y + y, ((frame / 5 + i) & 3) ? DOT : DIM);
    }
    if (g.turboOn())                                       // lineas de velocidad
        for (int i = 0; i < 8; i++) { unsigned r = hsh((unsigned)(frame * 5 + i)); int y = (int)(r % (unsigned)bh), x = (int)((r >> 8) % (unsigned)(bw - 30)); drect(BOARD_X + x, BOARD_Y + y, BOARD_X + x + 14, BOARD_Y + y, C_RGB(18, 6, 6)); }
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++) dpixel(ox + x * CELL + 5, oy + y * CELL + 5, DOT);

    bool wrap = g.wrapping();
    if (g.shiftWarn() && ((frame / 3) & 1)) wrap = !wrap;
    uint16_t bc = wrap ? C_RGB(4, 24, 30) : C_RGB(28, 6, 6);
    if (g.mirrorOn() && ((frame / 4) & 1)) bc = C_RGB(24, 8, 31);
    if (g.turboOn() && ((frame / 2) & 1)) bc = C_RGB(31, 20, 4);
    for (int i = 0; i < bw; i++) {
        if (wrap && ((i / 6) & 1)) continue;
        dpixel(BOARD_X + i, BOARD_Y, bc); dpixel(BOARD_X + i, BOARD_Y + 1, bc);
        dpixel(BOARD_X + i, BOARD_Y + bh - 1, bc); dpixel(BOARD_X + i, BOARD_Y + bh - 2, bc);
    }
    for (int i = 0; i < bh; i++) {
        if (wrap && ((i / 6) & 1)) continue;
        dpixel(BOARD_X, BOARD_Y + i, bc); dpixel(BOARD_X + 1, BOARD_Y + i, bc);
        dpixel(BOARD_X + bw - 1, BOARD_Y + i, bc); dpixel(BOARD_X + bw - 2, BOARD_Y + i, bc);
    }

    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++)
            if (g.rockAt(x, y)) drawRock(ox + x * CELL, oy + y * CELL, g.tempRockAt(x, y), frame);
    for (int i = 0; i < g.bonusN; i++) drawBerry(ox + g.bonus[i].x * CELL, oy + g.bonus[i].y * CELL, frame, i);
    drawFood(ox + g.food.x * CELL, oy + g.food.y * CELL, frame);
    if (g.goldenActive) drawGolden(ox + g.golden.x * CELL, oy + g.golden.y * CELL, frame, g.goldenLeft < 14);
    if (g.sp.active) drawSpecial(ox + g.sp.c.x * CELL, oy + g.sp.c.y * CELL, g.sp.type, frame, g.sp.left < 18);

    Cell h = g.head();
    if (g.boss.active) drawBoss(g.boss, ox, oy, frame, h);

    static Cell cells[MAXLEN];
    for (int i = 0; i < g.len; i++) cells[i] = g.cellFromTail(i);
    int pdx = (g.food.x > h.x) - (g.food.x < h.x), pdy = (g.food.y > h.y) - (g.food.y < h.y);
    int fxd = g.food.x - h.x, fyd = g.food.y - h.y;
    bool mouth = (g.dir == RIGHT && fyd == 0 && fxd > 0 && fxd <= 3) || (g.dir == LEFT && fyd == 0 && fxd < 0 && fxd >= -3) ||
                 (g.dir == DOWN && fxd == 0 && fyd > 0 && fyd <= 3) || (g.dir == UP && fxd == 0 && fyd < 0 && fyd >= -3);
    drawSnake(cells, g.len, ox, oy, s.skin, frame, g.dir, v.mood, pdx, pdy, mouth);
    for (const auto &b : g.bullets) if (b.alive) drawBullet(ox + b.x * CELL, oy + b.y * CELL, frame);

    if (g.fogOn()) {                                      // niebla: solo se ve una ventana de 7x7 alrededor
        int x0 = ox + (h.x - 3) * CELL, x1 = ox + (h.x + 4) * CELL - 1, y0 = oy + (h.y - 3) * CELL, y1 = oy + (h.y + 4) * CELL - 1;
        int bx2 = BOARD_X + bw - 1, by2 = BOARD_Y + bh - 1;
        if (y0 > BOARD_Y) drect(BOARD_X, BOARD_Y, bx2, y0 - 1 > by2 ? by2 : y0 - 1, C_BLACK);
        if (y1 < by2) drect(BOARD_X, y1 + 1 < BOARD_Y ? BOARD_Y : y1 + 1, bx2, by2, C_BLACK);
        int ya = y0 < BOARD_Y ? BOARD_Y : y0, yb = y1 > by2 ? by2 : y1;
        if (ya <= yb) {
            if (x0 > BOARD_X) drect(BOARD_X, ya, x0 - 1 > bx2 ? bx2 : x0 - 1, yb, C_BLACK);
            if (x1 < bx2) drect(x1 + 1 < BOARD_X ? BOARD_X : x1 + 1, ya, bx2, yb, C_BLACK);
        }
    }
}

// ---------------------------------------------------------------- HUD y escena completa
void drawGame(const View &v) {
    const Game &g = *v.g; const Fx &fx = *v.fx; const SaveData &s = *v.s; int frame = v.frame;
    bool frenzy = g.frenzyLeft > 0;
    int bw = BW * CELL;
    dclear(BG);
    if (g.mode == MODE_3D) draw3D(v); else draw2D(v);

    for (const auto &q : fx.p) if (q.life) drawParticle(q);
    for (const auto &r : fx.rg)
        if (r.life) { ringDraw(r.x, r.y, r.r, r.color); if (r.r > 4) ringDraw(r.x, r.y, r.r - 3, r.color); }
    for (const auto &t : fx.ft) if (t.life) dtext(t.x - tw(t.s) / 2, t.y, t.color, t.s);
    if (fx.flash > 0) drect(BOARD_X, BOARD_Y, BOARD_X + bw - 1, BOARD_Y + 3, C_WHITE);

    drect(0, 0, 395, 14, SIDE);
    dtext(4, 3, TXT, MODE_NAME[g.mode]);
    dprint(300, 3, GOLD, "$%d", (int)s.coins);
    dprint(350, 3, DIM, "Nv%d", levelFromXp(s.xp));

    int sx = 298;
    drect(292, 16, 395, 223, SIDE);
    drect(292, 16, 292, 223, frenzy ? hueColor(frame * 6, 31) : C_RGB(6, 14, 20));
    dtext(sx, 18, DIM, "PUNTOS");
    dprint(sx, 28, GOOD, "%d", (int)g.score);
    dtext(sx, 42, DIM, "MEJOR");
    dprint(sx, 52, TXT, "%d", (int)(g.score > s.best[g.mode] ? g.score : s.best[g.mode]));
    dprint(sx, 66, DIM, "COMBO");
    dprint(sx + 48, 66, g.combo > 1 ? GOLD : TXT, "x%d", g.combo);
    drect(sx, 77, sx + 90, 81, C_RGB(4, 5, 9));
    if (g.comboPct() > 0) drect(sx, 77, sx + g.comboPct() * 90 / 100, 81, g.combo >= COMBO_MAX ? hueColor(frame * 8, 31) : GOLD);
    dprint(sx, 88, DIM, "LARGO");
    dprint(sx + 48, 88, TXT, "%d", g.len);
    dtext(sx, 102, DIM, "MEJORA");
    drect(sx, 113, sx + 90, 117, C_RGB(4, 5, 9));
    int up = g.eaten % UPGRADE_EVERY;
    if (up > 0) drect(sx, 113, sx + up * 90 / UPGRADE_EVERY, 117, PURPLE);
    dtext(sx, 124, DIM, "ESCUDO");
    for (int i = 0; i < g.shield && i < 5; i++) { drect(sx + 44 + i * 9, 124, sx + 49 + i * 9, 131, CYAN); dpixel(sx + 46 + i * 9, 132, CYAN); }
    switch (g.mode) {
    case MODE_TIME: {
        dtext(sx, 138, DIM, "TIEMPO");
        dprint(sx + 48, 138, g.timeLeftMs < 10000 ? RED : TXT, "%d s", g.timeLeftMs / 1000);
        int w = g.timeLeftMs * 90 / 60000; if (w > 90) w = 90;
        drect(sx, 149, sx + 90, 153, C_RGB(4, 5, 9));
        if (w > 0) drect(sx, 149, sx + w, 153, g.timeLeftMs < 10000 ? RED : GOOD);
        break; }
    case MODE_SHIFT:
        dtext(sx, 138, DIM, g.dimension ? "PORTALES" : "MUROS");
        drect(sx, 149, sx + 90, 153, C_RGB(4, 5, 9));
        drect(sx, 149, sx + g.shiftTimer * 90 / SHIFT_PERIOD, 153, g.shiftWarn() ? RED : C_RGB(10, 20, 31));
        break;
    case MODE_BOSS:
        if (g.boss.active) {
            dtext(sx, 138, RED, BOSS_NAME[g.boss.type]);
            drect(sx, 149, sx + 90, 153, C_RGB(4, 5, 9));
            drect(sx, 149, sx + (g.boss.hp > 0 ? g.boss.hp : 0) * 90 / g.boss.maxhp, 153, g.boss.hp * 2 <= g.boss.maxhp ? RED : GOOD);
        } else dprint(sx, 138, DIM, "JEFE %d...", g.stage);
        break;
    case MODE_ROCKS: case MODE_3D: dprint(sx, 138, DIM, "ROCAS %d", g.rockCount); break;
    default: break;
    }
    if (g.evId != GEV_NONE) {
        bool warn = g.evWarn > 0;
        if (!warn || ((frame / 3) & 1)) dtext(sx, 164, warn ? RED : DIM, warn ? "PREPARATE!" : "EVENTO");
        dtext(sx, 174, PURPLE, EVENT_NAME[g.evId]);
        int w = warn ? g.evWarn * 90 / EVENT_WARN : g.evLeft * 90 / 140; if (w > 90) w = 90;
        drect(sx, 185, sx + 90, 189, C_RGB(4, 5, 9));
        if (w > 0) drect(sx, 185, sx + w, 189, warn ? RED : PURPLE);
    }
    if (frenzy) { if ((frame / 3) & 1) dtext(sx, 196, hueColor(frame * 10, 31), "FIEBRE x2!"); }
    else if (g.streak > 0) dprint(sx, 196, GOLD, "RACHA %d/3", g.streak);
    if (g.iceLeft > 0) dtext(sx, 207, CYAN, "HIELO!");
    else if (g.scorePct > 0) dprint(sx, 207, PURPLE, "PTS +%d%%", g.scorePct);

    int fw = frenzy ? g.frenzyLeft * 290 / FRENZY_TICKS : g.streak * 290 / 3;
    if (fw > 0) drect(0, 218, fw, 222, frenzy ? hueColor(frame * 8, 31) : GOLD);

    if (v.bannerLeft > 0 && v.banner) {
        int w = tw(v.banner) + 16;
        drect(145 - w / 2, 20, 145 + w / 2, 36, C_BLACK);
        drect(145 - w / 2, 20, 145 + w / 2, 21, GOLD);
        ctext(145, 25, ((frame / 4) & 1) ? GOLD : TXT, v.banner);
    }
    if (v.paused) {
        drect(60, 80, 230, 140, C_BLACK);
        ctext(145, 90, TXT, "PAUSA");
        ctext(145, 108, DIM, "EXE seguir");
        ctext(145, 120, DIM, "EXIT salir al menu");
    }
    if (v.present) dupdate();
}

// ---------------------------------------------------------------- Otras pantallas
static void drawVersion(uint16_t c) {
    static const uint8_t G[12][5] = {
        {7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},{7,4,7,1,7},
        {7,4,7,5,7},{7,1,1,2,2},{7,5,7,5,7},{7,5,7,1,7},{5,5,5,5,2},{0,0,0,0,2}};
    const char *t = VERSION_TEXT;
    int n = (int)strlen(t);
    int x = 394 - n * 4, y = 216;
    for (int i = 0; i < n; i++, x += 4) {
        int g = (t[i] >= '0' && t[i] <= '9') ? t[i] - '0' : (t[i] == 'v' ? 10 : (t[i] == '.' ? 11 : -1));
        if (g < 0) continue;
        for (int r = 0; r < 5; r++)
            for (int b = 0; b < 3; b++)
                if (G[g][r] & (4 >> b)) dpixel(x + b, y + r, c);
    }
}

void drawTitle(int frame, LoadResult lr, const SaveData &s) {
    dclear(BG);
    for (int i = 0; i < 40; i++) {
        int x = (i * 53 + frame * (1 + i % 3)) % 396, y = (i * 37) % 224;
        dpixel(x, y, ((frame / 6 + i) & 3) ? DOT : DIM);
    }
    const char *t = "OUROBOROS";
    for (int i = 0; t[i]; i++) {
        char ch[2] = {t[i], 0};
        int y = 34 + WAVE[(frame / 2 + i * 2) & 15];
        dtext(110 + i * 20, y, hueColor(i * 14 + frame * 2, 31), ch);
    }
    ctext(198, 56, DIM, "SNAKE DIMENSION");
    drawPreview(150, 90, s.skin, frame);
    if ((frame / 15) & 1) ctext(198, 160, TXT, "Pulsa EXE");
    drawVersion(DIM);
    if (lr == LOAD_RECOVERED) ctext(198, 190, C_RGB(31, 24, 4), "Guardado recuperado de copia");
    if (lr == LOAD_CORRUPT) ctext(198, 190, C_RGB(31, 8, 4), "Guardado danado: perfil reiniciado");
    dupdate();
}

static void xpBar(int x, int y, int w, const SaveData &s) {
    int lv = levelFromXp(s.xp);
    uint32_t a = xpForLevel(lv), b = xpForLevel(lv + 1);
    int fill = b > a ? (int)((s.xp - a) * (uint32_t)w / (b - a)) : 0;
    drect(x, y, x + w, y + 4, C_RGB(4, 5, 9));
    if (fill > 0) drect(x, y, x + fill, y + 4, C_RGB(10, 28, 31));
}

void drawMenu(int sel, int frame, const SaveData &s, const char *msg) {
    dclear(BG);
    dtext(10, 6, GOOD, "OUROBOROS");
    int lv = levelFromXp(s.xp);
    dprint(240, 6, TXT, "Nv %d", lv);
    dprint(290, 6, GOLD, "$%d", (int)s.coins);
    xpBar(240, 17, 100, s);
    static const char *extra[3] = {"TIENDA", "LOGROS", "STATS"};
    for (int i = 0; i < MODE_COUNT + 3; i++) {
        int y = 26 + i * 16;
        bool on = (i == sel);
        bool locked = (i < MODE_COUNT && lv < MODE_LEVEL[i]);
        if (on) drect(6, y - 3, 224, y + 11, C_RGB(3, 8, 7));
        uint16_t col = locked ? C_RGB(10, 6, 6) : (on ? C_RGB(12, 31, 16) : DIM);
        const char *name = i < MODE_COUNT ? MODE_NAME[i] : extra[i - MODE_COUNT];
        dtext(16 + (on ? ((frame / 4) & 3) : 0), y, col, name);
        if (i < MODE_COUNT) {
            if (locked) dprint(150, y, C_RGB(26, 8, 8), "Nv %d", (int)MODE_LEVEL[i]);
            else dprint(150, y, DIM, "%d", (int)s.best[i]);
        }
    }
    drawPreview(250, 60, s.skin, frame);
    if (sel < MODE_COUNT) dtext(10, 192, TXT, MODE_DESC[sel]);
    else if (sel == MODE_COUNT) dtext(10, 192, TXT, "Skins, estelas y efectos con tus monedas.");
    else if (sel == MODE_COUNT + 1) dprint(10, 192, TXT, "Logros: %d/%d", popcount32(s.achievements), ACH_COUNT);
    else dtext(10, 192, TXT, "Tu historial y progreso.");
    if (msg && msg[0]) dtext(10, 204, C_RGB(31, 24, 4), msg);
    else dtext(10, 212, DIM, "ARRIBA/ABAJO  EXE=elegir  MENU=salir");
    drawVersion(DIM);
    dupdate();
}

void drawUpgrade(const Game &g, int sel, int frame) {
    drect(4, 40, 288, 200, C_BLACK);
    ctext(146, 46, GOLD, g.mode == MODE_BOSS && g.bossesDefeated > 0 && g.eaten % UPGRADE_EVERY != 0 ? "JEFE DERROTADO! ELIGE" : "ELIGE UNA MEJORA");
    for (int i = 0; i < 3; i++) {
        int x = 10 + i * 92, y = 66;
        bool on = (i == sel);
        drect(x, y, x + 86, y + 112, on ? C_RGB(6, 4, 14) : C_RGB(2, 2, 5));
        uint16_t edge = on ? hueColor(frame * 6, 31) : C_RGB(8, 8, 12);
        drect(x, y, x + 86, y + 1, edge); drect(x, y + 111, x + 86, y + 112, edge);
        drect(x, y, x + 1, y + 112, edge); drect(x + 85, y, x + 86, y + 112, edge);
        int u = g.offer[i];
        ctext(x + 43, y + 10, on ? TXT : DIM, UPG_NAME[u]);
        drect(x + 33, y + 34, x + 53, y + 54, on ? GOLD : C_RGB(12, 12, 6));
        ctext(x + 43, y + 66, on ? TXT : DIM, UPG_D1[u]);
        ctext(x + 43, y + 78, on ? TXT : DIM, UPG_D2[u]);
    }
    ctext(146, 186, DIM, "IZQ/DER y EXE");
    dupdate();
}

void drawOver(const Game &g, const SaveData &s, const RunResult &rr, uint32_t runAch, int achCoins, int frame) {
    drect(20, 24, 272, 210, C_BLACK);
    drect(20, 24, 272, 25, GOLD); drect(20, 209, 272, 210, GOLD);
    bool won = g.state == ST_WON;
    ctext(146, 30, won ? GOLD : (g.timeUp ? GOLD : RED), won ? "TABLERO COMPLETO!" : (g.timeUp ? "TIEMPO AGOTADO" : "GAME OVER"));
    dprint(40, 48, TXT, "Puntos: %d", (int)g.score);
    if (g.mode == MODE_BOSS) dprint(40, 60, DIM, "Jefes derrotados: %d", g.bossesDefeated);
    else dprint(40, 60, DIM, "Largo %d   Combo max x%d", g.len, g.maxCombo);
    if (rr.record) { if ((frame / 4) & 1) dtext(40, 74, GOLD, "NUEVO RECORD!"); }
    else dprint(40, 74, DIM, "Record: %d", (int)s.best[g.mode]);
    dprint(40, 90, GOLD, "+%d monedas", (int)rr.coins + achCoins);
    if (rr.levelAfter > rr.levelBefore) {
        if ((frame / 3) & 1) dprint(40, 104, hueColor(frame * 8, 31), "SUBES A NIVEL %d!", rr.levelAfter);
    } else xpBar(40, 106, 150, s);
    int y = 120, shown = 0, total = popcount32(runAch);
    for (int i = 0; i < ACH_COUNT && shown < 4; i++)
        if ((runAch >> i) & 1) { dprint(40, y, PURPLE, "LOGRO: %s", ACH_NAME[i]); y += 12; shown++; }
    if (total > shown) dprint(40, y, PURPLE, "+%d logros mas", total - shown);
    ctext(146, 192, DIM, "EXE=otra vez   EXIT=menu");
    dupdate();
}

static void shape(int px, int py, uint16_t color, uint8_t kind) {
    Particle q; q.x = (int16_t)(px * 16); q.y = (int16_t)(py * 16); q.vx = q.vy = 0; q.life = 1; q.color = color; q.kind = kind;
    drawParticle(q);
}

void drawShop(int tab, int sel, const SaveData &s, int frame, const char *msg) {
    dclear(BG);
    static const char *TABN[3] = {"SKINS", "ESTELAS", "EFECTOS"};
    for (int t = 0; t < 3; t++) dtext(10 + t * 56, 6, t == tab ? GOOD : DIM, TABN[t]);
    dprint(300, 6, GOLD, "$%d", (int)s.coins);
    int n = tab == 0 ? SKIN_COUNT : (tab == 1 ? TRAIL_COUNT : EATFX_COUNT);
    int first = sel - 3; if (first > n - 8) first = n - 8; if (first < 0) first = 0;
    for (int r = 0; r < 8 && first + r < n; r++) {
        int i = first + r, y = 26 + r * 19;
        const ItemInfo &it = tab == 0 ? SKINS[i] : (tab == 1 ? TRAILS[i] : EATFX[i]);
        bool own = tab == 0 ? ownsSkin(s, i) : (tab == 1 ? ownsTrail(s, i) : ownsEatFx(s, i));
        bool eq = tab == 0 ? (s.skin == i) : (tab == 1 ? (s.trail == i) : (s.eatfx == i));
        if (i == sel) drect(6, y - 4, 205, y + 13, C_RGB(3, 8, 7));
        dtext(14, y, i == sel ? TXT : DIM, it.name);
        if (eq) dtext(110, y, GOOD, "EQUIPADO");
        else if (own) dtext(110, y, DIM, "TUYO");
        else if (it.cost == NOT_BUYABLE) dtext(110, y, PURPLE, "SECRETO");
        else dprint(110, y, s.coins >= it.cost ? GOLD : C_RGB(20, 8, 8), "$%d", (int)it.cost);
    }
    if (first > 0) dtext(190, 20, DIM, "^");
    if (first + 8 < n) dtext(190, 178, DIM, "v");

    if (tab == 0) {
        bool locked = !ownsSkin(s, sel) && SKINS[sel].cost == NOT_BUYABLE;
        drawPreview(230, 60, locked ? 0 : sel, frame);
        if (locked) ctext(275, 100, PURPLE, "???");
    } else if (tab == 1) {
        drawPreview(230, 40, s.skin, frame);
        int px = 250 + (frame * 2) % 100, py = 130 + WAVE[(frame / 2) & 15];
        shape(px, py, GOLD, 0);
        if (sel > 0) for (int k = 1; k < 9; k++) {
            uint16_t c; uint8_t kd; trailParams(sel, frame - k * 3, c, kd);
            shape(px - k * 6, py + ((k & 1) ? 3 : -2) + WAVE[(frame / 2 + k * 3) & 15] / 2, c, kd);
        }
    } else {
        int cx = 285, cy = 100, ph = frame % 40;
        static const int8_t D8[8][2] = {{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1}};
        shape(cx, cy, RED, 0); drect(cx - 3, cy - 3, cx + 3, cy + 3, ph < 4 ? C_BLACK : RED);
        switch (sel) {
        case 0: if (ph < 14) for (int k = 0; k < 8; k++) drect(cx + D8[k][0] * ph, cy + D8[k][1] * ph, cx + D8[k][0] * ph + 1, cy + D8[k][1] * ph + 1, C_RGB(31, 10, 8)); break;
        case 1: if (ph < 20) for (int k = 0; k < 8; k++) drect(cx + D8[k][0] * ph * 2, cy + D8[k][1] * ph * 2, cx + D8[k][0] * ph * 2 + 2, cy + D8[k][1] * ph * 2 + 2, C_RGB(31, 20, 4)); if (ph < 20) { ringDraw(cx, cy, ph, C_RGB(31, 10, 8)); ringDraw(cx, cy, ph / 2, GOLD); } break;
        case 2: for (int k = 0; k < 3; k++) { int r = (ph + k * 8) % 30; ringDraw(cx, cy, r + 2, shade(CYAN, 32 - r)); } break;
        case 3: for (int k = 0; k < 14; k++) { unsigned r = hsh((unsigned)(k * 31)); if (ph < 28) drect(cx + (int)(r % 50) - 25, cy - 10 + (int)((r >> 8) % 10) + ph, cx + (int)(r % 50) - 24, cy - 9 + (int)((r >> 8) % 10) + ph, hueColor(k * 14, 31)); } break;
        case 4: if (ph < 14) for (int k = 0; k < 8; k += 2) shape(cx + D8[k][0] * ph * 2, cy + D8[k][1] * ph * 2, C_RGB(31, 31, 10), 2); if (ph < 3) drect(cx - 30, cy, cx + 30, cy, C_WHITE); break;
        default: if (ph < 30) for (int k = 0; k < 5; k++) shape(cx - 12 + k * 6, cy - ph + (k & 1) * 6, C_RGB(31, 9, 18), 1); break;
        }
    }
    if (msg && msg[0]) dtext(10, 196, C_RGB(31, 24, 4), msg);
    dtext(10, 210, DIM, "IZQ/DER=pestana  EXE=comprar/equipar  EXIT=volver");
    dupdate();
}

void drawAch(const SaveData &s) {
    dclear(BG);
    dprint(10, 6, GOOD, "LOGROS %d/%d", popcount32(s.achievements), ACH_COUNT);
    for (int i = 0; i < ACH_COUNT; i++) {
        int col = i / 8, row = i % 8;
        int x = 8 + col * 130, y = 28 + row * 22;
        bool got = (s.achievements >> i) & 1;
        drect(x, y, x + 8, y + 8, got ? GOLD : C_RGB(5, 5, 8));
        dtext(x + 14, y, got ? TXT : DIM, (got || i != ACH_SECRET) ? ACH_NAME[i] : "???");
        dprint(x + 14, y + 10, DIM, "+%d", (int)ACH_REWARD[i]);
    }
    dtext(10, 210, DIM, "EXIT = volver");
    dupdate();
}

void drawStats(const SaveData &s) {
    dclear(BG);
    dtext(10, 6, GOOD, "ESTADISTICAS");
    int lv = levelFromXp(s.xp), y = 24;
    dprint(14, y, TXT, "Nivel %d  (XP %d / %d)", lv, (int)s.xp, (int)xpForLevel(lv + 1)); y += 13;
    dprint(14, y, TXT, "Monedas: %d", (int)s.coins); y += 13;
    dprint(14, y, TXT, "Partidas: %d   Muertes: %d", (int)s.games, (int)s.deaths); y += 13;
    dprint(14, y, TXT, "Comida total: %d", (int)s.food); y += 13;
    dprint(14, y, TXT, "Jefes: %d  Eventos: %d", (int)s.bossKills, (int)s.eventsTotal); y += 17;
    for (int i = 0; i < MODE_COUNT; i++) { dprint(14, y, DIM, "%s: %d", MODE_NAME[i], (int)s.best[i]); y += 12; }
    dtext(10, 210, DIM, "EXIT = volver");
    dupdate();
}

}
