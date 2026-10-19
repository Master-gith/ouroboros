#pragma once
// Perfil persistente v2: CRC32, doble ranura A/B, migración desde v1.
#include <stdint.h>
#include "game.h"
#include "content.h"

namespace od {

constexpr int SAVE_SIZE = 64;
constexpr uint32_t SAVE_MAGIC = 0x3152554Fu;  // "OUR1"
constexpr uint32_t SAVE_VERSION = 2;
constexpr uint32_t DEFAULT_UNLOCKED = (1u << 0) | (1u << 8);  // skin 0 y estela 0

struct SaveData {
    uint32_t generation = 0;
    uint32_t coins = 0, xp = 0;
    uint32_t games = 0, food = 0, deaths = 0;
    uint32_t unlocked = DEFAULT_UNLOCKED;   // bits 0-7 skins, 8-11 estelas
    uint32_t achievements = 0;
    uint32_t best[MODE_COUNT] = {};
    uint8_t skin = 0, trail = 0;
};

struct RunResult { bool record = false; uint32_t coins = 0; int levelBefore = 1, levelAfter = 1; };

enum LoadResult { LOAD_NEW = 0, LOAD_OK = 1, LOAD_RECOVERED = 2, LOAD_CORRUPT = 3 };
enum BuyResult { BUY_OK = 0, BUY_NO_COINS = 1, BUY_NOT_BUYABLE = 2, BUY_OWNED = 3 };

uint32_t crc32(const uint8_t *d, int n);
void encode(const SaveData &s, uint8_t out[SAVE_SIZE]);
bool decode(const uint8_t in[SAVE_SIZE], SaveData &s);

typedef bool (*ReadFn)(int slot, uint8_t *buf64);
typedef bool (*WriteFn)(int slot, const uint8_t *buf64);
LoadResult loadProfile(SaveData &s, ReadFn rd);
bool storeProfile(SaveData &s, WriteFn wr);

int levelFromXp(uint32_t xp);
uint32_t xpForLevel(int level);
RunResult applyRun(SaveData &s, Mode m, uint32_t score, uint32_t food, bool won, int coinPct);

// Logros: check devuelve los NUEVOS; claim los marca y suma monedas (devuelve monedas).
uint32_t checkAchievements(const SaveData &s, const Game &g);
int claimAchievements(SaveData &s, uint32_t mask);

// Tienda
bool ownsSkin(const SaveData &s, int i);
bool ownsTrail(const SaveData &s, int i);
BuyResult buySkin(SaveData &s, int i);
BuyResult buyTrail(SaveData &s, int i);
int popcount32(uint32_t v);

}
