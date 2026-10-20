#include "content.h"
namespace od {

const ItemInfo SKINS[SKIN_COUNT] = {
    {"VERDE", 0}, {"NEON", 250}, {"FUEGO", 400}, {"HIELO", 400}, {"ORO", 600},
    {"GALAXIA", 800}, {"ARCOIRIS", 1200}, {"GLITCH", NOT_BUYABLE},
    {"FANTASMA", 900}, {"ROBOT", 1300}, {"ESQUELETO", 1600}, {"ELECTRICA", 2000},
    {"DRAGON", 2800}, {"GATO", 2200}, {"ARGOS", 3500},
};
const ItemInfo TRAILS[TRAIL_COUNT] = {
    {"NINGUNA", 0}, {"CHISPAS", 150}, {"BURBUJAS", 250}, {"ARCOIRIS", 500},
    {"FUEGO", 700}, {"CORAZONES", 900}, {"RAYOS", 1400}, {"ESTRELLAS", 2000},
};
const ItemInfo EATFX[EATFX_COUNT] = {
    {"NORMAL", 0}, {"EXPLOSION", 300}, {"ONDAS", 500}, {"CONFETI", 800}, {"RAYOS", 1200}, {"CORAZONES", 1600},
};
const char *const MODE_NAME[MODE_COUNT] = {"CLASSIC", "TORUS", "ROCAS", "DIMENSION", "CONTRARRELOJ", "JEFES", "3D"};
const char *const MODE_DESC[MODE_COUNT] = {
    "Paredes letales. Combos, dorada y mejoras.",
    "Sin paredes: los bordes se conectan.",
    "Aparecen rocas. En FIEBRE las rompes.",
    "Muros y portales se alternan. Atento!",
    "60 s. Cada fruta suma tiempo.",
    "Cada fruta hiere al jefe. Esquiva sus balas!",
    "Vista 3D. IZQ/DER giran hacia tu lado.",
};
const uint8_t MODE_LEVEL[MODE_COUNT] = {1, 1, 2, 3, 4, 6, 8};
const char *const UPG_NAME[UP_COUNT] = {"ESCUDO", "LENTO", "COMBO LARGO", "ORO+", "ENCOGER", "PUNTOS+25%", "MONEDAS+25%", "DANO JEFE"};
const char *const UPG_D1[UP_COUNT] = {"Esquiva 2", "Todo mas", "Combo dura", "Mas frutas", "Pierdes 3", "Cada fruta", "Mas monedas", "+1 dano a"};
const char *const UPG_D2[UP_COUNT] = {"choques", "lento", "mas tiempo", "doradas", "segmentos", "vale mas", "al terminar", "los jefes"};
const char *const ACH_NAME[ACH_COUNT] = {
    "PRIMER BOCADO", "COMBO X5", "FIEBRE", "FRUTA DORADA", "LARGO 25", "LARGO 50", "500 PUNTOS",
    "1500 PUNTOS", "3 MEJORAS", "ESCUDO USADO", "DESTRUCTOR", "RELOJERO", "DIMENSIONES",
    "COLECCIONISTA", "SECRETO",
    "PRIMER JEFE", "TRES JEFES", "SOBREVIVIENTE", "FRUTAS RARAS", "LARGO 80", "TURISTA 3D",
    "NIVEL 10", "8 SKINS", "ESTRELLA",
};
const uint8_t ACH_REWARD[ACH_COUNT] = {5, 20, 30, 15, 20, 40, 30, 60, 20, 10, 40, 40, 30, 25, 50,
                                       80, 150, 60, 50, 100, 80, 100, 100, 150};
const char *const EVENT_NAME[GEV_COUNT] = {"", "LLUVIA", "NIEBLA", "TURBO", "ESPEJO", "TERREMOTO"};
const char *const EVENT_HINT[GEV_COUNT] = {"", "Lluvia de frutas!", "Niebla: poca vision!", "Turbo: puntos x2!", "Espejo: controles al reves!", "Terremoto: rocas!"};
const char *const BOSS_NAME[3] = {"CENTINELA", "ARANA", "REY"};

}
