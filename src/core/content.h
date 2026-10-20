#pragma once
// Tablas de contenido (solo ASCII: la fuente de gint no dibuja acentos).
#include <stdint.h>
#include "game.h"
namespace od {

constexpr const char *VERSION_TEXT = "v0.3";
constexpr int SKIN_COUNT = 15, TRAIL_COUNT = 8, EATFX_COUNT = 6, ACH_COUNT = 24;
constexpr int SECRET_SKIN = 7;
constexpr uint16_t NOT_BUYABLE = 0xFFFF;
constexpr int ACH_COLLECTOR = 13, ACH_SECRET = 14;

// Bits de "unlocked": skins 0-14, estelas 16-23, efectos al comer 24-29.
constexpr int trailBit(int i) { return 16 + i; }
constexpr int fxBit(int i) { return 24 + i; }
constexpr uint32_t SKIN_MASK = 0x7FFFu;

struct ItemInfo { const char *name; uint16_t cost; };

extern const ItemInfo SKINS[SKIN_COUNT];
extern const ItemInfo TRAILS[TRAIL_COUNT];
extern const ItemInfo EATFX[EATFX_COUNT];
extern const char *const MODE_NAME[MODE_COUNT];
extern const char *const MODE_DESC[MODE_COUNT];
extern const uint8_t MODE_LEVEL[MODE_COUNT];
extern const char *const UPG_NAME[UP_COUNT];
extern const char *const UPG_D1[UP_COUNT];
extern const char *const UPG_D2[UP_COUNT];
extern const char *const ACH_NAME[ACH_COUNT];
extern const uint8_t ACH_REWARD[ACH_COUNT];
extern const char *const EVENT_NAME[GEV_COUNT];
extern const char *const EVENT_HINT[GEV_COUNT];
extern const char *const BOSS_NAME[3];

}
