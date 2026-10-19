#pragma once
// Núcleo del juego: lógica pura, SIN dependencias de gint (se prueba en PC).
#include <stdint.h>
#include "rng.h"

namespace od {

constexpr int BW = 36;            // columnas del tablero
constexpr int BH = 26;            // filas del tablero
constexpr int MAXLEN = BW * BH;
constexpr int COMBO_WINDOW = 28;  // ticks para encadenar combo
constexpr int COMBO_MAX = 5;

enum Dir : uint8_t { UP = 0, RIGHT = 1, DOWN = 2, LEFT = 3 };
enum Mode : uint8_t { MODE_CLASSIC = 0, MODE_TORUS = 1, MODE_COUNT = 2 };
enum State : uint8_t { ST_PLAYING = 0, ST_DEAD = 1, ST_WON = 2 };

struct Cell { int8_t x, y; };

// Eventos del último tick (para efectos visuales/sonoros en la capa de plataforma).
struct Events {
    bool ate = false, golden = false, died = false, won = false;
    Cell at = {0, 0};
};

class Game {
public:
    void start(Mode m, uint32_t seed);
    void queueDir(Dir d);            // buffer de 2 giros; ignora inversiones
    void step();                     // avanza un tick
    int  intervalMs() const;         // tiempo entre ticks según progreso

    // Lectura para render
    Cell cellFromTail(int i) const { return body[(tailI + i) % MAXLEN]; }
    Cell head() const { return body[headI]; }

    Mode mode = MODE_CLASSIC;
    State state = ST_PLAYING;
    Dir dir = RIGHT;
    int len = 0;
    uint32_t score = 0;
    int combo = 1;
    int eaten = 0;
    int ticks = 0;
    Cell food = {0, 0};
    bool goldenActive = false;
    Cell golden = {0, 0};
    int goldenLeft = 0;
    uint32_t seedUsed = 0;
    Events ev;

    // Solo para pruebas
    bool occupied(int x, int y) const { return occ[y * BW + x] != 0; }

private:
    bool pickEmpty(Cell &out, bool avoidFood, bool avoidGolden);
    bool placeFood();
    void spawnGolden();
    void die();

    Cell body[MAXLEN];
    uint8_t occ[BW * BH];
    int headI = 0, tailI = 0;
    Dir q[2];
    int qn = 0;
    int sinceEat = 999;
    int pendingGrow = 0;
    Rng rng;
};

}
