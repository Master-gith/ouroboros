// OUROBOROS: Snake Dimension v0.3 (fx-CG50, gint).
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

// ---- Estado grande en memoria estatica: la pila de gint es pequena.
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

// Guardar toca el sistema de ficheros del SO: se hace poco (salir al menu, tienda,
// cada 5 partidas) y despues se reinicia el temporizador.
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

static void announce(uint32_t m) {
    for (int i = 0; i < ACH_COUNT; i++)
        if ((m >> i) & 1) {
            char b[32]; snprintf(b, sizeof b, "LOGRO: %s +%d", ACH_NAME[i], (int)ACH_REWARD[i]);
            setBanner(b);
        }
}
static void checkAch() {
    uint32_t m = checkAchievements(save, game) | checkMeta(save);
    if (!m) return;
    achCoins += claimAchievements(save, m); runAch |= m; dirty = true;
    announce(m);
}

static void startRun(Mode m, int frame) {
    game.start(m, rtc_ticks() ^ ((uint32_t)frame * 2654435761u));
    fx.clear(); fx.rng.seed(game.seedUsed ^ 0xA5A5u);
    mood = MOOD_NORMAL; happyT = 0; bannerLeft = 0; runAch = 0; achCoins = 0; rr = RunResult();
}

static void finishRun() {
    rr = applyRun(save, game.mode, game.score, (uint32_t)game.eaten, game.state == ST_WON, game.coinPct, game.bossesDefeated);
    save.eventsTotal += (uint32_t)game.eventsSurvived;
    if (game.mode == MODE_BOSS && (uint32_t)(game.stage - 1) > save.bestStage) save.bestStage = (uint32_t)(game.stage - 1);
    checkAch();
    dirty = true; unsaved++;       // se guarda al salir al menu o cada 5 partidas, nunca al morir
}

// Posicion en pantalla de un efecto en el tablero (en 3D la cabeza siempre esta en el mismo sitio).
static void screenPos(Cell c, int &px, int &py) {
    if (game.mode == MODE_3D) { px = HEAD3D_X; py = HEAD3D_Y; }
    else { px = BOARD_X + c.x * CELL + CELL / 2; py = BOARD_Y + c.y * CELL + CELL / 2; }
}

static void eatEffect(int cx, int cy, uint16_t c, bool golden) {
    switch (save.eatfx) {
    case 1: fx.burst(cx, cy, c, 36, 34); fx.burst(cx, cy, C_RGB(31, 31, 10), 10, 20); fx.ring(cx, cy, c); fx.shake = 4; break;
    case 2: fx.ring(cx, cy, c); fx.ring(cx, cy, C_RGB(6, 26, 31)); fx.ring(cx, cy, C_RGB(31, 31, 31)); fx.burst(cx, cy, c, 6); break;
    case 3: for (int i = 0; i < 28; i++) fx.emit(cx, cy, (int)fx.rng.range(61) - 30, -(int)fx.rng.range(30), 14 + (int)fx.rng.range(12), hueColor((int)fx.rng.range(192), 31), 0); break;
    case 4: fx.burst(cx, cy, C_RGB(31, 31, 10), 14, 40, 2); fx.flash = 4; fx.shake = 6; break;
    case 5: for (int i = 0; i < 10; i++) fx.emit(cx, cy, (int)fx.rng.range(25) - 12, -6 - (int)fx.rng.range(10), 16 + (int)fx.rng.range(8), C_RGB(31, 9, 18), 1); break;
    default: fx.burst(cx, cy, c, golden ? 26 : 12); if (golden) fx.ring(cx, cy, c); break;
    }
}

static void onEvents(int frame) {
    const Events &e = game.ev;
    int cx, cy; screenPos(e.at, cx, cy);
    int hx, hy; screenPos(game.head(), hx, hy);
    if (e.ate) {
        char b[8]; snprintf(b, sizeof b, "+%d", e.gained);
        uint16_t c = e.golden ? C_RGB(31, 28, 3) : (game.combo >= 3 ? C_RGB(31, 20, 4) : C_RGB(31, 10, 8));
        eatEffect(cx, cy, c, e.golden);
        fx.text(cx, cy - 8, game.combo >= 2 ? C_RGB(31, 28, 3) : C_RGB(27, 30, 31), b);
        if (game.combo >= 4 && save.eatfx == 0) fx.ring(cx, cy, c);
        if (game.combo >= 3 && fx.shake < 3) fx.shake = 3;
        happyT = 10;
        checkAch();
    }
    if (e.special && !e.rotten) { fx.ring(cx, cy, e.frenzy ? hueColor(frame * 7, 31) : C_RGB(6, 26, 31)); fx.text(cx, cy - 16, C_RGB(6, 26, 31), "RARA!"); }
    if (e.rotten) { fx.burst(cx, cy, C_RGB(10, 14, 3), 18); fx.text(cx, cy - 8, C_RGB(10, 20, 3), "PUAJ!"); fx.shake = 4; }
    if (e.frenzy) { fx.ring(hx, hy, hueColor(game.ticks * 7, 31)); fx.text(hx, hy - 20, C_RGB(31, 31, 8), "FIEBRE!"); fx.flash = 6; fx.shake = 8; setBanner("MODO FIEBRE x2"); }
    if (e.shielded) { fx.burst(hx, hy, C_RGB(6, 26, 31), 24); fx.ring(hx, hy, C_RGB(6, 26, 31)); fx.text(hx, hy - 14, C_RGB(6, 26, 31), "ESCUDO!"); fx.shake = 5; checkAch(); }
    if (e.smashed) { fx.burst(hx, hy, C_RGB(18, 18, 20), 20); fx.text(hx, hy - 14, C_RGB(31, 24, 4), "SMASH!"); fx.shake = 5; checkAch(); }
    if (e.shifted) { fx.flash = 6; fx.shake = 6; setBanner(game.dimension ? "DIMENSION: PORTALES" : "DIMENSION: MUROS"); }
    if (e.eventWarn) { setBanner(EVENT_HINT[game.evId]); fx.flash = 3; }
    if (e.eventStart) { fx.flash = 5; fx.shake = 6; }
    if (e.eventEnd) { fx.text(145, BOARD_Y + 60, C_RGB(31, 28, 3), "+50"); setBanner("EVENTO SUPERADO!"); checkAch(); }
    if (e.bossSpawn) {
        char b[32]; snprintf(b, sizeof b, "JEFE: %s", BOSS_NAME[game.boss.type]);
        setBanner(b); fx.flash = 6; fx.shake = 8;
        fx.ring(game.boss.x * CELL + 15, BOARD_Y + game.boss.y * CELL + 15, C_RGB(31, 8, 6));
    }
    if (e.bossHit) {
        int bx = game.boss.x * CELL + 15, by = BOARD_Y + game.boss.y * CELL + 15;
        fx.burst(bx, by, C_RGB(31, 24, 4), 10); fx.text(bx, by - 18, C_RGB(31, 8, 6), "GOLPE!"); fx.shake = 3;
    }
    if (e.bossDown) {
        int bx = game.boss.x * CELL + 15, by = BOARD_Y + game.boss.y * CELL + 15;
        fx.burst(bx, by, C_RGB(31, 20, 4), 50, 38); fx.burst(bx, by, C_WHITE, 20, 28, 2);
        fx.ring(bx, by, C_WHITE); fx.ring(bx, by, C_RGB(31, 20, 4)); fx.flash = 8; fx.shake = 14;
        setBanner("JEFE DERROTADO!"); checkAch();
    }
    if (e.died) {
        fx.burst(hx, hy, C_RGB(31, 4, 4), 44, 26);
        fx.ring(hx, hy, C_RGB(31, 8, 6));
        fx.flash = 6; fx.shake = 12;
    }
}

// Estelas (al avanzar) y particulas ambientales propias de cada skin (cada fotograma).
static void emitTrail(int frame) {
    if (save.trail == 0 || game.state != ST_PLAYING || game.mode == MODE_3D) return;
    Cell t = game.cellFromTail(0);
    int px = BOARD_X + t.x * CELL + 5, py = BOARD_Y + t.y * CELL + 5;
    uint16_t c; uint8_t kind; trailParams(save.trail, frame, c, kind);
    int vy = (kind == 1 || kind == 3) ? -8 : 0;
    fx.emit(px, py, (int)fx.rng.range(9) - 4, vy, 14, c, kind);
    if (save.trail == 3 || save.trail == 7) fx.emit(px, py, (int)fx.rng.range(9) - 4, -3, 12, c, kind);
}
static void emitAmbient(int frame) {
    if (game.mode == MODE_3D || game.state != ST_PLAYING || (frame & 1)) return;
    int s = save.skin;
    if (s != 2 && s != 3 && s != 4 && s != 5 && s != 8 && s != 11) return;
    Cell c = game.cellFromTail((int)fx.rng.range((uint32_t)game.len));
    int px = BOARD_X + c.x * CELL + 3 + (int)fx.rng.range(5), py = BOARD_Y + c.y * CELL + 2;
    switch (s) {
    case 2:  fx.emit(px, py, (int)fx.rng.range(7) - 3, -14, 12, ((frame / 2) & 1) ? C_RGB(31, 20, 3) : C_RGB(31, 7, 2), 0); break;
    case 3:  fx.emit(px, py, (int)fx.rng.range(9) - 4, 7, 14, C_WHITE, 0); break;
    case 4:  fx.emit(px, py, 0, 0, 8, C_RGB(31, 28, 4), 2); break;
    case 5:  fx.emit(px, py, (int)fx.rng.range(9) - 4, (int)fx.rng.range(9) - 4, 10, C_RGB(22, 12, 31), 2); break;
    case 8:  fx.emit(px, py, (int)fx.rng.range(5) - 2, -9, 16, C_RGB(18, 22, 31), 3); break;
    default: fx.emit(px, py, (int)fx.rng.range(29) - 14, (int)fx.rng.range(29) - 14, 5, C_RGB(31, 31, 10), 0); break;
    }
}

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
            for (int i = 0; i < k.nd; i++) {
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
            if (k.down && sel < MODE_COUNT + 2) sel++;
            if (k.exit) scr = S_TITLE;
            if (k.exe) {
                if (sel < MODE_COUNT) {
                    int lv = levelFromXp(save.xp);
                    if (lv < MODE_LEVEL[sel]) {
                        static char b[40]; snprintf(b, sizeof b, "Bloqueado: necesitas nivel %d", (int)MODE_LEVEL[sel]);
                        menuMsg = b; menuMsgT = 70;
                    } else { startRun((Mode)sel, frame); acc = 0; scr = S_PLAY; }
                } else if (sel == MODE_COUNT) { scr = S_SHOP; shopTab = 0; shopSel = save.skin; }
                else if (sel == MODE_COUNT + 1) scr = S_ACH;
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
            int n = shopTab == 0 ? SKIN_COUNT : (shopTab == 1 ? TRAIL_COUNT : EATFX_COUNT);
            auto equipped = [&]() { return shopTab == 0 ? (int)save.skin : (shopTab == 1 ? (int)save.trail : (int)save.eatfx); };
            if (k.left || k.right) { shopTab = (shopTab + (k.right ? 1 : 2)) % 3; shopSel = equipped(); }
            if (k.up && shopSel > 0) shopSel--;
            if (k.down && shopSel < n - 1) shopSel++;
            if (k.exe) {
                bool own = shopTab == 0 ? ownsSkin(save, shopSel) : (shopTab == 1 ? ownsTrail(save, shopSel) : ownsEatFx(save, shopSel));
                auto equip = [&]() {
                    if (shopTab == 0) save.skin = (uint8_t)shopSel; else if (shopTab == 1) save.trail = (uint8_t)shopSel; else save.eatfx = (uint8_t)shopSel;
                };
                if (own) { equip(); shopMsg = "Equipado"; dirty = true; }
                else {
                    BuyResult br = shopTab == 0 ? buySkin(save, shopSel) : (shopTab == 1 ? buyTrail(save, shopSel) : buyEatFx(save, shopSel));
                    if (br == BUY_OK) {
                        equip(); shopMsg = "COMPRADO Y EQUIPADO!"; dirty = true;
                        uint32_t m = checkMeta(save);
                        if (m) { claimAchievements(save, m); shopMsg = "COMPRADO! Logro desbloqueado"; }
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
            for (int i = 0; i < k.nd; i++) {
                if (game.relativeControls()) {               // 3D: IZQ/DER giran respecto a donde miras
                    if (k.dirs[i] == LEFT) game.queueTurn(-1);
                    else if (k.dirs[i] == RIGHT) game.queueTurn(+1);
                } else game.queueDir(k.dirs[i]);
            }
            acc += dtMs;
            int guard = 0;
            while (acc >= game.intervalMs() && game.state == ST_PLAYING && !game.upgradePending && guard++ < 4) {
                acc -= game.intervalMs();
                game.step();
                emitTrail(frame);
                onEvents(frame);
            }
            if (acc > game.intervalMs()) acc = game.intervalMs();
            if (game.evId == GEV_QUAKE && game.evWarn == 0 && game.evLeft > 0 && fx.shake < 2) fx.shake = 2;
            emitAmbient(frame);
            fx.update();
            if (happyT > 0) happyT--;
            if (bannerLeft > 0) bannerLeft--;
            mood = game.state != ST_PLAYING ? MOOD_DEAD : (game.frenzyLeft > 0 ? MOOD_FRENZY : (happyT > 0 ? MOOD_HAPPY : MOOD_NORMAL));
            View v = {&game, &fx, frame, &save, mood, banner, bannerLeft, false, true};
            drawGame(v);
            if (game.state != ST_PLAYING) { finishRun(); scr = S_OVER; }
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
                int hx, hy; screenPos(game.head(), hx, hy);
                fx.ring(hx, hy, C_RGB(20, 12, 31)); fx.burst(hx, hy, C_RGB(20, 12, 31), 20);
                checkAch();
                acc = 0; scr = S_PLAY;
            }
            break; }

        case S_PAUSE: {
            View v = {&game, &fx, frame, &save, MOOD_NORMAL, banner, 0, true, true};
            if (k.exe) scr = S_PLAY;
            if (k.exit) {
                if (game.eaten > 0) { finishRun(); saveNow(); }
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
