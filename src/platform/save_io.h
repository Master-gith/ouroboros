#pragma once
#include <stdint.h>
namespace od {
// Lectura/escritura de las dos ranuras de guardado (64 bytes cada una).
bool ioRead(int slot, uint8_t *buf64);
bool ioWrite(int slot, const uint8_t *buf64);
}
