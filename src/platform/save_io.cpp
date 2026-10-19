// E/S de guardado en la fx-CG50 con gint (fopen directo; gint gestiona el cambio al SO).
// v0.1.3: el fichero se crea UNA sola vez y después se sobrescribe en el sitio ("r+b"),
// para no crear/borrar ficheros en cada guardado (fragmenta la memoria y provoca
// la optimización automática del SO, que parecía reiniciar la calculadora).
#include "save_io.h"
#include <stdio.h>
#include "../core/profile.h"

namespace od {

static const char *SLOT_PATH[2] = { "/OUROA.sav", "/OUROB.sav" };

bool ioRead(int slot, uint8_t *buf64) {
    FILE *f = fopen(SLOT_PATH[slot & 1], "rb");
    if (!f) return false;
    size_t n = fread(buf64, 1, SAVE_SIZE, f);
    fclose(f);
    return n == (size_t)SAVE_SIZE;
}

bool ioWrite(int slot, const uint8_t *buf64) {
    const char *path = SLOT_PATH[slot & 1];
    FILE *f = fopen(path, "r+b");          // existe: sobrescribir en el sitio
    if (!f) f = fopen(path, "wb");         // primera vez: crearlo
    if (!f) return false;
    size_t n = fwrite(buf64, 1, SAVE_SIZE, f);
    fclose(f);
    return n == (size_t)SAVE_SIZE;
}

}
