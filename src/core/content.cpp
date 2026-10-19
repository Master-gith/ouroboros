#include "content.h"
namespace od {

const ItemInfo SKINS[SKIN_COUNT] = {
    {"VERDE", 0}, {"NEON", 60}, {"FUEGO", 100}, {"HIELO", 100},
    {"ORO", 180}, {"GALAXIA", 220}, {"ARCOIRIS", 300}, {"GLITCH", NOT_BUYABLE},
};
const ItemInfo TRAILS[TRAIL_COUNT] = {
    {"NINGUNA", 0}, {"CHISPAS", 80}, {"BURBUJAS", 120}, {"ARCOIRIS", 200},
};
const char *const MODE_NAME[MODE_COUNT] = {"CLASSIC", "TORUS", "ROCAS", "DIMENSION", "CONTRARRELOJ"};
const char *const MODE_DESC[MODE_COUNT] = {
    "Paredes letales. Combos, dorada y mejoras.",
    "Sin paredes: los bordes se conectan.",
    "Aparecen rocas. En FIEBRE las rompes.",
    "Muros y portales se alternan. Atento!",
    "60 s. Cada fruta suma tiempo.",
};
const uint8_t MODE_LEVEL[MODE_COUNT] = {1, 1, 2, 3, 4};
const char *const UPG_NAME[UP_COUNT] = {"ESCUDO", "TIEMPO LENTO", "COMBO LARGO", "ORO+", "ENCOGER", "PUNTOS +25%", "MONEDAS +25%"};
const char *const UPG_D1[UP_COUNT] = {"Esquiva 2 choques", "Todo va un poco", "El combo dura", "Mas frutas", "Pierdes 3", "Cada fruta vale", "Mas monedas al"};
const char *const UPG_D2[UP_COUNT] = {"y gira sola", "mas despacio", "mas tiempo", "doradas", "segmentos", "mas puntos", "acabar la partida"};
const char *const ACH_NAME[ACH_COUNT] = {
    "PRIMER BOCADO", "COMBO X5", "FIEBRE", "FRUTA DORADA", "LARGO 25", "LARGO 50", "500 PUNTOS",
    "1500 PUNTOS", "3 MEJORAS", "ESCUDO USADO", "DESTRUCTOR", "RELOJERO", "DIMENSIONES",
    "COLECCIONISTA", "SECRETO",
};
const uint8_t ACH_REWARD[ACH_COUNT] = {5, 20, 30, 15, 20, 40, 30, 60, 20, 10, 40, 40, 30, 25, 50};

}
