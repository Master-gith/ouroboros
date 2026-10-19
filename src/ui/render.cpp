// Renderizado 2D con gint (fx-CG50, VRAM 396x224). Solo usa dclear/drect/dpixel/dtext/dprint/dupdate.
#include "render.h"
#include "../core/content.h"
#include <gint/display.h>
#include <string.h>

namespace od {

static const uint16_t BG    = C_RGB(1, 1, 3);
static const uint16_t BOARDC = C_RGB(2, 3, 5);
static const uint16_t DOT   = C_RGB(4, 6, 9);
static const uint16_t TXT   = C_RGB(27, 30, 31);
static const uint16_t DIM   = C_RGB(12, 15, 20);
static const uint16_t GOOD  = C_RGB(12, 31, 16);
static const uint16_t GOLD  = C_RGB(31, 27, 3);
static const uint16_t RED   = C_RGB(31, 6, 8);
static const uint16_t SIDE  = C_RGB(2, 3, 7);

static const int8_t WAVE[16] = {0, 1, 2, 3, 3, 2, 1, 0, 0, -1, -2, -3, -3, -2, -1, 0};

static int tw(const char *s) { int n = 0; while (*s++) n += 6; return n; }
static void ctext(int cx, int y, uint16_t c, const char *s) { dtext(cx - tw(s) / 2, y, c, s); }

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

// ---------------------------------------------------------------- Efectos
void Fx::clear() {
    for (auto &q : p) q.life = 0;
    for (auto &r : rg) r.life = 0;
    for (auto &t : ft) t.life = 0;
    flash = 0; shake = 0; ox = 0; oy = 0;
}
void Fx::burst(int px, int py, uint16_t color, int count, int spread) {
    for (auto &q : p) {
        if (count <= 0) break;
        if (q.life) continue;
        q.x = (int16_t)(px * 16); q.y = (int16_t)(py * 16);
        q.vx = (int16_t)((int)rng.range((uint32_t)(2 * spread + 1)) - spread);
        q.vy = (int16_t)((int)rng.range((uint32_t)(2 * spread + 1)) - spread);
        q.life = (uint8_t)(10 + rng.range(12)); q.color = color;
        count--;
    }
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

static void ringDraw(int cx, int cy, int r, uint16_t c) {
    int x = r, y = 0, e = 1 - r;
    while (x >= y) {
        dpixel(cx + x, cy + y, c); dpixel(cx - x, cy + y, c); dpixel(cx + x, cy - y, c); dpixel(cx - x, cy - y, c);
        dpixel(cx + y, cy + x, c); dpixel(cx - y, cy + x, c); dpixel(cx + y, cy - x, c); dpixel(cx - y, cy - x, c);
        y++;
        if (e < 0) e += 2 * y + 1; else { x--; e += 2 * (y - x) + 1; }
    }
}

// ---------------------------------------------------------------- Serpiente
static uint16_t glitchCol(int i, int frame) {
    static const uint16_t c[5] = {C_RGB(31, 2, 20), C_RGB(2, 31, 22), C_RGB(31, 31, 4), C_RGB(8, 4, 31), C_RGB(31, 31, 31)};
    unsigned v = (unsigned)(i * 2654435761u + (unsigned)(frame / 2) * 40503u);
    return c[(v >> 13) % 5];
}

static uint16_t skinBody(int skin, int i, int n, int frame) {
    int t = n > 1 ? (i * 31) / (n - 1) : 31;      // 0 cola ... 31 cabeza
    int alt = (i & 1) ? 3 : 0;
    switch (skin) {
    case 1: { int ph = (i * 4 + frame * 2) & 63, a = ph < 32 ? ph : 63 - ph;
              return C_RGB(6 + a * 18 / 31 + alt, 10 + (31 - a) * 18 / 31, 26 + alt / 2); }
    case 2: { int fl = ((frame / 2 + i) % 3) * 2; return C_RGB(18 + t * 13 / 31, 2 + t * 18 / 31 + fl + alt, t > 26 ? 6 : 0); }
    case 3: return C_RGB(10 + t * 12 / 31 + alt, 20 + t * 10 / 31, 31);
    case 4: { int pos = (frame * 2) % (n + 8) - 4, d = i - pos; if (d < 0) d = -d;
              if (d <= 1) return C_RGB(31, 31, 20);
              return C_RGB(24 + alt / 2, 16 + t / 4, 2); }
    case 5: { if ((i * 7 + frame / 2) % 11 == 0) return C_RGB(31, 31, 31);
              return C_RGB(8 + t / 3 + alt, 2, 16 + t / 2); }
    case 6: return hueColor(i * 10 + frame * 4, 31);
    case 7: return glitchCol(i, frame);
    default: return C_RGB(2 + t / 9 + alt / 2, 11 + t * 13 / 31 + alt, 5 + t / 8);
    }
}
static uint16_t skinHead(int skin, int frame) {
    switch (skin) {
    case 1: return C_RGB(24, 31, 31);
    case 2: return C_RGB(31, 26, 8);
    case 3: return C_RGB(28, 31, 31);
    case 4: return C_RGB(31, 30, 14);
    case 5: return C_RGB(28, 16, 31);
    case 6: return hueColor(frame * 5, 31);
    case 7: return glitchCol(99, frame);
    default: return C_RGB(18, 31, 20);
    }
}

// Cuadrado de 10 px con esquinas biseladas.
static void sq(int x, int y, int inset, uint16_t c) {
    int a = inset, b = 9 - inset;
    drect(x + a + 1, y + a, x + b - 1, y + b, c);
    drect(x + a, y + a + 1, x + b, y + b - 1, c);
}

static void drawHead(int x, int y, Dir d, uint16_t hc, int frame, Mood mood, int pdx, int pdy, bool mouth) {
    sq(x, y, 0, hc);
    static const int EX[4][2] = {{1, 6}, {5, 5}, {1, 6}, {2, 2}};      // UP, RIGHT, DOWN, LEFT
    static const int EY[4][2] = {{2, 2}, {1, 6}, {5, 5}, {1, 6}};
    bool blink = (frame % 110) < 4;
    for (int e = 0; e < 2; e++) {
        int ex = x + EX[d][e], ey = y + EY[d][e];
        if (mood == MOOD_DEAD) {                      // ojos en X
            uint16_t k = C_BLACK;
            dpixel(ex, ey, k); dpixel(ex + 2, ey, k); dpixel(ex + 1, ey + 1, k); dpixel(ex, ey + 2, k); dpixel(ex + 2, ey + 2, k);
        } else if (mood == MOOD_HAPPY) {              // ojos felices ^
            dpixel(ex, ey + 2, C_BLACK); dpixel(ex + 1, ey + 1, C_BLACK); dpixel(ex + 2, ey + 2, C_BLACK);
        } else if (blink) {
            drect(ex, ey + 1, ex + 2, ey + 1, C_BLACK);
        } else if (mood == MOOD_FRENZY) {             // ojos rojos con pupila blanca y ceja
            drect(ex, ey, ex + 2, ey + 2, C_RGB(31, 4, 4));
            dpixel(ex + 1 + pdx, ey + 1 + pdy, C_WHITE);
            drect(ex, ey - 1, ex + 2, ey - 1, C_BLACK);
        } else {
            drect(ex, ey, ex + 2, ey + 2, C_WHITE);
            dpixel(ex + 1 + pdx, ey + 1 + pdy, C_BLACK);
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
    } else if (mood != MOOD_DEAD) {                   // lengua bífida que entra y sale
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

static void drawSnake(const Cell *c, int n, int ox, int oy, int skin, int frame, Dir hd, Mood mood,
                      int pdx, int pdy, bool mouth) {
    bool rainbow = (mood == MOOD_FRENZY);
    for (int i = 0; i < n - 1; i++) {
        int x = ox + c[i].x * CELL, y = oy + c[i].y * CELL;
        uint16_t col = rainbow ? hueColor(i * 9 + frame * 6, 31) : skinBody(skin, i, n, frame);
        if (mood == MOOD_DEAD) col = C_RGB(10, 12, 14);
        sq(x, y, i == 0 ? 3 : (i == 1 ? 2 : 1), col);
        int dx = c[i + 1].x - c[i].x, dy = c[i + 1].y - c[i].y;
        if ((dx == 1 || dx == -1) && dy == 0) {
            int lo = (dx == 1) ? x : x - CELL;
            drect(lo + 8, y + 2, lo + CELL + 1, y + 7, col);
        } else if (dx == 0 && (dy == 1 || dy == -1)) {
            int lo = (dy == 1) ? y : y - CELL;
            drect(x + 2, lo + 8, x + 7, lo + CELL + 1, col);
        }
    }
    uint16_t hc = rainbow ? hueColor(frame * 8, 31) : skinHead(skin, frame);
    if (mood == MOOD_DEAD) hc = C_RGB(14, 16, 18);
    drawHead(ox + c[n - 1].x * CELL, oy + c[n - 1].y * CELL, hd, hc, frame, mood, pdx, pdy, mouth);
}

// Serpiente de muestra (tienda, menú, título): ruta fija de celdas adyacentes.
static void drawPreview(int px, int py, int skin, int frame) {
    static const int8_t P[12][2] = {{0,2},{1,2},{2,2},{2,1},{3,1},{4,1},{4,2},{5,2},{6,2},{6,1},{7,1},{8,1}};
    Cell c[12];
    for (int i = 0; i < 12; i++) { c[i].x = P[i][0]; c[i].y = P[i][1]; }
    int look = ((frame / 40) % 3) - 1;
    drawSnake(c, 12, px, py, skin, frame, RIGHT, MOOD_NORMAL, look, 0, (frame % 160) > 140);
}

// ---------------------------------------------------------------- Comida y rocas
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
static void drawRock(int x, int y) {
    sq(x, y, 0, C_RGB(10, 11, 13));
    drect(x + 2, y + 1, x + 7, y + 1, C_RGB(17, 18, 21));
    drect(x + 2, y + 8, x + 8, y + 8, C_RGB(4, 4, 6));
    dpixel(x + 4, y + 4, C_RGB(5, 5, 7)); dpixel(x + 5, y + 5, C_RGB(5, 5, 7));
}

// Fuente diminuta 3x5 propia (solo 'v', '.', digitos) para el numero de version.
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

// ---------------------------------------------------------------- Pantallas
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
    for (int i = 0; i < 8; i++) {
        int y = 28 + i * 20;
        bool on = (i == sel);
        bool locked = (i < MODE_COUNT && lv < MODE_LEVEL[i]);
        if (on) drect(6, y - 4, 224, y + 14, C_RGB(3, 8, 7));
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
    else if (sel == 5) dtext(10, 192, TXT, "Skins y estelas con tus monedas.");
    else if (sel == 6) dprint(10, 192, TXT, "Logros: %d/%d", popcount32(s.achievements), ACH_COUNT);
    else dtext(10, 192, TXT, "Tu historial y progreso.");
    if (msg && msg[0]) dtext(10, 204, C_RGB(31, 24, 4), msg);
    else dtext(10, 212, DIM, "ARRIBA/ABAJO  EXE=elegir  MENU=salir");
    drawVersion(DIM);
    dupdate();
}

void drawGame(const View &v) {
    const Game &g = *v.g; const Fx &fx = *v.fx; const SaveData &s = *v.s; int frame = v.frame;
    bool frenzy = g.frenzyLeft > 0;
    int ox = BOARD_X + fx.ox, oy = BOARD_Y + fx.oy;
    int bw = BW * CELL, bh = BH * CELL;
    dclear(BG);

    uint16_t bgc = g.wrapping() ? C_RGB(1, 4, 7) : BOARDC;
    if (frenzy) bgc = hueColor(frame * 3, 5);
    drect(BOARD_X, BOARD_Y, BOARD_X + bw - 1, BOARD_Y + bh - 1, bgc);
    for (int i = 0; i < 22; i++) {            // estrellas que derivan
        int x = (i * 53 + frame * (1 + i % 3) / 2) % bw, y = (i * 37) % bh;
        dpixel(BOARD_X + x, BOARD_Y + y, ((frame / 5 + i) & 3) ? DOT : DIM);
    }
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++)
            dpixel(ox + x * CELL + 5, oy + y * CELL + 5, DOT);

    // Borde: sólido y rojo con muros; cian discontinuo con portales; parpadea al cambiar.
    bool wrap = g.wrapping();
    if (g.shiftWarn() && ((frame / 3) & 1)) wrap = !wrap;
    uint16_t bc = wrap ? C_RGB(4, 24, 30) : C_RGB(28, 6, 6);
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
            if (g.rockAt(x, y)) drawRock(ox + x * CELL, oy + y * CELL);
    drawFood(ox + g.food.x * CELL, oy + g.food.y * CELL, frame);
    if (g.goldenActive) drawGolden(ox + g.golden.x * CELL, oy + g.golden.y * CELL, frame, g.goldenLeft < 14);

    static Cell cells[MAXLEN];
    for (int i = 0; i < g.len; i++) cells[i] = g.cellFromTail(i);
    Cell h = g.head();
    int pdx = (g.food.x > h.x) - (g.food.x < h.x), pdy = (g.food.y > h.y) - (g.food.y < h.y);
    int fxd = g.food.x - h.x, fyd = g.food.y - h.y;
    bool mouth = (g.dir == RIGHT && fyd == 0 && fxd > 0 && fxd <= 3) || (g.dir == LEFT && fyd == 0 && fxd < 0 && fxd >= -3) ||
                 (g.dir == DOWN && fxd == 0 && fyd > 0 && fyd <= 3) || (g.dir == UP && fxd == 0 && fyd < 0 && fyd >= -3);
    drawSnake(cells, g.len, ox, oy, s.skin, frame, g.dir, v.mood, pdx, pdy, mouth);

    for (const auto &q : fx.p)
        if (q.life) { int px = q.x / 16, py = q.y / 16; drect(px, py, px + 1, py + 1, q.color); }
    for (const auto &r : fx.rg)
        if (r.life) { ringDraw(r.x, r.y, r.r, r.color); if (r.r > 4) ringDraw(r.x, r.y, r.r - 3, r.color); }
    for (const auto &t : fx.ft)
        if (t.life) dtext(t.x - tw(t.s) / 2, t.y, t.color, t.s);
    if (fx.flash > 0) drect(BOARD_X, BOARD_Y, BOARD_X + bw - 1, BOARD_Y + 3, C_WHITE);

    // ---- Barra superior
    drect(0, 0, 395, 14, SIDE);
    dtext(4, 3, TXT, MODE_NAME[g.mode]);
    dprint(300, 3, GOLD, "$%d", (int)s.coins);
    dprint(350, 3, DIM, "Nv%d", levelFromXp(s.xp));

    // ---- Panel lateral
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
    if (up > 0) drect(sx, 113, sx + up * 90 / UPGRADE_EVERY, 117, C_RGB(20, 12, 31));
    dtext(sx, 124, DIM, "ESCUDO");
    for (int i = 0; i < g.shield && i < 5; i++) { drect(sx + 44 + i * 9, 124, sx + 49 + i * 9, 131, C_RGB(6, 26, 31)); dpixel(sx + 46 + i * 9, 132, C_RGB(6, 26, 31)); }
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
    case MODE_ROCKS: dprint(sx, 138, DIM, "ROCAS %d", g.rockCount); break;
    default: break;
    }
    if (g.scorePct > 0) dprint(sx, 164, C_RGB(20, 12, 31), "PTS +%d%%", g.scorePct);
    if (g.slowMs > 0) dtext(sx, 176, C_RGB(10, 20, 31), "LENTO");
    if (frenzy) { if ((frame / 3) & 1) dtext(sx, 190, hueColor(frame * 10, 31), "FIEBRE x2!"); }
    else if (g.streak > 0) dprint(sx, 190, GOLD, "RACHA %d/3", g.streak);

    // ---- Barra inferior: fiebre / racha
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

void drawUpgrade(const Game &g, int sel, int frame) {
    drect(4, 40, 288, 200, C_BLACK);
    ctext(146, 46, GOLD, "ELIGE UNA MEJORA");
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
    dprint(40, 50, TXT, "Puntos: %d", (int)g.score);
    dprint(40, 62, DIM, "Largo %d   Combo max x%d", g.len, g.maxCombo);
    if (rr.record) { if ((frame / 4) & 1) dtext(40, 76, GOLD, "NUEVO RECORD!"); }
    else dprint(40, 76, DIM, "Record: %d", (int)s.best[g.mode]);
    dprint(40, 92, GOLD, "+%d monedas", (int)rr.coins + achCoins);
    if (rr.levelAfter > rr.levelBefore) {
        if ((frame / 3) & 1) dprint(40, 106, hueColor(frame * 8, 31), "SUBES A NIVEL %d!", rr.levelAfter);
    } else xpBar(40, 108, 150, s);
    int y = 122, shown = 0, total = popcount32(runAch);
    for (int i = 0; i < ACH_COUNT && shown < 3; i++)
        if ((runAch >> i) & 1) { dprint(40, y, C_RGB(20, 12, 31), "LOGRO: %s", ACH_NAME[i]); y += 12; shown++; }
    if (total > shown) dprint(40, y, C_RGB(20, 12, 31), "+%d logros mas", total - shown);
    ctext(146, 192, DIM, "EXE=otra vez   EXIT=menu");
    dupdate();
}

void drawShop(int tab, int sel, const SaveData &s, int frame, const char *msg) {
    dclear(BG);
    dtext(10, 6, tab == 0 ? GOOD : DIM, "SKINS");
    dtext(60, 6, tab == 1 ? GOOD : DIM, "ESTELAS");
    dprint(300, 6, GOLD, "$%d", (int)s.coins);
    int n = tab == 0 ? SKIN_COUNT : TRAIL_COUNT;
    for (int i = 0; i < n; i++) {
        int y = 26 + i * 20;
        const ItemInfo &it = tab == 0 ? SKINS[i] : TRAILS[i];
        bool own = tab == 0 ? ownsSkin(s, i) : ownsTrail(s, i);
        bool eq = tab == 0 ? (s.skin == i) : (s.trail == i);
        if (i == sel) drect(6, y - 4, 200, y + 14, C_RGB(3, 8, 7));
        dtext(14, y, i == sel ? TXT : DIM, it.name);
        if (eq) dtext(110, y, GOOD, "EQUIPADO");
        else if (own) dtext(110, y, DIM, "TUYO");
        else if (it.cost == NOT_BUYABLE) dtext(110, y, C_RGB(20, 12, 31), "SECRETO");
        else dprint(110, y, s.coins >= it.cost ? GOLD : C_RGB(20, 8, 8), "$%d", (int)it.cost);
    }
    int pskin = tab == 0 ? sel : s.skin;
    bool locked = tab == 0 && !ownsSkin(s, sel) && SKINS[sel].cost == NOT_BUYABLE;
    drawPreview(230, 60, locked ? 0 : pskin, frame);
    if (tab == 1) {
        int px = 236 + (frame * 2) % 80, py = 120 + WAVE[(frame / 2) & 15];
        uint16_t c = sel == 1 ? GOLD : (sel == 2 ? C_RGB(10, 28, 31) : hueColor(frame * 8, 31));
        if (sel > 0) for (int i = 0; i < 6; i++) drect(px - i * 6, py + (i & 1) * 3, px - i * 6 + 1, py + (i & 1) * 3 + 1, c);
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
        int x = 10 + col * 195, y = 28 + row * 22;
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
    int lv = levelFromXp(s.xp), y = 26;
    dprint(14, y, TXT, "Nivel %d  (XP %d)", lv, (int)s.xp); y += 14;
    dprint(14, y, TXT, "Monedas: %d", (int)s.coins); y += 14;
    dprint(14, y, TXT, "Partidas: %d", (int)s.games); y += 14;
    dprint(14, y, TXT, "Comida total: %d", (int)s.food); y += 14;
    dprint(14, y, TXT, "Muertes: %d", (int)s.deaths); y += 18;
    for (int i = 0; i < MODE_COUNT; i++) { dprint(14, y, DIM, "%s: %d", MODE_NAME[i], (int)s.best[i]); y += 13; }
    dtext(10, 210, DIM, "EXIT = volver");
    dupdate();
}

}
