// OUROBOROS: Snake Dimension v0.2 (fx-CG50, gint).
#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/timer.h>
#include <gint/gint.h>
#include <gint/cpu.h>
#include <gint/rtc.h>
#include <stdio.h>

#include "core/game.h"
#include "core/profile.h"
#include "core/content.h"
#include "platform/save_io.h"
#include "ui/render.h"

using namespace od;

static volatile int tickCount = 0;
static int tickCb(volatile int *f) { (*f)++; return TIMER_CONTINUE; }

enum Screen { S_TITLE, S_MENU, S_PLAY, S_PAUSE, S_UPGRADE, S_OVER, S_STATS, S_SHOP, S_ACH };

struct Keys {
    bool up = false, down = false, left = false, right = false, exe = false, exit = false, menu = false;
    Dir dirs[4]; int nd = 0;
};

static void pump(Keys &k) {
    k = Keys();
    key_event_t e;
    while ((e = pollevent()).type != KEYEV_NONE) {
        if (e.type != KEYEV_DOWN) continue;
        switch (e.key) {
        case KEY_UP:    k.up = true;    if (k.nd < 4) k.dirs[k.nd++] = UP; break;
        case KEY_DOWN:  k.down = true;  if (k.nd < 4) k.dirs[k.nd++] = DOWN; break;
        case KEY_LEFT:  k.left = true;  if (k.nd < 4) k.dirs[k.nd++] = LEFT; break;
        case KEY_RIGHT: k.right = true; if (k.nd < 4) k.dirs[k.nd++] = RIGHT; break;
        case KEY_EXE:   k.exe = true; break;
        case KEY_EXIT:  k.exit = true; break;
        case KEY_MENU:  k.menu = true; break;
        default: break;
        }
    }
}

// ---- Estado grande en memoria estática: la pila de gint es pequeña.
static SaveData save;
static Game game;
static Fx fx;
static int tmr = -1;
static bool dirty = false;
static int unsaved = 0;

static void startTimer() {
    if (tmr >= 0) timer_stop(tmr);
    tickCount = 0;
    tmr = timer_configure(TIMER_ANY, 1000000 / 30, GINT_CALL(tickCb, (void *)&tickCount));
    if (tmr >= 0) timer_start(tmr);
}

// Guardar toca el sistema de ficheros del SO: se hace poco (récord, salir al menú,
// cada 5 partidas, compras) y después se reinicia el temporizador.
static void saveNow() {
    drect(40, 80, 250, 140, C_BLACK);
    drect(40, 80, 250, 81, C_RGB(31, 24, 4)); drect(40, 139, 250, 140, C_RGB(31, 24, 4));
    dtext(60, 90, C_RGB(31, 24, 4), "GUARDANDO...");
    dtext(60, 106, C_RGB(12, 15, 20), "No apagues la calculadora.");
    dtext(60, 118, C_RGB(12, 15, 20), "Puede tardar unos segundos.");
    dupdate();
    storeProfile(save, ioWrite);
    startTimer();
    dirty = false; unsaved = 0;
}

// ---- Partida
static Mood mood = MOOD_NORMAL;
static int happyT = 0, bannerLeft = 0;
static char banner[32];
static uint32_t runAch = 0;
static int achCoins = 0;
static RunResult rr;

static void setBanner(const char *s) {
    int i = 0; while (s[i] && i < 31) { banner[i] = s[i]; i++; }
    banner[i] = 0; bannerLeft = 70;
}

static void checkAch() {
    uint32_t m = checkAchievements(save, game);
    if (!m) return;
    int c = claimAchievements(save, m);
    achCoins += c; runAch |= m; dirty = true;
    for (int i = 0; i < ACH_COUNT; i++)
        if ((m >> i) & 1) {
            char b[32]; snprintf(b, sizeof b, "LOGRO: %s +%d", ACH_NAME[i], (int)ACH_REWARD[i]);
            setBanner(b);
        }
}

static void startRun(Mode m, int frame) {
    game.start(m, rtc_ticks() ^ ((uint32_t)frame * 2654435761u));
    fx.clear(); fx.rng.seed(game.seedUsed ^ 0xA5A5u);
    mood = MOOD_NORMAL; happyT = 0; bannerLeft = 0; runAch = 0; achCoins = 0; rr = RunResult();
}

static void finishRun(bool showOver) {
    rr = applyRun(save, game.mode, game.score, (uint32_t)game.eaten, game.state == ST_WON, game.coinPct);
    checkAch();
    dirty = true; unsaved++;
    (void)showOver;   // se guarda al salir al menu o cada 5 partidas, nunca al morir
}

static void onEvents() {
    const Events &e = game.ev;
    int cx = BOARD_X + e.at.x * CELL + CELL / 2, cy = BOARD_Y + e.at.y * CELL + CELL / 2;
    if (e.ate) {
        char b[8]; snprintf(b, sizeof b, "+%d", e.gained);
        uint16_t c = e.golden ? C_RGB(31, 28, 3) : (game.combo >= 3 ? C_RGB(31, 20, 4) : C_RGB(31, 10, 8));
        fx.burst(cx, cy, c, e.golden ? 26 : 12);
        fx.text(cx, cy - 8, game.combo >= 2 ? C_RGB(31, 28, 3) : C_RGB(27, 30, 31), b);
        if (e.golden || game.combo >= 4) fx.ring(cx, cy, c);
        if (game.combo >= 3) fx.shake = 3;
        happyT = 10;
        checkAch();
    }
    if (e.frenzy) { fx.ring(cx, cy, hueColor(game.ticks * 7, 31)); fx.text(cx, cy - 20, C_RGB(31, 31, 8), "FIEBRE!"); fx.flash = 6; fx.shake = 8; setBanner("MODO FIEBRE x2"); }
    if (e.shielded) { fx.burst(cx, cy, C_RGB(6, 26, 31), 24); fx.ring(game.head().x * CELL + 5, BOARD_Y + game.head().y * CELL + 5, C_RGB(6, 26, 31)); fx.text(game.head().x * CELL + 5, BOARD_Y + game.head().y * CELL - 6, C_RGB(6, 26, 31), "ESCUDO!"); fx.shake = 5; checkAch(); }
    if (e.smashed) { fx.burst(game.head().x * CELL + 5, BOARD_Y + game.head().y * CELL + 5, C_RGB(18, 18, 20), 20); fx.text(game.head().x * CELL + 5, BOARD_Y + game.head().y * CELL - 6, C_RGB(31, 24, 4), "SMASH!"); fx.shake = 5; checkAch(); }
    if (e.shifted) { fx.flash = 6; fx.shake = 6; setBanner(game.dimension ? "DIMENSION: PORTALES" : "DIMENSION: MUROS"); }
    if (e.died) {
        Cell h = game.head();
        fx.burst(h.x * CELL + 5, BOARD_Y + h.y * CELL + 5, C_RGB(31, 4, 4), 44, 26);
        fx.ring(h.x * CELL + 5, BOARD_Y + h.y * CELL + 5, C_RGB(31, 8, 6));
        fx.flash = 6; fx.shake = 12;
    }
}

static void emitTrail(int frame) {
    if (save.trail == 0 || game.state != ST_PLAYING) return;
    Cell t = game.cellFromTail(0);
    int px = BOARD_X + t.x * CELL + 5, py = BOARD_Y + t.y * CELL + 5;
    uint16_t c = save.trail == 1 ? C_RGB(31, 26, 4) : (save.trail == 2 ? C_RGB(8, 26, 31) : hueColor(frame * 9, 31));
    fx.burst(px, py, c, save.trail == 3 ? 2 : 1, 10);
}

// Código secreto del menú: ARRIBA ARRIBA ABAJO ABAJO IZQ DER IZQ DER
static const Dir KONAMI[8] = {UP, UP, DOWN, DOWN, LEFT, RIGHT, LEFT, RIGHT};

int main(void) {
    LoadResult lr = loadProfile(save, ioRead);
    startTimer();

    Screen scr = S_TITLE;
    fx.clear();
    int sel = 0, frame = 0, acc = 0, dtMs = 33;
    int upSel = 1, shopTab = 0, shopSel = 0;
    const char *menuMsg = ""; int menuMsgT = 0;
    const char *shopMsg = ""; int shopMsgT = 0;
    Dir hist[8]; int hn = 0;
    Keys k;

    for (;;) {
        pump(k);
        if (k.menu) { gint_osmenu(); }
        frame++;
        if (menuMsgT > 0 && --menuMsgT == 0) menuMsg = "";
        if (shopMsgT > 0 && --shopMsgT == 0) shopMsg = "";

        switch (scr) {
        case S_TITLE:
            if (k.exe) scr = S_MENU;
            drawTitle(frame, lr, save);
            break;

        case S_MENU: {
            for (int i = 0; i < k.nd; i++) {                       // código secreto
                if (hn < 8) hist[hn++] = k.dirs[i];
                else { for (int j = 0; j < 7; j++) hist[j] = hist[j + 1]; hist[7] = k.dirs[i]; }
                bool ok = (hn == 8);
                for (int j = 0; ok && j < 8; j++) if (hist[j] != KONAMI[j]) ok = false;
                if (ok && !((save.achievements >> ACH_SECRET) & 1)) {
                    save.unlocked |= 1u << SECRET_SKIN;
                    claimAchievements(save, 1u << ACH_SECRET);
                    menuMsg = "SECRETO! Skin GLITCH desbloqueada"; menuMsgT = 120;
                    dirty = true;
                }
            }
            if (k.up && sel > 0) sel--;
            if (k.down && sel < 7) sel++;
            if (k.exit) scr = S_TITLE;
            if (k.exe) {
                if (sel < MODE_COUNT) {
                    int lv = levelFromXp(save.xp);
                    if (lv < MODE_LEVEL[sel]) {
                        static char b[40]; snprintf(b, sizeof b, "Bloqueado: necesitas nivel %d", (int)MODE_LEVEL[sel]);
                        menuMsg = b; menuMsgT = 70;
                    } else { startRun((Mode)sel, frame); acc = 0; scr = S_PLAY; }
                } else if (sel == 5) { scr = S_SHOP; shopTab = 0; shopSel = save.skin; }
                else if (sel == 6) scr = S_ACH;
                else scr = S_STATS;
            }
            if (scr == S_MENU) drawMenu(sel, frame, save, menuMsg);
            break; }

        case S_STATS:
            if (k.exit || k.exe) scr = S_MENU;
            if (scr == S_STATS) drawStats(save);
            break;

        case S_ACH:
            if (k.exit || k.exe) scr = S_MENU;
            if (scr == S_ACH) drawAch(save);
            break;

        case S_SHOP: {
            int n = shopTab == 0 ? SKIN_COUNT : TRAIL_COUNT;
            if (k.left || k.right) { shopTab = 1 - shopTab; shopSel = shopTab == 0 ? save.skin : save.trail; }
            if (k.up && shopSel > 0) shopSel--;
            if (k.down && shopSel < n - 1) shopSel++;
            if (k.exe) {
                bool own = shopTab == 0 ? ownsSkin(save, shopSel) : ownsTrail(save, shopSel);
                if (own) {
                    if (shopTab == 0) save.skin = (uint8_t)shopSel; else save.trail = (uint8_t)shopSel;
                    shopMsg = "Equipado"; dirty = true;
                } else {
                    BuyResult br = shopTab == 0 ? buySkin(save, shopSel) : buyTrail(save, shopSel);
                    if (br == BUY_OK) {
                        if (shopTab == 0) save.skin = (uint8_t)shopSel; else save.trail = (uint8_t)shopSel;
                        shopMsg = "COMPRADO Y EQUIPADO!"; dirty = true;
                        if (popcount32(save.unlocked & 0xFFu) >= 4 && !((save.achievements >> ACH_COLLECTOR) & 1)) {
                            claimAchievements(save, 1u << ACH_COLLECTOR);
                            shopMsg = "COMPRADO! Logro COLECCIONISTA";
                        }
                    } else if (br == BUY_NO_COINS) shopMsg = "No te alcanzan las monedas";
                    else shopMsg = "Se desbloquea con un secreto...";
                }
                shopMsgT = 60;
            }
            if (k.exit) { if (dirty) saveNow(); scr = S_MENU; }
            if (scr == S_SHOP) drawShop(shopTab, shopSel, save, frame, shopMsg);
            break; }

        case S_PLAY: {
            if (k.exit) { scr = S_PAUSE; break; }
            for (int i = 0; i < k.nd; i++) game.queueDir(k.dirs[i]);
            acc += dtMs;
            int guard = 0;
            while (acc >= game.intervalMs() && game.state == ST_PLAYING && !game.upgradePending && guard++ < 4) {
                acc -= game.intervalMs();
                game.step();
                emitTrail(frame);
                onEvents();
            }
            if (acc > game.intervalMs()) acc = game.intervalMs();
            fx.update();
            if (happyT > 0) happyT--;
            if (bannerLeft > 0) bannerLeft--;
            mood = game.state != ST_PLAYING ? MOOD_DEAD : (game.frenzyLeft > 0 ? MOOD_FRENZY : (happyT > 0 ? MOOD_HAPPY : MOOD_NORMAL));
            View v = {&game, &fx, frame, &save, mood, banner, bannerLeft, false, true};
            drawGame(v);
            if (game.state != ST_PLAYING) { finishRun(true); scr = S_OVER; }
            else if (game.upgradePending) { upSel = 1; scr = S_UPGRADE; }
            break; }

        case S_UPGRADE: {
            if (k.left && upSel > 0) upSel--;
            if (k.right && upSel < 2) upSel++;
            View v = {&game, &fx, frame, &save, MOOD_NORMAL, banner, bannerLeft, false, false};
            drawGame(v);
            drawUpgrade(game, upSel, frame);
            if (k.exe) {
                game.chooseUpgrade(upSel);
                fx.ring(BOARD_X + game.head().x * CELL + 5, BOARD_Y + game.head().y * CELL + 5, C_RGB(20, 12, 31));
                fx.burst(BOARD_X + game.head().x * CELL + 5, BOARD_Y + game.head().y * CELL + 5, C_RGB(20, 12, 31), 20);
                checkAch();
                acc = 0; scr = S_PLAY;
            }
            break; }

        case S_PAUSE: {
            View v = {&game, &fx, frame, &save, MOOD_NORMAL, banner, 0, true, true};
            if (k.exe) scr = S_PLAY;
            if (k.exit) {
                if (game.eaten > 0) { finishRun(false); saveNow(); }
                scr = S_MENU;
            }
            if (scr == S_PAUSE) drawGame(v);
            break; }

        case S_OVER: {
            fx.update();
            View v = {&game, &fx, frame, &save, MOOD_DEAD, banner, 0, false, false};
            drawGame(v);
            drawOver(game, save, rr, runAch, achCoins, frame);
            if (k.exe) {
                if (unsaved >= 5) saveNow();
                startRun(game.mode, frame); acc = 0; scr = S_PLAY;
            }
            if (k.exit) { if (dirty) saveNow(); scr = S_MENU; }
            break; }
        }

        // Ritmo de fotogramas con tope (nunca esperar para siempre).
        if (tmr >= 0) {
            int guard = 0;
            while (tickCount <= 0 && guard++ < 2000) sleep();
            if (tickCount <= 0) startTimer();
            int n = tickCount; tickCount = 0;
            dtMs = 33 * (n < 1 ? 1 : (n > 3 ? 3 : n));
        }
    }
    return 1;
}
