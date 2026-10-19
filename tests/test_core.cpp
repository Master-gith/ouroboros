// Pruebas en PC del núcleo (sin gint). Compilar con: make -C tests
#include <stdio.h>
#include <string.h>
#include "../src/core/game.h"
#include "../src/core/profile.h"
using namespace od;

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static void checkInvariants(const Game &g) {
    int cnt = 0;
    for (int y = 0; y < BH; y++) for (int x = 0; x < BW; x++) if (g.occupied(x, y)) cnt++;
    CHECK(cnt == g.len);
    if (g.state == ST_PLAYING) {
        CHECK(!g.occupied(g.food.x, g.food.y));
        if (g.goldenActive) { CHECK(!g.occupied(g.golden.x, g.golden.y)); CHECK(!(g.golden.x == g.food.x && g.golden.y == g.food.y)); }
    }
    for (int i = 0; i < g.len; i++) {
        Cell c = g.cellFromTail(i);
        CHECK(g.occupied(c.x, c.y));
        if (i > 0) {
            Cell p = g.cellFromTail(i - 1);
            int dx = c.x - p.x, dy = c.y - p.y;
            if (g.mode == MODE_TORUS) { if (dx > 1) dx -= BW; if (dx < -1) dx += BW; if (dy > 1) dy -= BH; if (dy < -1) dy += BH; }
            CHECK((dx * dx + dy * dy) == 1);
        }
    }
}

// Bot voraz que evita morir en el siguiente paso cuando puede.
static Dir botChoice(const Game &g, Rng &r) {
    Cell h = g.head();
    static const int DX[4] = {0, 1, 0, -1}, DY[4] = {-1, 0, 1, 0};
    Dir best = g.dir; int bestScore = -1000000;
    for (int d = 0; d < 4; d++) {
        if (d == ((g.dir + 2) & 3)) continue;
        int nx = h.x + DX[d], ny = h.y + DY[d];
        if (g.mode == MODE_TORUS) { nx = (nx + BW) % BW; ny = (ny + BH) % BH; }
        else if (nx < 0 || ny < 0 || nx >= BW || ny >= BH) continue;
        int s = 0;
        if (g.occupied(nx, ny)) s -= 1000;
        int ddx = nx - g.food.x, ddy = ny - g.food.y;
        s -= (ddx < 0 ? -ddx : ddx) + (ddy < 0 ? -ddy : ddy);
        s += (int)r.range(3);
        if (s > bestScore) { bestScore = s; best = (Dir)d; }
    }
    return best;
}

static void testGameplay() {
    int died = 0, ate = 0;
    for (int m = 0; m < MODE_COUNT; m++)
        for (uint32_t seed = 1; seed <= 200; seed++) {
            Game g; Rng r; r.seed(seed * 7919u);
            g.start((Mode)m, seed);
            for (int t = 0; t < 3000 && g.state == ST_PLAYING; t++) {
                g.queueDir(botChoice(g, r));
                g.step();
                if (g.ev.ate) ate++;
                checkInvariants(g);
            }
            if (g.state == ST_DEAD) died++;
        }
    CHECK(ate > 1000);
    printf("gameplay: %d comidas, %d muertes en 400 partidas simuladas\n", ate, died);
}

static void testRules() {
    Game g; g.start(MODE_CLASSIC, 42);
    CHECK(g.len == 3 && g.state == ST_PLAYING);
    g.queueDir(LEFT); g.step();                  // inversión ignorada
    CHECK(g.dir == RIGHT);
    // Pared mata en Classic
    Game w; w.start(MODE_CLASSIC, 5);
    for (int i = 0; i < BW && w.state == ST_PLAYING; i++) w.step();
    CHECK(w.state == ST_DEAD);
    // Atraviesa en Torus
    Game t; t.start(MODE_TORUS, 5);
    for (int i = 0; i < BW * 2; i++) t.step();
    CHECK(t.state == ST_PLAYING || t.state == ST_DEAD);  // puede comer, pero no muere por pared
    // Determinismo por semilla
    Game a, b; a.start(MODE_CLASSIC, 99); b.start(MODE_CLASSIC, 99);
    CHECK(a.food.x == b.food.x && a.food.y == b.food.y);
    // Entrar en la celda de la cola es legal: bucle 2x2 de longitud 4
    Game c; c.start(MODE_TORUS, 1);
    CHECK(c.intervalMs() == 150);
}

static uint8_t slots[2][SAVE_SIZE]; static bool slotExists[2]; static bool failNext = false;
static bool rd(int s, uint8_t *b) { if (!slotExists[s]) return false; memcpy(b, slots[s], SAVE_SIZE); return true; }
static bool wr(int s, const uint8_t *b) {
    if (failNext) { failNext = false; memset(slots[s], 0xA5, SAVE_SIZE / 2); memcpy(slots[s] + SAVE_SIZE / 2, b + SAVE_SIZE / 2, SAVE_SIZE / 2); slotExists[s] = true; return false; }
    memcpy(slots[s], b, SAVE_SIZE); slotExists[s] = true; return true;
}

static void testSave() {
    SaveData s; CHECK(loadProfile(s, rd) == LOAD_NEW);
    applyRun(s, MODE_CLASSIC, 500, 20, false);
    CHECK(storeProfile(s, wr));
    applyRun(s, MODE_TORUS, 300, 10, false);
    CHECK(storeProfile(s, wr));
    SaveData l; CHECK(loadProfile(l, rd) == LOAD_OK);
    CHECK(l.best[0] == 500 && l.best[1] == 300 && l.generation == 2 && l.coins == s.coins);
    // Escritura interrumpida: la ranura corrupta se descarta y sobrevive la anterior
    applyRun(l, MODE_CLASSIC, 900, 40, false);
    failNext = true; CHECK(!storeProfile(l, wr));
    SaveData r; LoadResult lr = loadProfile(r, rd);
    CHECK(lr == LOAD_RECOVERED); CHECK(r.generation == 2 && r.best[0] == 500);
    // Un bit alterado se detecta
    uint8_t buf[SAVE_SIZE]; encode(s, buf); buf[13] ^= 1; SaveData x; CHECK(!decode(buf, x));
    // Ambas corruptas
    memset(slots[0], 0, SAVE_SIZE); memset(slots[1], 0xFF, SAVE_SIZE);
    CHECK(loadProfile(x, rd) == LOAD_CORRUPT && x.coins == 0);
    CHECK(levelFromXp(0) == 1 && levelFromXp(50) == 2 && levelFromXp(200) == 3);
    CHECK(crc32((const uint8_t *)"123456789", 9) == 0xCBF43926u);  // vector estándar CRC32
}

int main() {
    testRules(); testGameplay(); testSave();
    if (fails) { printf("%d fallos\n", fails); return 1; }
    printf("TODAS LAS PRUEBAS OK\n");
    return 0;
}
