#pragma once
#include <stdint.h>
namespace od {
// xorshift32: determinista, sin estado global; permite repetir partidas por semilla.
struct Rng {
    uint32_t s = 0x9E3779B9u;
    void seed(uint32_t v) { s = v ? v : 0x9E3779B9u; }
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    uint32_t range(uint32_t n) { return next() % n; }
};
}
