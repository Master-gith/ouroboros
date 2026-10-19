// E/S de guardado en la fx-CG50 con gint.
// v0.1.1: SIN gint_world_switch manual. El crash "stack overflow during world switch"
// apuntaba a un cambio de mundo anidado; se usa fopen directo (gint/fxlibc gestiona el SO).
#include "save_io.h"
#include <stdio.h>
#include "../core/profile.h"

namespace od {

static const char *SLOT_PATH[2] = { "/OUROA.sav", "/OUROB.sav" };

static bool run(int slot, uint8_t *buf, bool write) {
    FILE *f = fopen(SLOT_PATH[slot & 1], write ? "wb" : "rb");
    if (!f) return false;
    size_t n = write ? fwrite(buf, 1, SAVE_SIZE, f) : fread(buf, 1, SAVE_SIZE, f);
    fclose(f);
    return n == (size_t)SAVE_SIZE;
}

bool ioRead(int slot, uint8_t *buf64) { return run(slot, buf64, false); }
bool ioWrite(int slot, const uint8_t *buf64) { return run(slot, (uint8_t *)buf64, true); }

}
