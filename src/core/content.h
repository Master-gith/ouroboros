#pragma once
// Tablas de contenido (solo ASCII: la fuente de gint no dibuja acentos).
#include <stdint.h>
#include "game.h"
namespace od {

constexpr int SKIN_COUNT = 8, TRAIL_COUNT = 4, ACH_COUNT = 15;
constexpr int SECRET_SKIN = 7;
constexpr uint16_t NOT_BUYABLE = 0xFFFF;
constexpr int ACH_COLLECTOR = 13, ACH_SECRET = 14;

struct ItemInfo { const char *name; uint16_t cost; };

extern const ItemInfo SKINS[SKIN_COUNT];
extern const ItemInfo TRAILS[TRAIL_COUNT];
extern const char *const MODE_NAME[MODE_COUNT];
extern const char *const MODE_DESC[MODE_COUNT];
extern const uint8_t MODE_LEVEL[MODE_COUNT];
extern const char *const UPG_NAME[UP_COUNT];
extern const char *const UPG_D1[UP_COUNT];
extern const char *const UPG_D2[UP_COUNT];
extern const char *const ACH_NAME[ACH_COUNT];
extern const uint8_t ACH_REWARD[ACH_COUNT];

}
