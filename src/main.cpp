// OUROBOROS: Snake Dimension (fx-CG50, gint). SIN COMPILAR NI PROBAR EN HARDWARE.
#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/timer.h>
#include <gint/gint.h>
#include <gint/cpu.h>
#include <gint/rtc.h>

#include "core/game.h"
#include "core/profile.h"
#include "platform/save_io.h"
#include "ui/render.h"

using namespace od;

static volatile int tickFlag = 0;
static int tickCb(volatile int *f) { *f = 1; return TIMER_CONTINUE; }

enum Screen { S_TITLE, S_MENU, S_PLAY, S_PAUSE, S_OVER, S_STATS };

struct Keys {
    bool up = false, down = false, exe = false, exit = false, menu = false;
    Dir dirs[4]; int nd = 0;
};

static void pump(Keys &k) {
    k = Keys();
    key_event_t e;
    while ((e = pollevent()).type != KEYEV_NONE) {
        if (e.type != KEYEV_DOWN) continue;
        switch (e.key) {
        case KEY_UP:    k.up = true;   if (k.nd < 4) k.dirs[k.nd++] = UP; break;
        case KEY_DOWN:  k.down = true; if (k.nd < 4) k.dirs[k.nd++] = DOWN; break;
        case KEY_LEFT:  if (k.nd < 4) k.dirs[k.nd++] = LEFT; break;
        case KEY_RIGHT: if (k.nd < 4) k.dirs[k.nd++] = RIGHT; break;
        case KEY_EXE:   k.exe = true; break;
        case KEY_EXIT:  k.exit = true; break;
        case KEY_MENU:  k.menu = true; break;
        default: break;
        }
    }
}

// Estado grande en memoria estática: la pila de gint es pequeña (~4 KB de Game+Fx
// en la pila provocaban "stack overflow during world switch" al usar ficheros).
static SaveData save;
static int tmr = -1;
static void startTimer() {
    if (tmr >= 0) timer_stop(tmr);
    tickFlag = 0;
    tmr = timer_configure(TIMER_ANY, 1000000 / 30, GINT_CALL(tickCb, (void *)&tickFlag));
    if (tmr >= 0) timer_start(tmr);
}

// Guardar usa el sistema de ficheros del SO; después se reinicia el temporizador
// por si el cambio de mundo lo dejó parado (causaría un cuelgue esperando el frame).
static void doSave(const SaveData &) { storeProfile(save, ioWrite); startTimer(); }

// Guardar con poca frecuencia: solo con récord nuevo, al salir al menú, o cada 5 partidas.
static bool dirty = false;
static int unsaved = 0;
static void saveNow() {
    dtext(80, 168, C_RGB(31, 24, 4), "Guardando...");
    dupdate();
    doSave(save);
    dirty = false; unsaved = 0;
}

static Game game;
static Fx fx;

int main(void) {
    LoadResult lr = loadProfile(save, ioRead);

    startTimer();

    Screen scr = S_TITLE;
    fx.clear();
    int sel = 0, frame = 0, acc = 0;
    bool record = false, runApplied = false, pendingSave = false;
    Keys k;

    for (;;) {
        pump(k);
        if (k.menu) { gint_osmenu(); }
        frame++;

        switch (scr) {
        case S_TITLE:
            if (k.exe) scr = S_MENU;
            drawTitle(frame, lr);
            break;
        case S_MENU:
            if (k.up && sel > 0) sel--;
            if (k.down && sel < 2) sel++;
            if (k.exit) scr = S_TITLE;
            if (k.exe) {
                if (sel == 2) scr = S_STATS;
                else {
                    game.start((Mode)sel, rtc_ticks() ^ ((uint32_t)frame * 2654435761u));
                    fx.clear(); fx.rng.seed(game.seedUsed ^ 0xA5A5u);
                    acc = 0; record = false; runApplied = false; scr = S_PLAY;
                }
            }
            if (scr == S_MENU) drawMenu(sel, frame, save);
            break;
        case S_STATS:
            if (k.exit || k.exe) scr = S_MENU;
            if (scr == S_STATS) drawStats(save);
            break;
        case S_PLAY: {
            if (k.exit) { scr = S_PAUSE; break; }
            for (int i = 0; i < k.nd; i++) game.queueDir(k.dirs[i]);
            acc += 33;
            while (acc >= game.intervalMs() && game.state == ST_PLAYING) {
                acc -= game.intervalMs();
                game.step();
                if (game.ev.ate) {
                    fx.burst(game.ev.at.x * CELL + CELL / 2, BOARD_Y + game.ev.at.y * CELL + CELL / 2,
                             game.ev.golden ? C_RGB(31, 28, 3) : C_RGB(31, 10, 8), game.ev.golden ? 24 : 12);
                }
                if (game.ev.died) {
                    Cell h = game.head();
                    fx.burst(h.x * CELL + CELL / 2, BOARD_Y + h.y * CELL + CELL / 2, C_RGB(31, 4, 4), 40);
                    fx.flash = 6;
                }
            }
            fx.update();
            drawGame(game, fx, frame, save, false);
            if (game.state != ST_PLAYING) {
                record = applyRun(save, game.mode, game.score, (uint32_t)game.eaten, game.state == ST_WON);
                runApplied = true;
                dirty = true; unsaved++;
                pendingSave = record;          // solo se guarda al instante con récord nuevo
                scr = S_OVER;
            }
            break;
        }
        case S_PAUSE:
            if (k.exe) scr = S_PLAY;
            if (k.exit) {
                if (!runApplied && game.eaten > 0) {
                    applyRun(save, game.mode, game.score, (uint32_t)game.eaten, false);
                    saveNow();
                }
                runApplied = true; scr = S_MENU;
            }
            if (scr == S_PAUSE) drawGame(game, fx, frame, save, true);
            break;
        case S_OVER:
            fx.update();
            drawGame(game, fx, frame, save, false);
            drawOver(game, save, record);
            if (pendingSave) {
                pendingSave = false;
                saveNow();
            }
            if (k.exe) {
                if (unsaved >= 5) saveNow();
                game.start(game.mode, rtc_ticks() ^ ((uint32_t)frame * 2654435761u));
                fx.clear(); acc = 0; record = false; runApplied = false; scr = S_PLAY;
            }
            if (k.exit) { if (dirty) saveNow(); scr = S_MENU; }
            break;
        }

        if (tmr >= 0) {
            int guard = 0;
            while (!tickFlag && guard++ < 2000) sleep();   // nunca esperar para siempre
            if (!tickFlag) startTimer();                    // el temporizador se perdió: reiniciarlo
            tickFlag = 0;
        }
    }
    return 1;
}
