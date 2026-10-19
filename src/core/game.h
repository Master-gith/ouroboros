#pragma once
// Núcleo del juego v0.2: lógica pura, SIN dependencias de gint (se prueba en PC).
#include <stdint.h>
#include "rng.h"

namespace od {

constexpr int BW = 29;            // columnas (celdas de 10 px)
constexpr int BH = 20;            // filas
constexpr int MAXLEN = BW * BH;
constexpr int COMBO_BASE_WINDOW = 28;
constexpr int COMBO_MAX = 5;
constexpr int UPGRADE_EVERY = 5;  // frutas entre mejoras
constexpr int SHIFT_PERIOD = 100; // ticks entre cambios de dimensión
constexpr int SHIFT_WARN = 16;
constexpr int MAX_ROCKS = 16;
constexpr int FRENZY_TICKS = 70;
constexpr int TIME_START_MS = 60000;

enum Dir : uint8_t { UP = 0, RIGHT = 1, DOWN = 2, LEFT = 3 };
enum Mode : uint8_t { MODE_CLASSIC = 0, MODE_TORUS, MODE_ROCKS, MODE_SHIFT, MODE_TIME, MODE_COUNT };
enum State : uint8_t { ST_PLAYING = 0, ST_DEAD = 1, ST_WON = 2 };
enum Upgrade : uint8_t { UP_SHIELD = 0, UP_SLOW, UP_COMBO, UP_GOLD, UP_SHRINK, UP_BOOST, UP_COINS, UP_COUNT };

struct Cell { int8_t x, y; };

struct Events {
    bool ate = false, golden = false, died = false, won = false;
    bool shielded = false, smashed = false, frenzy = false, upgrade = false;
    bool shifted = false, timeUp = false;
    int gained = 0;
    Cell at = {0, 0};
};

class Game {
public:
    void start(Mode m, uint32_t seed);
    void queueDir(Dir d);
    void step();
    int  intervalMs() const;
    void chooseUpgrade(int i);       // 0..2, solo si upgradePending

    Cell cellFromTail(int i) const { return body[(tailI + i) % MAXLEN]; }
    Cell head() const { return body[headI]; }
    bool wrapping() const { return mode == MODE_TORUS || (mode == MODE_SHIFT && dimension == 1); }
    bool rockAt(int x, int y) const { return rock[y * BW + x] != 0; }
    bool occupied(int x, int y) const { return occ[y * BW + x] != 0; }
    int  comboWindow() const { return COMBO_BASE_WINDOW + comboBonus; }
    int  comboPct() const { int r = comboWindow() - sinceEat; return r <= 0 ? 0 : r * 100 / comboWindow(); }
    bool shiftWarn() const { return mode == MODE_SHIFT && shiftTimer <= SHIFT_WARN; }

    Mode mode = MODE_CLASSIC;
    State state = ST_PLAYING;
    Dir dir = RIGHT;
    int len = 0;
    uint32_t score = 0;
    int combo = 1, maxCombo = 1;
    int eaten = 0, ticks = 0;
    Cell food = {0, 0};
    bool goldenActive = false;
    Cell golden = {0, 0};
    int goldenLeft = 0;
    uint32_t seedUsed = 0;
    Events ev;

    // Modos
    int dimension = 0;               // SHIFT: 0 muros, 1 portal
    int shiftTimer = SHIFT_PERIOD;
    int timeLeftMs = TIME_START_MS;  // TIME
    int rockCount = 0;
    bool timeUp = false;

    // Fiebre y mejoras
    int frenzyLeft = 0, streak = 0;
    int shield = 0, slowMs = 0, comboBonus = 0, goldLevel = 0, scorePct = 0, coinPct = 0;
    bool upgradePending = false;
    uint8_t offer[3] = {0, 0, 0};

    // Estadísticas de la partida (logros)
    int frenzyCount = 0, goldenEaten = 0, upgradesTaken = 0, shieldsUsed = 0, rocksSmashed = 0;

private:
    struct Tgt { int x, y; bool wall, eatF, eatG, grow, hitBody, hitRock, fatal; };
    Tgt target(Dir d) const;
    bool pickEmpty(Cell &out, bool avoidFood, bool avoidGolden);
    bool placeFood();
    void spawnGolden();
    bool rockOk(int x, int y, Cell hd) const;
    void spawnRock();
    void rollOffers();
    void die();

    Cell body[MAXLEN];
    uint8_t occ[BW * BH];
    uint8_t rock[BW * BH];
    int headI = 0, tailI = 0;
    Dir q[2];
    int qn = 0;
    int sinceEat = 999;
    int pendingGrow = 0;
    Rng rng;
};

}
