#include "game.h"

namespace od {

static inline int cidx(int x, int y) { return y * BW + x; }
static const int8_t DX[4] = {0, 1, 0, -1};
static const int8_t DY[4] = {-1, 0, 1, 0};
static const int8_t DX8[8] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int8_t DY8[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
static inline int sgn(int v) { return (v > 0) - (v < 0); }
static inline int iabs(int v) { return v < 0 ? -v : v; }

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
    food.x = -1; food.y = -1;
    goldenActive = false; goldenLeft = 0; sp = Special(); bonusN = 0;
    frenzyLeft = 0; streak = 0; iceLeft = 0; shield = 0; slowMs = 0; comboBonus = 0; goldLevel = 0;
    scorePct = 0; coinPct = 0; bossDmg = 0; upgradePending = false;
    frenzyCount = 0; goldenEaten = 0; upgradesTaken = 0; shieldsUsed = 0; rocksSmashed = 0;
    eventsSurvived = 0; specialEaten = 0;
    dimension = 0; shiftTimer = SHIFT_PERIOD; timeLeftMs = TIME_START_MS; timeUp = false;
    evId = GEV_NONE; evWarn = 0; evLeft = 0; evTimer = 150 + (int)rng.range(100); lastEv = 0;
    boss = Boss(); for (auto &b : bullets) b.alive = false;
    stage = 1; bossRespawn = 20; bossesDefeated = 0; bossKillMask = 0;
    ev = Events();
    placeFood();
    if (mode == MODE_ROCKS || mode == MODE_3D) for (int i = 0; i < 3; i++) spawnRock(1);
}

void Game::queueDir(Dir d) {
    if (mirrorOn()) d = (Dir)((d + 2) & 3);
    Dir ref = qn ? q[qn - 1] : dir;
    if (d == ref || d == (Dir)((ref + 2) & 3)) return;
    if (qn < 2) q[qn++] = d;
}

void Game::queueTurn(int rel) {
    Dir ref = qn ? q[qn - 1] : dir;
    bool right = rel > 0;
    if (mirrorOn()) right = !right;
    Dir d = (Dir)((ref + (right ? 1 : 3)) & 3);
    if (qn < 2) q[qn++] = d;
}

// Celda ocupada por cualquier cosa (serpiente, roca, fruta, jefe...).
bool Game::taken(int x, int y) const {
    if (occ[cidx(x, y)] || rock[cidx(x, y)]) return true;
    if (food.x == x && food.y == y) return true;
    if (goldenActive && golden.x == x && golden.y == y) return true;
    if (sp.active && sp.c.x == x && sp.c.y == y) return true;
    for (int i = 0; i < bonusN; i++) if (bonus[i].x == x && bonus[i].y == y) return true;
    if (bossCell(x, y)) return true;
    return false;
}

bool Game::pickEmpty(Cell &out) {
    int n = 0;
    for (int pass = 0; pass < 2; pass++) {
        int k = pass ? (int)rng.range((uint32_t)n) : 0;
        for (int y = 0; y < BH; y++)
            for (int x = 0; x < BW; x++) {
                if (taken(x, y)) continue;
                if (!pass) { n++; continue; }
                if (k-- == 0) { out.x = (int8_t)x; out.y = (int8_t)y; return true; }
            }
        if (!pass && n == 0) return false;
    }
    return false;
}

bool Game::placeFood() {
    food.x = -1; food.y = -1;
    Cell c;
    if (!pickEmpty(c)) return false;
    food = c;
    return true;
}
void Game::spawnGolden() { Cell c; if (pickEmpty(c)) { golden = c; goldenActive = true; goldenLeft = 45; } }
void Game::spawnSpecial() {
    Cell c;
    if (sp.active || !pickEmpty(c)) return;
    int r = (int)rng.range(10);
    sp.type = r < 4 ? SP_ICE : (r < 7 ? SP_ROTTEN : SP_RAINBOW);
    sp.c = c; sp.active = true; sp.left = 60;
}

// Rocas justas: aisladas (sin vecinas en 8 direcciones), lejos de la cabeza.
bool Game::rockOk(int x, int y, Cell hd) const {
    if (taken(x, y)) return false;
    if (iabs(x - hd.x) < 4 && iabs(y - hd.y) < 4) return false;
    for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++) {
            int nx = x + i, ny = y + j;
            if (nx < 0 || ny < 0 || nx >= BW || ny >= BH) continue;
            if (rock[cidx(nx, ny)]) return false;
        }
    return true;
}
void Game::spawnRock(uint8_t kind) {
    if (rockCount >= MAX_ROCKS) return;
    Cell hd = body[headI];
    int n = 0;
    for (int y = 0; y < BH; y++) for (int x = 0; x < BW; x++) if (rockOk(x, y, hd)) n++;
    if (!n) return;
    int k = (int)rng.range((uint32_t)n);
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++)
            if (rockOk(x, y, hd) && k-- == 0) { rock[cidx(x, y)] = kind; rockCount++; return; }
}

void Game::die() { state = ST_DEAD; ev.died = true; }

void Game::shrinkTail(int n) {
    for (int k = 0; k < n && len > 3; k++) {
        Cell t = body[tailI];
        occ[cidx(t.x, t.y)] = 0;
        tailI = (tailI + 1) % MAXLEN; len--;
    }
}

int Game::intervalMs() const {
    int base = mode == MODE_TIME ? 110 : (mode == MODE_BOSS ? 140 : 150);
    int floorMs = mode == MODE_BOSS ? 75 : 60;
    int v = base - eaten * 3;
    if (v < floorMs) v = floorMs;
    v += slowMs + (iceLeft > 0 ? 45 : 0);
    if (turboOn()) v = v * 70 / 100;
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
    t.eatF = t.eatG = t.eatS = t.grow = t.hitBody = t.hitRock = t.hitBoss = false;
    t.bonusIdx = -1;
    if (t.wall) { t.fatal = true; return t; }
    t.eatF = (nx == food.x && ny == food.y);
    t.eatG = goldenActive && nx == golden.x && ny == golden.y;
    t.eatS = sp.active && nx == sp.c.x && ny == sp.c.y;
    for (int i = 0; i < bonusN; i++) if (bonus[i].x == nx && bonus[i].y == ny) t.bonusIdx = i;
    t.grow = t.eatF || t.eatG || (t.eatS && sp.type != SP_ROTTEN) || t.bonusIdx >= 0 || pendingGrow > 0;
    Cell tl = body[tailI];
    t.hitBody = occ[cidx(nx, ny)] && !(!t.grow && nx == tl.x && ny == tl.y);
    t.hitRock = rock[cidx(nx, ny)] != 0;
    t.hitBoss = bossCell(nx, ny);
    t.fatal = t.hitBody || t.hitBoss || (t.hitRock && frenzyLeft <= 0);
    return t;
}

void Game::rollOffers() {
    uint8_t pool[UP_COUNT]; int n = 0;
    for (int u = 0; u < UP_COUNT; u++) {
        if (u == UP_SHRINK && len < 9) continue;
        if (u == UP_SHIELD && shield >= 5) continue;
        if (u == UP_GOLD && goldLevel >= 3) continue;
        if (u == UP_SLOW && slowMs >= 56) continue;
        if (u == UP_DAMAGE && (mode != MODE_BOSS || bossDmg >= 3)) continue;
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
    case UP_SHRINK: shrinkTail(3); break;
    case UP_BOOST:  scorePct += 25; break;
    case UP_COINS:  coinPct += 25; break;
    case UP_DAMAGE: bossDmg++; break;
    }
    upgradesTaken++;
    upgradePending = false;
}

// ---------------------------------------------------------------- Eventos aleatorios
void Game::eventsStep() {
    if (mode == MODE_BOSS) return;
    if (evId == GEV_NONE) {
        if (--evTimer > 0) return;
        int e;
        do { e = 1 + (int)rng.range(GEV_COUNT - 1); } while (e == lastEv);
        evId = (uint8_t)e; lastEv = e; evWarn = EVENT_WARN; ev.eventWarn = true;
        return;
    }
    if (evWarn > 0) {
        if (--evWarn > 0) return;
        static const int DUR[GEV_COUNT] = {0, 90, 140, 100, 90, 100};
        evLeft = DUR[evId]; ev.eventStart = true;
        if (evId == GEV_RAIN) {
            for (int i = 0; i < MAX_BONUS; i++) { Cell c; if (!pickEmpty(c)) break; bonus[bonusN++] = c; }
        } else if (evId == GEV_QUAKE) {
            for (int i = 0; i < 6; i++) spawnRock(2);
        }
        return;
    }
    if (--evLeft > 0) return;
    if (evId == GEV_RAIN) bonusN = 0;
    if (evId == GEV_QUAKE)
        for (int i = 0; i < BW * BH; i++) if (rock[i] == 2) { rock[i] = 0; rockCount--; }
    evId = GEV_NONE; evTimer = 180 + (int)rng.range(120);
    eventsSurvived++; score += 50; ev.eventEnd = true;
}

// ---------------------------------------------------------------- Jefes
bool Game::rectFree(int x, int y) const {
    if (x < 1 || y < 1 || x + 3 > BW - 1 || y + 3 > BH - 1) return false;
    for (int j = 0; j < 3; j++)
        for (int i = 0; i < 3; i++) {
            int cx = x + i, cy = y + j;
            if (occ[cidx(cx, cy)] || rock[cidx(cx, cy)]) return false;
            if (food.x == cx && food.y == cy) return false;
            if (goldenActive && golden.x == cx && golden.y == cy) return false;
            if (sp.active && sp.c.x == cx && sp.c.y == cy) return false;
            for (int b = 0; b < bonusN; b++) if (bonus[b].x == cx && bonus[b].y == cy) return false;
        }
    return true;
}

void Game::spawnBoss() {
    Cell h = body[headI];
    for (int tries = 0; tries < 60; tries++) {
        int x = 1 + (int)rng.range(BW - 4), y = 1 + (int)rng.range(BH - 4);
        if (boss.type == 0 || ((stage - 1) % 3) == 0) y = 1 + (int)rng.range(3);
        if (!rectFree(x, y)) continue;
        if (iabs(x + 1 - h.x) < 7 && iabs(y + 1 - h.y) < 7) continue;
        boss = Boss();
        boss.active = true; boss.type = (uint8_t)((stage - 1) % 3);
        boss.maxhp = 6 + 2 * (stage - 1); if (boss.maxhp > 24) boss.maxhp = 24;
        boss.hp = boss.maxhp; boss.x = x; boss.y = y; boss.warn = 15; boss.cd = 20; boss.moveCd = 45; boss.dx = 1;
        if (boss.type == 0) { boss.y = 1 + (int)rng.range(3); if (!rectFree(boss.x, boss.y)) { boss.active = false; continue; } }
        ev.bossSpawn = true;
        return;
    }
    bossRespawn = 5;   // sin sitio libre: reintentar enseguida
}

void Game::defeatBoss() {
    boss.active = false;
    for (auto &b : bullets) b.alive = false;
    bossesDefeated++; bossKillMask |= 1u << boss.type;
    score += (uint32_t)(300 + 100 * stage);
    stage++; bossRespawn = 45; ev.bossDown = true;
    spawnGolden();
    if (!upgradePending) { rollOffers(); upgradePending = true; ev.upgrade = true; }
}

void Game::fire(int bx, int by, int dx, int dy) {
    if (bx < 0 || by < 0 || bx >= BW || by >= BH) return;
    Cell h = body[headI];
    if (iabs(bx - h.x) <= 2 && iabs(by - h.y) <= 2) return;   // nunca un disparo imposible de esquivar
    for (auto &b : bullets)
        if (!b.alive) { b.x = (int8_t)bx; b.y = (int8_t)by; b.dx = (int8_t)dx; b.dy = (int8_t)dy; b.alive = true; ev.bossShot = true; return; }
}

void Game::bossStep() {
    Cell h = body[headI];
    if (!boss.active) {
        if (--bossRespawn <= 0) spawnBoss();
    } else if (boss.warn > 0) {
        boss.warn--;
    } else {
        int cx = boss.x + 1, cy = boss.y + 1;
        bool phase2 = boss.hp * 2 <= boss.maxhp;
        int speedUp = stage / 3 > 5 ? 5 : stage / 3;
        if (boss.type == 0) {                                  // CENTINELA: patrulla y dispara en abanico
            if (ticks % 2 == 0) {
                int nx = boss.x + boss.dx;
                if (nx < 1 || nx > BW - 4 || !rectFree(nx, boss.y)) boss.dx = -boss.dx; else boss.x = nx;
            }
            if (--boss.cd <= 0) {
                boss.cd = (phase2 ? 9 : 14) - speedUp;
                int ax = sgn(h.x - cx);
                fire(cx, boss.y + 3, ax, 1);
                if (phase2) { fire(cx, boss.y + 3, ax - 1 < -1 ? -1 : ax - 1, 1); fire(cx, boss.y + 3, ax + 1 > 1 ? 1 : ax + 1, 1); }
            }
        } else if (boss.type == 1) {                            // ARAÑA: persigue y dispara en cruz
            if (ticks % 3 == 0) {
                int dxm = h.x - cx, dym = h.y - cy, sx = 0, sy = 0;
                if (iabs(dxm) >= iabs(dym)) sx = sgn(dxm); else sy = sgn(dym);
                if (rectFree(boss.x + sx, boss.y + sy)) { boss.x += sx; boss.y += sy; }
            }
            if (--boss.cd <= 0) {
                boss.cd = (phase2 ? 12 : 18) - speedUp;
                int step = phase2 ? 1 : 2;
                for (int d = 0; d < 8; d += step) fire(cx + DX8[d] * 2, cy + DY8[d] * 2, DX8[d], DY8[d]);
            }
        } else {                                                // REY: se teletransporta y lanza anillos
            if (--boss.moveCd <= 0) {
                boss.moveCd = 45;
                for (int tries = 0; tries < 30; tries++) {
                    int x = 1 + (int)rng.range(BW - 4), y = 1 + (int)rng.range(BH - 4);
                    if (!rectFree(x, y) || (iabs(x + 1 - h.x) < 6 && iabs(y + 1 - h.y) < 6)) continue;
                    boss.x = x; boss.y = y; break;
                }
            }
            if (--boss.cd <= 0) {
                boss.cd = (phase2 ? 18 : 26) - speedUp;
                for (int d = 0; d < 8; d++) fire(cx + DX8[d] * 2, cy + DY8[d] * 2, DX8[d], DY8[d]);
                if (phase2) fire(cx + 2 * sgn(h.x - cx), cy + 2 * sgn(h.y - cy), sgn(h.x - cx), sgn(h.y - cy));
            }
        }
    }
    // Balas: se comprueba antes y después de moverlas (evita atravesar la cabeza).
    for (auto &b : bullets) {
        if (!b.alive) continue;
        for (int pass = 0; pass < 2 && b.alive; pass++) {
            if (pass == 1) {
                b.x = (int8_t)(b.x + b.dx); b.y = (int8_t)(b.y + b.dy);
                if (b.x < 0 || b.y < 0 || b.x >= BW || b.y >= BH) { b.alive = false; break; }
            }
            if (b.x == h.x && b.y == h.y) {
                b.alive = false;
                if (shield > 0) { shield--; shieldsUsed++; ev.shielded = true; }
                else { die(); return; }
            }
        }
    }
}

// ---------------------------------------------------------------- Tick
void Game::step() {
    ev = Events();
    if (state != ST_PLAYING || upgradePending) return;
    ticks++; sinceEat++;

    if (mode == MODE_TIME) {
        timeLeftMs -= intervalMs();
        if (timeLeftMs <= 0) { timeLeftMs = 0; timeUp = true; state = ST_DEAD; ev.died = true; ev.timeUp = true; return; }
    }
    if (frenzyLeft > 0) frenzyLeft--;
    if (iceLeft > 0) iceLeft--;
    if (sp.active && --sp.left <= 0) sp.active = false;
    if (mode == MODE_SHIFT && --shiftTimer <= 0) {
        Cell h0 = body[headI];
        bool onBorder = h0.x == 0 || h0.y == 0 || h0.x == BW - 1 || h0.y == BH - 1;
        if (dimension == 1 && onBorder) shiftTimer = 6;
        else { dimension = 1 - dimension; shiftTimer = SHIFT_PERIOD; ev.shifted = true; }
    }
    eventsStep();

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
        if (!t.eatF && !t.eatG && !t.eatS && t.bonusIdx < 0) pendingGrow--;
    } else {
        Cell tl = body[tailI];
        occ[cidx(tl.x, tl.y)] = 0;
        tailI = (tailI + 1) % MAXLEN; len--;
    }
    headI = (headI + 1) % MAXLEN;
    body[headI].x = (int8_t)t.x; body[headI].y = (int8_t)t.y;
    occ[cidx(t.x, t.y)] = 1;
    len++;

    bool rotten = t.eatS && sp.type == SP_ROTTEN;
    if (rotten) {                                      // fruta podrida: castigo, sin combo ni puntos
        sp.active = false; specialEaten++;
        combo = 1; streak = 0; sinceEat = 999;
        shrinkTail(2);
        ev.rotten = true; ev.special = true; ev.at.x = (int8_t)t.x; ev.at.y = (int8_t)t.y;
    } else if (t.eatF || t.eatG || t.eatS || t.bonusIdx >= 0) {
        combo = (sinceEat <= comboWindow()) ? (combo < COMBO_MAX ? combo + 1 : COMBO_MAX) : 1;
        sinceEat = 0;
        if (combo > maxCombo) maxCombo = combo;
        int base = t.eatG ? 50 : (t.eatS ? 30 : 10);
        int mult = 1 + (frenzyLeft > 0 ? 1 : 0) + (turboOn() ? 1 : 0);
        int pts = base * combo * (100 + scorePct) / 100 * mult;
        score += (uint32_t)pts;
        eaten++;
        ev.ate = true; ev.golden = t.eatG; ev.gained = pts;
        ev.at.x = (int8_t)t.x; ev.at.y = (int8_t)t.y;
        if (t.eatG) { goldenActive = false; pendingGrow += 2; goldenEaten++; }
        if (t.eatS) {
            specialEaten++; ev.special = true;
            if (sp.type == SP_ICE) iceLeft = 50;
            else if (frenzyLeft < 35) { frenzyLeft = 35; ev.frenzy = true; }
            sp.active = false;
        }
        if (t.bonusIdx >= 0) { bonus[t.bonusIdx] = bonus[bonusN - 1]; bonusN--; }
        if (mode == MODE_TIME) { timeLeftMs += t.eatG ? 8000 : 4000; if (timeLeftMs > 99000) timeLeftMs = 99000; }

        if (combo == COMBO_MAX) streak++; else streak = 0;
        if (streak >= 3 && frenzyLeft <= 0) { frenzyLeft = FRENZY_TICKS; streak = 0; frenzyCount++; ev.frenzy = true; }

        if (mode == MODE_BOSS && boss.active && boss.warn <= 0) {
            int dmg = 1 + (combo >= 3 ? 1 : 0) + bossDmg + (frenzyLeft > 0 ? 1 : 0);
            boss.hp -= dmg; score += 25; ev.bossHit = true;
            if (boss.hp <= 0) defeatBoss();
        }
        if (t.eatF) {
            if (!placeFood()) { state = ST_WON; ev.won = true; return; }
            int den = 6 - 2 * goldLevel; if (den < 2) den = 2;
            if (!goldenActive && rng.range((uint32_t)den) == 0) spawnGolden();
            if (!sp.active && rng.range(8) == 0) spawnSpecial();
        }
        if (mode == MODE_ROCKS && eaten % 3 == 0) spawnRock(1);
        if (mode == MODE_3D && eaten % 4 == 0) spawnRock(1);
        if (!upgradePending && eaten % UPGRADE_EVERY == 0) { rollOffers(); upgradePending = true; ev.upgrade = true; }
    }
    if (goldenActive && --goldenLeft <= 0) goldenActive = false;
    if (mode == MODE_BOSS && state == ST_PLAYING) bossStep();
}

}
