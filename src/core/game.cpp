#include "game.h"

namespace od {

static inline int cidx(int x, int y) { return y * BW + x; }

void Game::start(Mode m, uint32_t seed) {
    mode = m;
    seedUsed = seed;
    rng.seed(seed);
    for (int i = 0; i < BW * BH; i++) occ[i] = 0;
    len = 3; tailI = 0; headI = 2;
    int sx = BW / 2, sy = BH / 2;
    for (int i = 0; i < 3; i++) {
        body[i].x = (int8_t)(sx - 2 + i);
        body[i].y = (int8_t)sy;
        occ[cidx(sx - 2 + i, sy)] = 1;
    }
    dir = RIGHT; qn = 0;
    state = ST_PLAYING;
    score = 0; combo = 1; sinceEat = 999; eaten = 0; ticks = 0; pendingGrow = 0;
    goldenActive = false; goldenLeft = 0;
    ev = Events();
    placeFood();
}

void Game::queueDir(Dir d) {
    Dir ref = qn ? q[qn - 1] : dir;
    if (d == ref || d == (Dir)((ref + 2) & 3)) return;
    if (qn < 2) q[qn++] = d;
}

// Elige una celda libre uniformemente al azar (nunca sobre la serpiente ni la comida).
bool Game::pickEmpty(Cell &out, bool avoidFood, bool avoidGolden) {
    int n = 0;
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++) {
            if (occ[cidx(x, y)]) continue;
            if (avoidFood && x == food.x && y == food.y) continue;
            if (avoidGolden && goldenActive && x == golden.x && y == golden.y) continue;
            n++;
        }
    if (n == 0) return false;
    int k = (int)rng.range((uint32_t)n);
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++) {
            if (occ[cidx(x, y)]) continue;
            if (avoidFood && x == food.x && y == food.y) continue;
            if (avoidGolden && goldenActive && x == golden.x && y == golden.y) continue;
            if (k-- == 0) { out.x = (int8_t)x; out.y = (int8_t)y; return true; }
        }
    return false;
}

bool Game::placeFood() {
    Cell c;
    if (!pickEmpty(c, false, true)) return false;
    food = c;
    return true;
}

void Game::spawnGolden() {
    Cell c;
    if (pickEmpty(c, true, false)) { golden = c; goldenActive = true; goldenLeft = 45; }
}

void Game::die() { state = ST_DEAD; ev.died = true; }

int Game::intervalMs() const {
    int v = 150 - eaten * 3;
    return v < 60 ? 60 : v;
}

void Game::step() {
    ev = Events();
    if (state != ST_PLAYING) return;
    ticks++; sinceEat++;

    if (qn) { dir = q[0]; q[0] = q[1]; qn--; }
    static const int8_t DX[4] = {0, 1, 0, -1};
    static const int8_t DY[4] = {-1, 0, 1, 0};

    Cell h = body[headI];
    int nx = h.x + DX[dir], ny = h.y + DY[dir];
    if (mode == MODE_TORUS) {
        nx = (nx + BW) % BW; ny = (ny + BH) % BH;
    } else if (nx < 0 || ny < 0 || nx >= BW || ny >= BH) {
        die(); return;
    }

    bool eatFood = (nx == food.x && ny == food.y);
    bool eatGold = goldenActive && nx == golden.x && ny == golden.y;
    bool grow = eatFood || eatGold || pendingGrow > 0;

    Cell t = body[tailI];
    // La cola se aparta en este mismo tick si no crecemos: entrar en su celda es legal.
    bool hitsBody = occ[cidx(nx, ny)] && !(!grow && nx == t.x && ny == t.y);
    if (hitsBody) { die(); return; }

    if (grow) {
        if (!eatFood && !eatGold) pendingGrow--;
    } else {
        occ[cidx(t.x, t.y)] = 0;
        tailI = (tailI + 1) % MAXLEN;
        len--;
    }
    headI = (headI + 1) % MAXLEN;
    body[headI].x = (int8_t)nx; body[headI].y = (int8_t)ny;
    occ[cidx(nx, ny)] = 1;
    len++;

    if (eatFood || eatGold) {
        combo = (sinceEat <= COMBO_WINDOW) ? (combo < COMBO_MAX ? combo + 1 : COMBO_MAX) : 1;
        sinceEat = 0;
        score += (uint32_t)((eatGold ? 50 : 10) * combo);
        eaten++;
        ev.ate = true; ev.golden = eatGold;
        ev.at.x = (int8_t)nx; ev.at.y = (int8_t)ny;
        if (eatGold) { goldenActive = false; pendingGrow += 2; }
        if (eatFood) {
            if (!placeFood()) { state = ST_WON; ev.won = true; return; }
            if (!goldenActive && rng.range(6) == 0) spawnGolden();
        }
    }
    if (goldenActive && --goldenLeft <= 0) goldenActive = false;
}

}
