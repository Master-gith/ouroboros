// Renderizado 2D con gint (fx-CG50, VRAM 396x224). SIN COMPILAR NI PROBAR EN HARDWARE.
#include "render.h"
#include <gint/display.h>

namespace od {

static const uint16_t BG    = C_RGB(1, 1, 3);
static const uint16_t BOARD = C_RGB(2, 3, 5);
static const uint16_t DOT   = C_RGB(4, 6, 9);
static const uint16_t TXT   = C_RGB(26, 30, 31);
static const uint16_t DIM   = C_RGB(12, 15, 20);

// Tabla de onda (±3 px) para animar el título.
static const int8_t WAVE[16] = {0, 1, 2, 3, 3, 2, 1, 0, 0, -1, -2, -3, -3, -2, -1, 0};

void Fx::clear() { for (auto &q : p) q.life = 0; flash = 0; }

void Fx::burst(int px, int py, uint16_t color, int count) {
    for (auto &q : p) {
        if (count <= 0) break;
        if (q.life) continue;
        q.x = (int16_t)(px * 16); q.y = (int16_t)(py * 16);
        q.vx = (int16_t)((int)rng.range(41) - 20); q.vy = (int16_t)((int)rng.range(41) - 20);
        q.life = (uint8_t)(10 + rng.range(10)); q.color = color;
        count--;
    }
}

void Fx::update() {
    for (auto &q : p) {
        if (!q.life) continue;
        q.x += q.vx; q.y += q.vy; q.life--;
    }
    if (flash > 0) flash--;
}

static void centered(int y, uint16_t col, const char *s, int approxW) {
    dtext((396 - approxW) / 2, y, col, s);
}

void drawTitle(int frame, LoadResult lr) {
    dclear(BG);
    const char *t = "OUROBOROS";
    for (int i = 0; t[i]; i++) {
        char ch[2] = {t[i], 0};
        int y = 70 + WAVE[(frame / 2 + i * 2) & 15];
        uint16_t c = C_RGB(6 + ((i * 3 + frame / 3) & 15), 28, 12 + (i * 2));
        dtext(110 + i * 20, y, c, ch);
    }
    centered(100, DIM, "SNAKE DIMENSION", 100);
    if ((frame / 15) & 1) centered(160, TXT, "Pulsa EXE", 60);
    if (lr == LOAD_RECOVERED) centered(190, C_RGB(31, 24, 4), "Guardado recuperado de copia", 170);
    if (lr == LOAD_CORRUPT) centered(190, C_RGB(31, 8, 4), "Guardado danado: perfil reiniciado", 190);
    dupdate();
}

void drawMenu(int sel, int frame, const SaveData &s) {
    dclear(BG);
    dtext(10, 8, C_RGB(10, 31, 14), "OUROBOROS");
    dprint(250, 8, DIM, "Nv %d  $%d", levelFromXp(s.xp), (int)s.coins);
    static const char *items[3] = {"CLASSIC", "TORUS", "STATS"};
    static const char *desc[3] = {"Paredes letales. Combos y comida dorada.",
                                  "Sin paredes: los bordes se conectan.",
                                  "Tu historial y progreso."};
    for (int i = 0; i < 3; i++) {
        int y = 60 + i * 32;
        bool on = (i == sel);
        if (on) drect(40, y - 6, 355, y + 20, C_RGB(3, 8, 6));
        dtext(60 + (on ? ((frame / 4) & 3) : 0), y, on ? C_RGB(12, 31, 16) : DIM, items[i]);
        if (i < 2) dprint(200, y, DIM, "Mejor: %d", (int)s.best[i]);
    }
    dtext(10, 200, TXT, desc[sel]);
    dtext(10, 212, DIM, "ARRIBA/ABAJO  EXE=jugar  MENU=salir");
    dupdate();
}

static void cellRect(int cx, int cy, int inset, uint16_t col) {
    int x = BOARD_X + cx * CELL, y = BOARD_Y + cy * CELL;
    drect(x + inset, y + inset, x + CELL - 1 - inset, y + CELL - 1 - inset, col);
}

void drawGame(const Game &g, const Fx &fx, int frame, const SaveData &s, bool paused) {
    dclear(BG);
    int bw = BW * CELL, bh = BH * CELL;
    drect(BOARD_X, BOARD_Y, BOARD_X + bw - 1, BOARD_Y + bh - 1, BOARD);
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++)
            dpixel(BOARD_X + x * CELL + 3, BOARD_Y + y * CELL + 3, DOT);
    // Borde: sólido y rojo en Classic; cian en Torus (los bordes se conectan).
    uint16_t bc = (g.mode == MODE_TORUS) ? C_RGB(4, 24, 30) : C_RGB(28, 6, 6);
    drect(BOARD_X, BOARD_Y, BOARD_X + bw - 1, BOARD_Y, bc);
    drect(BOARD_X, BOARD_Y + bh - 1, BOARD_X + bw - 1, BOARD_Y + bh - 1, bc);
    drect(BOARD_X, BOARD_Y, BOARD_X, BOARD_Y + bh - 1, bc);
    drect(BOARD_X + bw - 1, BOARD_Y, BOARD_X + bw - 1, BOARD_Y + bh - 1, bc);

    // Comida con pulso
    int pulse = ((frame / 4) & 3);
    cellRect(g.food.x, g.food.y, 1 + (pulse == 1 || pulse == 2 ? 0 : 1), C_RGB(31, 6, 8));
    if (g.goldenActive) {
        bool blink = g.goldenLeft < 14 && ((frame / 3) & 1);
        if (!blink) cellRect(g.golden.x, g.golden.y, pulse & 1, C_RGB(31, 28, 3));
    }
    // Serpiente: degradado cola -> cabeza
    for (int i = 0; i < g.len; i++) {
        Cell c = g.cellFromTail(i);
        int t = (i * 31) / (g.len > 1 ? g.len - 1 : 1);
        cellRect(c.x, c.y, 1, C_RGB(2 + t / 8, 8 + t * 2 / 3, 4 + t / 3));
    }
    Cell h = g.head();
    cellRect(h.x, h.y, 0, C_RGB(18, 31, 20));
    int hx = BOARD_X + h.x * CELL, hy = BOARD_Y + h.y * CELL;
    dpixel(hx + 2, hy + 2, C_BLACK); dpixel(hx + 5, hy + 2, C_BLACK);

    for (const auto &q : fx.p)
        if (q.life) {
            int px = q.x / 16, py = q.y / 16;
            drect(px, py, px + 1, py + 1, q.color);
        }
    if (fx.flash > 0) drect(BOARD_X, BOARD_Y, BOARD_X + bw - 1, BOARD_Y + 2, C_WHITE);

    // HUD
    drect(0, 0, 395, 14, C_RGB(2, 3, 6));
    dprint(4, 3, TXT, "%s", g.mode == MODE_TORUS ? "TORUS" : "CLASSIC");
    dprint(300, 0 + 16, TXT, "PUNTOS");
    dprint(300, 28, C_RGB(12, 31, 16), "%d", (int)g.score);
    dprint(300, 46, DIM, "MEJOR");
    dprint(300, 58, TXT, "%d", (int)(g.score > s.best[g.mode] ? g.score : s.best[g.mode]));
    dprint(300, 76, DIM, "LARGO");
    dprint(300, 88, TXT, "%d", g.len);
    dprint(300, 106, DIM, "COMBO");
    dprint(300, 118, g.combo > 1 ? C_RGB(31, 24, 4) : TXT, "x%d", g.combo);
    dprint(300, 136, DIM, "NIVEL %d", levelFromXp(s.xp));
    if (paused) {
        drect(80, 90, 215, 140, C_BLACK);
        dtext(100, 98, TXT, "PAUSA");
        dtext(90, 114, DIM, "EXE seguir  EXIT salir");
    }
    dupdate();
}

void drawOver(const Game &g, const SaveData &s, bool record) {
    drect(60, 60, 235, 170, C_BLACK);
    dtext(100, 68, g.state == ST_WON ? C_RGB(31, 28, 3) : C_RGB(31, 8, 6),
          g.state == ST_WON ? "TABLERO COMPLETO" : "GAME OVER");
    dprint(80, 92, TXT, "Puntos: %d", (int)g.score);
    dprint(80, 106, TXT, "Largo:  %d", g.len);
    if (record) dtext(80, 124, C_RGB(31, 24, 4), "NUEVO RECORD!");
    dprint(80, 140, DIM, "Monedas: %d", (int)s.coins);
    dtext(80, 154, DIM, "EXE=otra vez  EXIT=menu");
    dupdate();
}

void drawStats(const SaveData &s) {
    dclear(BG);
    dtext(10, 8, C_RGB(10, 31, 14), "ESTADISTICAS");
    int y = 40;
    dprint(20, y, TXT, "Nivel %d  (XP %d)", levelFromXp(s.xp), (int)s.xp); y += 16;
    dprint(20, y, TXT, "Monedas: %d", (int)s.coins); y += 16;
    dprint(20, y, TXT, "Partidas: %d", (int)s.games); y += 16;
    dprint(20, y, TXT, "Comida total: %d", (int)s.food); y += 16;
    dprint(20, y, TXT, "Muertes: %d", (int)s.deaths); y += 16;
    dprint(20, y, TXT, "Mejor Classic: %d", (int)s.best[0]); y += 16;
    dprint(20, y, TXT, "Mejor Torus: %d", (int)s.best[1]); y += 24;
    dtext(20, y, DIM, "EXIT = volver");
    dupdate();
}

}
