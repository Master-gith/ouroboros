#pragma once
// Perfil persistente: serialización con CRC32 y doble ranura (A/B) para no perder
// datos si la escritura se interrumpe. Lógica pura: la E/S real se inyecta.
#include <stdint.h>
#include "game.h"

namespace od {

constexpr int SAVE_SIZE = 64;      // par, requerido por el sistema de ficheros de la fx-CG
constexpr uint32_t SAVE_MAGIC = 0x3152554Fu;  // "OUR1"
constexpr uint32_t SAVE_VERSION = 1;

struct SaveData {
    uint32_t generation = 0;
    uint32_t coins = 0, xp = 0;
    uint32_t best[MODE_COUNT] = {0, 0};
    uint32_t games = 0, food = 0, deaths = 0;
    uint32_t unlocked = 0;     // bitmask reservado para skins/modos
    uint32_t settings = 0;
};

enum LoadResult { LOAD_NEW = 0, LOAD_OK = 1, LOAD_RECOVERED = 2, LOAD_CORRUPT = 3 };

uint32_t crc32(const uint8_t *d, int n);
void encode(const SaveData &s, uint8_t out[SAVE_SIZE]);
bool decode(const uint8_t in[SAVE_SIZE], SaveData &s);

typedef bool (*ReadFn)(int slot, uint8_t *buf64);        // true si leyó 64 bytes
typedef bool (*WriteFn)(int slot, const uint8_t *buf64); // true si escribió OK

LoadResult loadProfile(SaveData &s, ReadFn rd);
bool storeProfile(SaveData &s, WriteFn wr);

int levelFromXp(uint32_t xp);
bool applyRun(SaveData &s, Mode m, uint32_t score, uint32_t food, bool won);

}
