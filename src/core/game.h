#pragma once
// Núcleo del juego v0.3: lógica pura, SIN dependencias de gint (se prueba en PC).
#include <stdint.h>
#include "rng.h"

namespace od {

constexpr int BW = 29;
constexpr int BH = 20;
constexpr int MAXLEN = BW * BH;
constexpr int COMBO_BASE_WINDOW = 28;
constexpr int COMBO_MAX = 5;
constexpr int UPGRADE_EVERY = 5;
constexpr int SHIFT_PERIOD = 100;
constexpr int SHIFT_WARN = 16;
constexpr int MAX_ROCKS = 16;
constexpr int FRENZY_TICKS = 70;
constexpr int TIME_START_MS = 60000;
constexpr int EVENT_WARN = 24;
constexpr int MAX_BONUS = 8;
constexpr int MAX_BULLETS = 24;

enum Dir : uint8_t { UP = 0, RIGHT = 1, DOWN = 2, LEFT = 3 };
enum Mode : uint8_t { MODE_CLASSIC = 0, MODE_TORUS, MODE_ROCKS, MODE_SHIFT, MODE_TIME, MODE_BOSS, MODE_3D, MODE_COUNT };
enum State : uint8_t { ST_PLAYING = 0, ST_DEAD = 1, ST_WON = 2 };
enum Upgrade : uint8_t { UP_SHIELD = 0, UP_SLOW, UP_COMBO, UP_GOLD, UP_SHRINK, UP_BOOST, UP_COINS, UP_DAMAGE, UP_COUNT };
enum GameEvent : uint8_t { GEV_NONE = 0, GEV_RAIN, GEV_FOG, GEV_TURBO, GEV_MIRROR, GEV_QUAKE, GEV_COUNT };
enum SpType : uint8_t { SP_ICE = 0, SP_ROTTEN, SP_RAINBOW };

struct Cell { int8_t x, y; };

struct Events {
    bool ate = false, golden = false, died = false, won = false;
    bool shielded = false, smashed = false, frenzy = false, upgrade = false;
    bool shifted = false, timeUp = false;
    bool eventWarn = false, eventStart = false, eventEnd = false;
    bool special = false, rotten = false;
    bool bossSpawn = false, bossHit = false, bossDown = false, bossShot = false;
    int gained = 0;
    Cell at = {0, 0};
};

struct Special { bool active = false; uint8_t type = 0; int left = 0; Cell c = {0, 0}; };
struct Boss {
    bool active = false;
    uint8_t type = 0;
    int x = 0, y = 0, hp = 0, maxhp = 0, cd = 0, moveCd = 0, warn = 0, dx = 1;
};
struct Bullet { int8_t x = 0, y = 0, dx = 0, dy = 0; bool alive = false; };

class Game {
public:
    void start(Mode m, uint32_t seed);
    void queueDir(Dir d);
    void queueTurn(int rel);         // control relativo (modo 3D): -1 izquierda, +1 derecha
    void step();
    int  intervalMs() const;
    void chooseUpgrade(int i);

    Cell cellFromTail(int i) const { return body[(tailI + i) % MAXLEN]; }
    Cell head() const { return body[headI]; }
    bool wrapping() const { return mode == MODE_TORUS || (mode == MODE_SHIFT && dimension == 1); }
    bool rockAt(int x, int y) const { return rock[y * BW + x] != 0; }
    bool tempRockAt(int x, int y) const { return rock[y * BW + x] == 2; }
    bool occupied(int x, int y) const { return occ[y * BW + x] != 0; }
    int  comboWindow() const { return COMBO_BASE_WINDOW + comboBonus; }
    int  comboPct() const { int r = comboWindow() - sinceEat; return r <= 0 ? 0 : r * 100 / comboWindow(); }
    bool shiftWarn() const { return mode == MODE_SHIFT && shiftTimer <= SHIFT_WARN; }
    bool mirrorOn() const { return evId == GEV_MIRROR && evWarn == 0 && evLeft > 0; }
    bool turboOn() const { return evId == GEV_TURBO && evWarn == 0 && evLeft > 0; }
    bool fogOn() const { return evId == GEV_FOG && evWarn == 0 && evLeft > 0; }
    bool bossCell(int x, int y) const { return boss.active && x >= boss.x && x < boss.x + 3 && y >= boss.y && y < boss.y + 3; }
    bool relativeControls() const { return mode == MODE_3D; }

    Mode mode = MODE_CLASSIC;
    State state = ST_PLAYING;
    Dir dir = RIGHT;
    int len = 0;
    uint32_t score = 0;
    int combo = 1, maxCombo = 1;
    int eaten = 0, ticks = 0;
    Cell food = {-1, -1};
    bool goldenActive = false;
    Cell golden = {0, 0};
    int goldenLeft = 0;
    Special sp;
    Cell bonus[MAX_BONUS];
    int bonusN = 0;
    uint32_t seedUsed = 0;
    Events ev;

    int dimension = 0, shiftTimer = SHIFT_PERIOD;
    int timeLeftMs = TIME_START_MS;
    int rockCount = 0;
    bool timeUp = false;

    int frenzyLeft = 0, streak = 0, iceLeft = 0;
    int shield = 0, slowMs = 0, comboBonus = 0, goldLevel = 0, scorePct = 0, coinPct = 0, bossDmg = 0;
    bool upgradePending = false;
    uint8_t offer[3] = {0, 0, 0};

    // Eventos aleatorios
    uint8_t evId = GEV_NONE;
    int evWarn = 0, evLeft = 0, evTimer = 200, lastEv = 0;

    // Jefes
    Boss boss;
    Bullet bullets[MAX_BULLETS];
    int stage = 1, bossRespawn = 20, bossesDefeated = 0;
    uint32_t bossKillMask = 0;

    // Estadísticas de la partida (logros)
    int frenzyCount = 0, goldenEaten = 0, upgradesTaken = 0, shieldsUsed = 0, rocksSmashed = 0;
    int eventsSurvived = 0, specialEaten = 0;

private:
    struct Tgt { int x, y; bool wall, eatF, eatG, eatS, grow, hitBody, hitRock, hitBoss, fatal; int bonusIdx; };
    Tgt target(Dir d) const;
    bool taken(int x, int y) const;
    bool pickEmpty(Cell &out);
    bool placeFood();
    void spawnGolden();
    void spawnSpecial();
    bool rockOk(int x, int y, Cell hd) const;
    void spawnRock(uint8_t kind);
    void rollOffers();
    void eventsStep();
    void bossStep();
    bool rectFree(int x, int y) const;
    void spawnBoss();
    void defeatBoss();
    void fire(int bx, int by, int dx, int dy);
    void shrinkTail(int n);
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
