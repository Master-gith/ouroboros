// E/S de guardado en la fx-CG50 con gint. SIN COMPILAR NI PROBAR EN HARDWARE.
// Ver README: la ruta de fichero debe verificarse en la calculadora real.
#include "save_io.h"
#include <gint/gint.h>
#include <stdio.h>
#include "../core/profile.h"

namespace od {

static const char *SLOT_PATH[2] = { "/OUROA.sav", "/OUROB.sav" };

struct IoJob { const char *path; uint8_t *buf; int write; int ok; };
static IoJob job;

// Se ejecuta fuera del "mundo gint" (con el SO), como exige el acceso a ficheros.
static int ioRun(int unused) {
    (void)unused;
    FILE *f = fopen(job.path, job.write ? "wb" : "rb");
    if (!f) { job.ok = 0; return 0; }
    size_t n = job.write ? fwrite(job.buf, 1, SAVE_SIZE, f) : fread(job.buf, 1, SAVE_SIZE, f);
    fclose(f);
    job.ok = (n == (size_t)SAVE_SIZE);
    return 0;
}

static bool run(int slot, uint8_t *buf, int write) {
    job.path = SLOT_PATH[slot & 1]; job.buf = buf; job.write = write; job.ok = 0;
    gint_world_switch(GINT_CALL(ioRun, 0));
    return job.ok != 0;
}

bool ioRead(int slot, uint8_t *buf64) { return run(slot, buf64, 0); }
bool ioWrite(int slot, const uint8_t *buf64) { return run(slot, (uint8_t *)buf64, 1); }

}
