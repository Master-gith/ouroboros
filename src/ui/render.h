#pragma once
#include "../core/game.h"
#include "../core/profile.h"

namespace od {

struct Particle { int16_t x, y, vx, vy; uint8_t life; uint16_t color; };

struct Fx {
    Particle p[64];
    Rng rng;
    int flash = 0;                  // destello de pantalla (frames restantes)
    void clear();
    void burst(int px, int py, uint16_t color, int count);
    void update();
};

void drawTitle(int frame, LoadResult lr);
void drawMenu(int sel, int frame, const SaveData &s);
void drawGame(const Game &g, const Fx &fx, int frame, const SaveData &s, bool paused);
void drawOver(const Game &g, const SaveData &s, bool record);
void drawStats(const SaveData &s);

constexpr int CELL = 8, BOARD_X = 0, BOARD_Y = 16;

}
