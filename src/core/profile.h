#pragma once
// Perfil persistente v3: 96 bytes, CRC32, doble ranura A/B, migración desde v1/v2.
#include <stdint.h>
#include "game.h"
#include "content.h"

namespace od {

constexpr int SAVE_SIZE = 96;       // formato v3 (ficheros nuevos OURA3/OURB3)
constexpr int LEGACY_SIZE = 64;     // formatos v1/v2 (ficheros antiguos, solo lectura)
constexpr uint32_t SAVE_MAGIC = 0x3152554Fu;  // "OUR1"
constexpr uint32_t SAVE_VERSION = 3;
constexpr uint32_t DEFAULT_UNLOCKED = (1u << 0) | (1u << 16) | (1u << 24);

struct SaveData {
    uint32_t generation = 0;
    uint32_t coins = 0, xp = 0;
    uint32_t games = 0, food = 0, deaths = 0;
    uint32_t unlocked = DEFAULT_UNLOCKED;
    uint32_t achievements = 0;
    uint32_t best[MODE_COUNT] = {};
    uint32_t bossKills = 0, bestStage = 0, eventsTotal = 0;
    uint8_t skin = 0, trail = 0, eatfx = 0;
};

struct RunResult { bool record = false; uint32_t coins = 0; int levelBefore = 1, levelAfter = 1; };

enum LoadResult { LOAD_NEW = 0, LOAD_OK = 1, LOAD_RECOVERED = 2, LOAD_CORRUPT = 3 };
enum BuyResult { BUY_OK = 0, BUY_NO_COINS = 1, BUY_NOT_BUYABLE = 2, BUY_OWNED = 3 };

uint32_t crc32(const uint8_t *d, int n);
void encode(const SaveData &s, uint8_t out[SAVE_SIZE]);
bool decode(const uint8_t *in, int len, SaveData &s);

// Lectura: slots 0,1 = ficheros v3 (hasta 96 bytes); slots 2,3 = antiguos (hasta 64).
// Devuelve los bytes leídos (0 si no existe).
typedef int (*ReadFn)(int slot, uint8_t *buf, int maxlen);
typedef bool (*WriteFn)(int slot, const uint8_t *buf, int len);
LoadResult loadProfile(SaveData &s, ReadFn rd);
bool storeProfile(SaveData &s, WriteFn wr);

int levelFromXp(uint32_t xp);
uint32_t xpForLevel(int level);
RunResult applyRun(SaveData &s, Mode m, uint32_t score, uint32_t food, bool won, int coinPct, int bossKills);

uint32_t checkAchievements(const SaveData &s, const Game &g);
uint32_t checkMeta(const SaveData &s);          // logros que dependen del perfil
int claimAchievements(SaveData &s, uint32_t mask);

bool ownsSkin(const SaveData &s, int i);
bool ownsTrail(const SaveData &s, int i);
bool ownsEatFx(const SaveData &s, int i);
BuyResult buySkin(SaveData &s, int i);
BuyResult buyTrail(SaveData &s, int i);
BuyResult buyEatFx(SaveData &s, int i);
int popcount32(uint32_t v);

}
