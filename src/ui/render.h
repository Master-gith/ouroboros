#pragma once
#include "../core/game.h"
#include "../core/profile.h"

namespace od {

enum Mood : uint8_t { MOOD_NORMAL, MOOD_HAPPY, MOOD_DEAD, MOOD_FRENZY };

struct Particle { int16_t x, y, vx, vy; uint8_t life; uint16_t color; };
struct Ring { int16_t x, y; uint8_t r, life; uint16_t color; };
struct FText { int16_t x, y; uint8_t life; uint16_t color; char s[8]; };

struct Fx {
    Particle p[96];
    Ring rg[4];
    FText ft[6];
    Rng rng;
    int flash = 0, shake = 0, ox = 0, oy = 0;
    void clear();
    void burst(int px, int py, uint16_t color, int count, int spread = 20);
    void ring(int px, int py, uint16_t color);
    void text(int px, int py, uint16_t color, const char *s);
    void update();
};

struct View {
    const Game *g; const Fx *fx; int frame; const SaveData *s;
    Mood mood; const char *banner; int bannerLeft; bool paused; bool present;
};

constexpr int CELL = 10, BOARD_X = 0, BOARD_Y = 16;

uint16_t hueColor(int h, int v);
void drawTitle(int frame, LoadResult lr, const SaveData &s);
void drawMenu(int sel, int frame, const SaveData &s, const char *msg);
void drawGame(const View &v);
void drawUpgrade(const Game &g, int sel, int frame);
void drawOver(const Game &g, const SaveData &s, const RunResult &rr, uint32_t runAch, int achCoins, int frame);
void drawStats(const SaveData &s);
void drawShop(int tab, int sel, const SaveData &s, int frame, const char *msg);
void drawAch(const SaveData &s);

}
