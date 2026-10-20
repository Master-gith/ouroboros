#pragma once
#include <stdint.h>
namespace od {
// Slots 0,1: ficheros v3 (96 bytes, lectura/escritura). Slots 2,3: ficheros antiguos v0.1/v0.2 (solo lectura).
int  ioRead(int slot, uint8_t *buf, int maxlen);       // bytes leidos (0 si no existe)
bool ioWrite(int slot, const uint8_t *buf, int len);   // solo slots 0 y 1
}
