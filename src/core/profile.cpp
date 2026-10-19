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

void encode(const SaveData &s, uint8_t out[SAVE_SIZE]) {
    memset(out, 0, SAVE_SIZE);
    put32(out + 0, SAVE_MAGIC);
    put32(out + 4, SAVE_VERSION);
    put32(out + 8, s.generation);
    put32(out + 12, s.coins);
    put32(out + 16, s.xp);
    put32(out + 20, s.best[0]);
    put32(out + 24, s.best[1]);
    put32(out + 28, s.games);
    put32(out + 32, s.food);
    put32(out + 36, s.deaths);
    put32(out + 40, s.unlocked);
    put32(out + 44, s.settings);
    put32(out + 60, crc32(out, 60));
}

bool decode(const uint8_t in[SAVE_SIZE], SaveData &s) {
    if (get32(in) != SAVE_MAGIC || get32(in + 4) != SAVE_VERSION) return false;
    if (get32(in + 60) != crc32(in, 60)) return false;
    const uint32_t LIM = 2000000000u;   // validación de plausibilidad
    SaveData t;
    t.generation = get32(in + 8);
    t.coins = get32(in + 12); t.xp = get32(in + 16);
    t.best[0] = get32(in + 20); t.best[1] = get32(in + 24);
    t.games = get32(in + 28); t.food = get32(in + 32); t.deaths = get32(in + 36);
    t.unlocked = get32(in + 40); t.settings = get32(in + 44);
    if (t.coins > LIM || t.xp > LIM || t.best[0] > LIM || t.best[1] > LIM) return false;
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
    // Alterna ranuras: si esta escritura falla, la ranura anterior sigue intacta.
    if (!wr((int)(n.generation & 1), buf)) return false;
    s = n;
    return true;
}

int levelFromXp(uint32_t xp) {
    uint32_t v = xp / 50, r = 0;
    while ((r + 1) * (r + 1) <= v) r++;
    return 1 + (int)r;
}

bool applyRun(SaveData &s, Mode m, uint32_t score, uint32_t food, bool won) {
    s.games++;
    s.food += food;
    if (!won) s.deaths++;
    s.xp += score;
    s.coins += score / 20 + (won ? 100 : 0);
    if (score > s.best[m]) { s.best[m] = score; return true; }
    return false;
}

}
