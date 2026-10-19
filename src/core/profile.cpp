#include "profile.h"
#include <string.h>

namespace od {

uint32_t crc32(const uint8_t *d, int n) {
    uint32_t c = 0xFFFFFFFFu;
    for (int i = 0; i < n; i++) {
        c ^= d[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}
static void put32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// Formato v2 (64 bytes): 0 magic | 4 u16 version | 6 skin | 7 trail | 8 gen | 12 coins |
// 16 xp | 20 games | 24 food | 28 deaths | 32 unlocked | 36 achievements |
// 40..59 best[5] | 60 crc32 de los bytes 0..59.
void encode(const SaveData &s, uint8_t out[SAVE_SIZE]) {
    memset(out, 0, SAVE_SIZE);
    put32(out + 0, SAVE_MAGIC);
    out[4] = (uint8_t)SAVE_VERSION; out[5] = 0; out[6] = s.skin; out[7] = s.trail;
    put32(out + 8, s.generation); put32(out + 12, s.coins); put32(out + 16, s.xp);
    put32(out + 20, s.games); put32(out + 24, s.food); put32(out + 28, s.deaths);
    put32(out + 32, s.unlocked); put32(out + 36, s.achievements);
    for (int i = 0; i < MODE_COUNT; i++) put32(out + 40 + 4 * i, s.best[i]);
    put32(out + 60, crc32(out, 60));
}

bool decode(const uint8_t in[SAVE_SIZE], SaveData &s) {
    if (get32(in) != SAVE_MAGIC) return false;
    int ver = in[4] | (in[5] << 8);
    if (ver != 1 && ver != 2) return false;
    if (get32(in + 60) != crc32(in, 60)) return false;
    SaveData t;
    t.generation = get32(in + 8); t.coins = get32(in + 12); t.xp = get32(in + 16);
    if (ver == 2) {
        t.skin = in[6]; t.trail = in[7];
        t.games = get32(in + 20); t.food = get32(in + 24); t.deaths = get32(in + 28);
        t.unlocked = get32(in + 32); t.achievements = get32(in + 36);
        for (int i = 0; i < MODE_COUNT; i++) t.best[i] = get32(in + 40 + 4 * i);
    } else {   // v1: best[2] en 20/24, games 28, food 32, deaths 36
        t.best[0] = get32(in + 20); t.best[1] = get32(in + 24);
        t.games = get32(in + 28); t.food = get32(in + 32); t.deaths = get32(in + 36);
        t.unlocked = 0; t.achievements = 0;
    }
    const uint32_t LIM = 2000000000u;
    if (t.coins > LIM || t.xp > LIM) return false;
    for (int i = 0; i < MODE_COUNT; i++) if (t.best[i] > LIM) return false;
    t.unlocked |= DEFAULT_UNLOCKED;
    t.achievements &= (1u << ACH_COUNT) - 1;
    if (t.skin >= SKIN_COUNT || !((t.unlocked >> t.skin) & 1)) t.skin = 0;
    if (t.trail >= TRAIL_COUNT || !((t.unlocked >> (8 + t.trail)) & 1)) t.trail = 0;
    s = t;
    return true;
}

LoadResult loadProfile(SaveData &s, ReadFn rd) {
    uint8_t buf[2][SAVE_SIZE];
    SaveData d[2];
    bool exists[2], valid[2];
    for (int i = 0; i < 2; i++) {
        exists[i] = rd(i, buf[i]);
        valid[i] = exists[i] && decode(buf[i], d[i]);
    }
    if (!exists[0] && !exists[1]) { s = SaveData(); return LOAD_NEW; }
    if (!valid[0] && !valid[1]) { s = SaveData(); return LOAD_CORRUPT; }
    int pick;
    if (valid[0] && valid[1]) pick = (d[1].generation > d[0].generation) ? 1 : 0;
    else pick = valid[0] ? 0 : 1;
    s = d[pick];
    bool bad = (exists[0] && !valid[0]) || (exists[1] && !valid[1]);
    return bad ? LOAD_RECOVERED : LOAD_OK;
}

bool storeProfile(SaveData &s, WriteFn wr) {
    SaveData n = s;
    n.generation = s.generation + 1;
    uint8_t buf[SAVE_SIZE];
    encode(n, buf);
    if (!wr((int)(n.generation & 1), buf)) return false;
    s = n;
    return true;
}

// Nivel n empieza en 50*(n-1)^2 XP.
uint32_t xpForLevel(int level) { return level <= 1 ? 0u : 50u * (uint32_t)(level - 1) * (uint32_t)(level - 1); }
int levelFromXp(uint32_t xp) {
    uint32_t v = xp / 50, r = 0;
    while ((r + 1) * (r + 1) <= v) r++;
    return 1 + (int)r;
}

RunResult applyRun(SaveData &s, Mode m, uint32_t score, uint32_t food, bool won, int coinPct) {
    RunResult r;
    r.levelBefore = levelFromXp(s.xp);
    s.games++; s.food += food;
    if (!won) s.deaths++;
    r.record = score > s.best[m];
    if (r.record) s.best[m] = score;
    s.xp += score + (won ? 200u : 0u);
    uint32_t c = food + score / 15 + (r.record && score > 0 ? 25u : 0u) + (won ? 100u : 0u);
    c = c * (uint32_t)(100 + coinPct) / 100u;
    s.coins += c;
    r.coins = c;
    r.levelAfter = levelFromXp(s.xp);
    return r;
}

uint32_t checkAchievements(const SaveData &s, const Game &g) {
    uint32_t m = 0;
    if (g.eaten >= 1) m |= 1u << 0;
    if (g.maxCombo >= COMBO_MAX) m |= 1u << 1;
    if (g.frenzyCount >= 1) m |= 1u << 2;
    if (g.goldenEaten >= 1) m |= 1u << 3;
    if (g.len >= 25) m |= 1u << 4;
    if (g.len >= 50) m |= 1u << 5;
    if (g.score >= 500) m |= 1u << 6;
    if (g.score >= 1500) m |= 1u << 7;
    if (g.upgradesTaken >= 3) m |= 1u << 8;
    if (g.shieldsUsed >= 1) m |= 1u << 9;
    if (g.rocksSmashed >= 3) m |= 1u << 10;
    if (g.mode == MODE_TIME && g.eaten >= 30) m |= 1u << 11;
    if (g.mode == MODE_SHIFT && g.eaten >= 10) m |= 1u << 12;
    return m & ~s.achievements;
}

int claimAchievements(SaveData &s, uint32_t mask) {
    mask &= ~s.achievements;
    int coins = 0;
    for (int i = 0; i < ACH_COUNT; i++)
        if ((mask >> i) & 1) coins += ACH_REWARD[i];
    s.achievements |= mask;
    s.coins += (uint32_t)coins;
    return coins;
}

bool ownsSkin(const SaveData &s, int i) { return i >= 0 && i < SKIN_COUNT && ((s.unlocked >> i) & 1); }
bool ownsTrail(const SaveData &s, int i) { return i >= 0 && i < TRAIL_COUNT && ((s.unlocked >> (8 + i)) & 1); }

static BuyResult buyBit(SaveData &s, int bit, uint16_t cost) {
    if ((s.unlocked >> bit) & 1) return BUY_OWNED;
    if (cost == NOT_BUYABLE) return BUY_NOT_BUYABLE;
    if (s.coins < cost) return BUY_NO_COINS;
    s.coins -= cost;
    s.unlocked |= 1u << bit;
    return BUY_OK;
}
BuyResult buySkin(SaveData &s, int i) { return (i < 0 || i >= SKIN_COUNT) ? BUY_NOT_BUYABLE : buyBit(s, i, SKINS[i].cost); }
BuyResult buyTrail(SaveData &s, int i) { return (i < 0 || i >= TRAIL_COUNT) ? BUY_NOT_BUYABLE : buyBit(s, 8 + i, TRAILS[i].cost); }

int popcount32(uint32_t v) { int n = 0; while (v) { n += v & 1; v >>= 1; } return n; }

}
