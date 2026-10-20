// E/S de guardado en la fx-CG50 con gint (fopen directo; gint gestiona el cambio al SO).
// El fichero se crea UNA sola vez y despues se sobrescribe en el sitio ("r+b"), para no
// crear/borrar ficheros en cada guardado (provoca la limpieza de memoria del SO).
#include "save_io.h"
#include <stdio.h>

namespace od {

static const char *PATH[4] = { "/OUA3.sav", "/OUB3.sav", "/OUROA.sav", "/OUROB.sav" };

int ioRead(int slot, uint8_t *buf, int maxlen) {
    FILE *f = fopen(PATH[slot & 3], "rb");
    if (!f) return 0;
    size_t n = fread(buf, 1, (size_t)maxlen, f);
    fclose(f);
    return (int)n;
}

bool ioWrite(int slot, const uint8_t *buf, int len) {
    if (slot < 0 || slot > 1) return false;
    const char *path = PATH[slot];
    FILE *f = fopen(path, "r+b");          // existe: sobrescribir en el sitio
    if (!f) f = fopen(path, "wb");         // primera vez: crearlo
    if (!f) return false;
    size_t n = fwrite(buf, 1, (size_t)len, f);
    fclose(f);
    return n == (size_t)len;
}

}
