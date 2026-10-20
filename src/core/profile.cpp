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

// v3 (96 bytes): 0 magic | 4 u16 version | 6 skin(5 bits)+eatfx(3 bits) | 7 trail | 8 gen | 12 coins |
// 16 xp | 20 games | 24 food | 28 deaths | 32 unlocked | 36 achievements | 40..67 best[7] |
// 68 bossKills | 72 bestStage | 76 eventsTotal | 80..91 reservado | 92 crc32 de 0..91.
void encode(const SaveData &s, uint8_t out[SAVE_SIZE]) {
    memset(out, 0, SAVE_SIZE);
    put32(out + 0, SAVE_MAGIC);
    out[4] = (uint8_t)SAVE_VERSION; out[5] = 0;
    out[6] = (uint8_t)((s.skin & 31) | ((s.eatfx & 7) << 5)); out[7] = s.trail;
    put32(out + 8, s.generation); put32(out + 12, s.coins); put32(out + 16, s.xp);
    put32(out + 20, s.games); put32(out + 24, s.food); put32(out + 28, s.deaths);
    put32(out + 32, s.unlocked); put32(out + 36, s.achievements);
    for (int i = 0; i < MODE_COUNT; i++) put32(out + 40 + 4 * i, s.best[i]);
    put32(out + 68, s.bossKills); put32(out + 72, s.bestStage); put32(out + 76, s.eventsTotal);
    put32(out + 92, crc32(out, 92));
}

bool decode(const uint8_t *in, int len, SaveData &s) {
    if (len < LEGACY_SIZE || get32(in) != SAVE_MAGIC) return false;
    int ver = in[4] | (in[5] << 8);
    SaveData t;
    if (ver == 3) {
        if (len < SAVE_SIZE || get32(in + 92) != crc32(in, 92)) return false;
        t.skin = in[6] & 31; t.eatfx = (in[6] >> 5) & 7; t.trail = in[7];
        t.generation = get32(in + 8); t.coins = get32(in + 12); t.xp = get32(in + 16);
        t.games = get32(in + 20); t.food = get32(in + 24); t.deaths = get32(in + 28);
        t.unlocked = get32(in + 32); t.achievements = get32(in + 36);
        for (int i = 0; i < MODE_COUNT; i++) t.best[i] = get32(in + 40 + 4 * i);
        t.bossKills = get32(in + 68); t.bestStage = get32(in + 72); t.eventsTotal = get32(in + 76);
    } else if (ver == 1 || ver == 2) {
        if (get32(in + 60) != crc32(in, 60)) return false;
        t.generation = get32(in + 8); t.coins = get32(in + 12); t.xp = get32(in + 16);
        if (ver == 2) {   // v2: estelas en bits 8-11 -> ahora bits 16-19
            t.skin = in[6]; t.trail = in[7];
            t.games = get32(in + 20); t.food = get32(in + 24); t.deaths = get32(in + 28);
            uint32_t u = get32(in + 32);
            t.unlocked = (u & 0xFFu) | (((u >> 8) & 0xFu) << 16);
            t.achievements = get32(in + 36);
            for (int i = 0; i < 5; i++) t.best[i] = get32(in + 40 + 4 * i);
        } else {          // v1
            t.best[0] = get32(in + 20); t.best[1] = get32(in + 24);
            t.games = get32(in + 28); t.food = get32(in + 32); t.deaths = get32(in + 36);
            t.unlocked = 0;
        }
    } else return false;

    const uint32_t LIM = 2000000000u;
    if (t.coins > LIM || t.xp > LIM) return false;
    for (int i = 0; i < MODE_COUNT; i++) if (t.best[i] > LIM) return false;
    t.unlocked |= DEFAULT_UNLOCKED;
    t.achievements &= (1u << ACH_COUNT) - 1;
    if (t.skin >= SKIN_COUNT || !((t.unlocked >> t.skin) & 1)) t.skin = 0;
    if (t.trail >= TRAIL_COUNT || !((t.unlocked >> trailBit(t.trail)) & 1)) t.trail = 0;
    if (t.eatfx >= EATFX_COUNT || !((t.unlocked >> fxBit(t.eatfx)) & 1)) t.eatfx = 0;
    s = t;
    return true;
}

LoadResult loadProfile(SaveData &s, ReadFn rd) {
    uint8_t buf[SAVE_SIZE];
    SaveData d[4];
    bool exists[4] = {false, false, false, false}, valid[4] = {false, false, false, false};
    for (int i = 0; i < 4; i++) {
        if (i >= 2 && (valid[0] || valid[1])) break;     // lo antiguo solo si no hay v3 válido
        memset(buf, 0, sizeof buf);
        int n = rd(i, buf, i < 2 ? SAVE_SIZE : LEGACY_SIZE);
        exists[i] = n > 0;
        valid[i] = exists[i] && decode(buf, n, d[i]);
    }
    bool anyExists = exists[0] || exists[1] || exists[2] || exists[3];
    bool bad = (exists[0] && !valid[0]) || (exists[1] && !valid[1]);
    int pick = -1;
    if (valid[0] || valid[1]) {
        if (valid[0] && valid[1]) pick = (d[1].generation > d[0].generation) ? 1 : 0;
        else pick = valid[0] ? 0 : 1;
    } else if (valid[2] || valid[3]) {
        if (valid[2] && valid[3]) pick = (d[3].generation > d[2].generation) ? 3 : 2;
        else pick = valid[2] ? 2 : 3;
    }
    if (pick >= 0) { s = d[pick]; return bad ? LOAD_RECOVERED : LOAD_OK; }
    s = SaveData();
    return anyExists ? LOAD_CORRUPT : LOAD_NEW;
}

bool storeProfile(SaveData &s, WriteFn wr) {
    SaveData n = s;
    n.generation = s.generation + 1;
    uint8_t buf[SAVE_SIZE];
    encode(n, buf);
    if (!wr((int)(n.generation & 1), buf, SAVE_SIZE)) return false;
    s = n;
    return true;
}

// Nivel n empieza en 120*(n-1)^2 XP: ~190 XP por partida -> nivel 10 tras ~50 partidas.
uint32_t xpForLevel(int level) { return level <= 1 ? 0u : 120u * (uint32_t)(level - 1) * (uint32_t)(level - 1); }
int levelFromXp(uint32_t xp) {
    uint32_t v = xp / 120, r = 0;
    while ((r + 1) * (r + 1) <= v) r++;
    return 1 + (int)r;
}

RunResult applyRun(SaveData &s, Mode m, uint32_t score, uint32_t food, bool won, int coinPct, int bossKills) {
    RunResult r;
    r.levelBefore = levelFromXp(s.xp);
    s.games++; s.food += food;
    if (!won) s.deaths++;
    s.bossKills += (uint32_t)bossKills;
    r.record = score > s.best[m];
    if (r.record) s.best[m] = score;
    s.xp += score / 6 + food * 2 + (won ? 300u : 0u);
    uint32_t c = food / 2 + score / 60 + (r.record && score > 0 ? 10u : 0u) + (won ? 60u : 0u) + (uint32_t)bossKills * 15u;
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
    if (g.bossesDefeated >= 1) m |= 1u << 15;
    if (g.bossesDefeated >= 3) m |= 1u << 16;
    if (g.eventsSurvived >= 3) m |= 1u << 17;
    if (g.specialEaten >= 5) m |= 1u << 18;
    if (g.len >= 80) m |= 1u << 19;
    if (g.mode == MODE_3D && g.score >= 300) m |= 1u << 20;
    if (g.score >= 3000) m |= 1u << 23;
    return m & ~s.achievements;
}

uint32_t checkMeta(const SaveData &s) {
    uint32_t m = 0;
    int skins = popcount32(s.unlocked & SKIN_MASK);
    if (skins >= 4) m |= 1u << ACH_COLLECTOR;
    if (levelFromXp(s.xp) >= 10) m |= 1u << 21;
    if (skins >= 8) m |= 1u << 22;
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
bool ownsTrail(const SaveData &s, int i) { return i >= 0 && i < TRAIL_COUNT && ((s.unlocked >> trailBit(i)) & 1); }
bool ownsEatFx(const SaveData &s, int i) { return i >= 0 && i < EATFX_COUNT && ((s.unlocked >> fxBit(i)) & 1); }

static BuyResult buyBit(SaveData &s, int bit, uint16_t cost) {
    if ((s.unlocked >> bit) & 1) return BUY_OWNED;
    if (cost == NOT_BUYABLE) return BUY_NOT_BUYABLE;
    if (s.coins < cost) return BUY_NO_COINS;
    s.coins -= cost;
    s.unlocked |= 1u << bit;
    return BUY_OK;
}
BuyResult buySkin(SaveData &s, int i) { return (i < 0 || i >= SKIN_COUNT) ? BUY_NOT_BUYABLE : buyBit(s, i, SKINS[i].cost); }
BuyResult buyTrail(SaveData &s, int i) { return (i < 0 || i >= TRAIL_COUNT) ? BUY_NOT_BUYABLE : buyBit(s, trailBit(i), TRAILS[i].cost); }
BuyResult buyEatFx(SaveData &s, int i) { return (i < 0 || i >= EATFX_COUNT) ? BUY_NOT_BUYABLE : buyBit(s, fxBit(i), EATFX[i].cost); }

int popcount32(uint32_t v) { int n = 0; while (v) { n += v & 1; v >>= 1; } return n; }

}
