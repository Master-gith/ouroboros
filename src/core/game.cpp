#include "game.h"

namespace od {

static inline int cidx(int x, int y) { return y * BW + x; }
static const int8_t DX[4] = {0, 1, 0, -1};
static const int8_t DY[4] = {-1, 0, 1, 0};

void Game::start(Mode m, uint32_t seed) {
    mode = m; seedUsed = seed; rng.seed(seed);
    for (int i = 0; i < BW * BH; i++) { occ[i] = 0; rock[i] = 0; }
    rockCount = 0;
    len = 3; tailI = 0; headI = 2;
    int sx = BW / 2, sy = BH / 2;
    for (int i = 0; i < 3; i++) {
        body[i].x = (int8_t)(sx - 2 + i); body[i].y = (int8_t)sy;
        occ[cidx(sx - 2 + i, sy)] = 1;
    }
    dir = RIGHT; qn = 0; state = ST_PLAYING;
    score = 0; combo = 1; maxCombo = 1; sinceEat = 999; eaten = 0; ticks = 0; pendingGrow = 0;
    goldenActive = false; goldenLeft = 0;
    frenzyLeft = 0; streak = 0; shield = 0; slowMs = 0; comboBonus = 0; goldLevel = 0;
    scorePct = 0; coinPct = 0; upgradePending = false;
    frenzyCount = 0; goldenEaten = 0; upgradesTaken = 0; shieldsUsed = 0; rocksSmashed = 0;
    dimension = 0; shiftTimer = SHIFT_PERIOD; timeLeftMs = TIME_START_MS; timeUp = false;
    ev = Events();
    placeFood();
    if (mode == MODE_ROCKS) for (int i = 0; i < 3; i++) spawnRock();
}

void Game::queueDir(Dir d) {
    Dir ref = qn ? q[qn - 1] : dir;
    if (d == ref || d == (Dir)((ref + 2) & 3)) return;
    if (qn < 2) q[qn++] = d;
}

bool Game::pickEmpty(Cell &out, bool avoidFood, bool avoidGolden) {
    int n = 0;
    for (int pass = 0; pass < 2; pass++) {
        int k = pass ? (int)rng.range((uint32_t)n) : 0;
        for (int y = 0; y < BH; y++)
            for (int x = 0; x < BW; x++) {
                if (occ[cidx(x, y)] || rock[cidx(x, y)]) continue;
                if (avoidFood && x == food.x && y == food.y) continue;
                if (avoidGolden && goldenActive && x == golden.x && y == golden.y) continue;
                if (!pass) { n++; continue; }
                if (k-- == 0) { out.x = (int8_t)x; out.y = (int8_t)y; return true; }
            }
        if (!pass && n == 0) return false;
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

// Rocas justas: aisladas (sin vecinas en 8 direcciones), lejos de la cabeza y nunca
// sobre comida/serpiente. Así nunca forman muros que encierren zonas.
bool Game::rockOk(int x, int y, Cell hd) const {
    if (occ[cidx(x, y)] || rock[cidx(x, y)]) return false;
    if (x == food.x && y == food.y) return false;
    if (goldenActive && x == golden.x && y == golden.y) return false;
    int dx = x - hd.x, dy = y - hd.y;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    if ((dx > dy ? dx : dy) < 4) return false;
    for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++) {
            int nx = x + i, ny = y + j;
            if (nx < 0 || ny < 0 || nx >= BW || ny >= BH) continue;
            if (rock[cidx(nx, ny)]) return false;
        }
    return true;
}

void Game::spawnRock() {
    if (rockCount >= MAX_ROCKS) return;
    Cell hd = body[headI];
    int n = 0;
    for (int y = 0; y < BH; y++) for (int x = 0; x < BW; x++) if (rockOk(x, y, hd)) n++;
    if (!n) return;
    int k = (int)rng.range((uint32_t)n);
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++)
            if (rockOk(x, y, hd) && k-- == 0) { rock[cidx(x, y)] = 1; rockCount++; return; }
}

void Game::die() { state = ST_DEAD; ev.died = true; }

int Game::intervalMs() const {
    int base = (mode == MODE_TIME) ? 110 : 150;
    int v = base - eaten * 3;
    if (v < 60) v = 60;
    v += slowMs;
    return v;
}

Game::Tgt Game::target(Dir d) const {
    Tgt t;
    Cell h = body[headI];
    int nx = h.x + DX[d], ny = h.y + DY[d];
    t.wall = false;
    if (wrapping()) { nx = (nx + BW) % BW; ny = (ny + BH) % BH; }
    else if (nx < 0 || ny < 0 || nx >= BW || ny >= BH) t.wall = true;
    t.x = nx; t.y = ny;
    t.eatF = t.eatG = t.grow = t.hitBody = t.hitRock = false;
    if (t.wall) { t.fatal = true; return t; }
    t.eatF = (nx == food.x && ny == food.y);
    t.eatG = goldenActive && nx == golden.x && ny == golden.y;
    t.grow = t.eatF || t.eatG || pendingGrow > 0;
    Cell tl = body[tailI];
    t.hitBody = occ[cidx(nx, ny)] && !(!t.grow && nx == tl.x && ny == tl.y);
    t.hitRock = rock[cidx(nx, ny)] != 0;
    t.fatal = t.hitBody || (t.hitRock && frenzyLeft <= 0);
    return t;
}

void Game::rollOffers() {
    uint8_t pool[UP_COUNT]; int n = 0;
    for (int u = 0; u < UP_COUNT; u++) {
        if (u == UP_SHRINK && len < 9) continue;
        if (u == UP_SHIELD && shield >= 5) continue;
        if (u == UP_GOLD && goldLevel >= 3) continue;
        if (u == UP_SLOW && slowMs >= 56) continue;
        pool[n++] = (uint8_t)u;
    }
    for (int i = n - 1; i > 0; i--) {
        int j = (int)rng.range((uint32_t)(i + 1));
        uint8_t tmp = pool[i]; pool[i] = pool[j]; pool[j] = tmp;
    }
    for (int i = 0; i < 3; i++) offer[i] = pool[i];
}

void Game::chooseUpgrade(int i) {
    if (!upgradePending || i < 0 || i > 2) return;
    switch (offer[i]) {
    case UP_SHIELD: shield += 2; if (shield > 5) shield = 5; break;
    case UP_SLOW:   slowMs += 14; break;
    case UP_COMBO:  comboBonus += 12; break;
    case UP_GOLD:   goldLevel++; break;
    case UP_SHRINK:
        for (int k = 0; k < 3 && len > 3; k++) {
            Cell t = body[tailI];
            occ[cidx(t.x, t.y)] = 0;
            tailI = (tailI + 1) % MAXLEN; len--;
        }
        break;
    case UP_BOOST:  scorePct += 25; break;
    case UP_COINS:  coinPct += 25; break;
    }
    upgradesTaken++;
    upgradePending = false;
}

void Game::step() {
    ev = Events();
    if (state != ST_PLAYING || upgradePending) return;
    ticks++; sinceEat++;

    if (mode == MODE_TIME) {
        timeLeftMs -= intervalMs();
        if (timeLeftMs <= 0) { timeLeftMs = 0; timeUp = true; state = ST_DEAD; ev.died = true; ev.timeUp = true; return; }
    }
    if (frenzyLeft > 0) frenzyLeft--;
    if (mode == MODE_SHIFT && --shiftTimer <= 0) {
        Cell h0 = body[headI];
        bool onBorder = h0.x == 0 || h0.y == 0 || h0.x == BW - 1 || h0.y == BH - 1;
        if (dimension == 1 && onBorder) shiftTimer = 6;   // no encierres al jugador contra un muro
        else { dimension = 1 - dimension; shiftTimer = SHIFT_PERIOD; ev.shifted = true; }
    }

    if (qn) { dir = q[0]; q[0] = q[1]; qn--; }
    Tgt t = target(dir);
    if (t.fatal) {
        if (shield > 0) {
            Dir a = (Dir)((dir + 1) & 3), b = (Dir)((dir + 3) & 3);
            Tgt ta = target(a), tb = target(b);
            bool fa = !ta.fatal, fb = !tb.fatal;
            if (!fa && !fb) { die(); return; }
            Dir pick = (fa && fb) ? (rng.range(2) ? a : b) : (fa ? a : b);
            shield--; shieldsUsed++; ev.shielded = true;
            dir = pick; qn = 0; t = target(pick);
        } else { die(); return; }
    }

    if (t.hitRock) { rock[cidx(t.x, t.y)] = 0; rockCount--; rocksSmashed++; score += 20; ev.smashed = true; }
    if (t.grow) {
        if (!t.eatF && !t.eatG) pendingGrow--;
    } else {
        Cell tl = body[tailI];
        occ[cidx(tl.x, tl.y)] = 0;
        tailI = (tailI + 1) % MAXLEN; len--;
    }
    headI = (headI + 1) % MAXLEN;
    body[headI].x = (int8_t)t.x; body[headI].y = (int8_t)t.y;
    occ[cidx(t.x, t.y)] = 1;
    len++;

    if (t.eatF || t.eatG) {
        combo = (sinceEat <= comboWindow()) ? (combo < COMBO_MAX ? combo + 1 : COMBO_MAX) : 1;
        sinceEat = 0;
        if (combo > maxCombo) maxCombo = combo;
        int pts = (t.eatG ? 50 : 10) * combo;
        pts = pts * (100 + scorePct) / 100;
        if (frenzyLeft > 0) pts *= 2;
        score += (uint32_t)pts;
        eaten++;
        ev.ate = true; ev.golden = t.eatG; ev.gained = pts;
        ev.at.x = (int8_t)t.x; ev.at.y = (int8_t)t.y;
        if (t.eatG) { goldenActive = false; pendingGrow += 2; goldenEaten++; }
        if (mode == MODE_TIME) { timeLeftMs += t.eatG ? 8000 : 4000; if (timeLeftMs > 99000) timeLeftMs = 99000; }

        if (combo == COMBO_MAX) streak++; else streak = 0;
        if (streak >= 3 && frenzyLeft <= 0) { frenzyLeft = FRENZY_TICKS; streak = 0; frenzyCount++; ev.frenzy = true; }

        if (t.eatF) {
            if (!placeFood()) { state = ST_WON; ev.won = true; return; }
            int den = 6 - 2 * goldLevel; if (den < 2) den = 2;
            if (!goldenActive && rng.range((uint32_t)den) == 0) spawnGolden();
        }
        if (mode == MODE_ROCKS && eaten % 3 == 0) spawnRock();
        if (eaten % UPGRADE_EVERY == 0) { rollOffers(); upgradePending = true; ev.upgrade = true; }
    }
    if (goldenActive && --goldenLeft <= 0) goldenActive = false;
}

}
