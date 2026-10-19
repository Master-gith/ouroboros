# OUROBOROS: Snake Dimension (fx-CG50) — v0.1

Add-in `.g3a` en C++ con gint/fxSDK. Pantalla 396x224, tablero 36x26 celdas de 8 px.

## Compilar
1. Instala fxSDK + gint (GiteaPC): `giteapc install Lephenixnoir/fxsdk Lephenixnoir/gint`
2. En esta carpeta: `fxsdk build-cg`  → genera `Ouroboros.g3a` (en `build-cg/`).
3. Copia el `.g3a` a la calculadora por USB.
Si tu versión de `fxsdk` exige un marcador de proyecto, crea uno con `fxsdk new` y copia `src/` y el `CMakeLists.txt`.

## Probar la lógica en PC
`make -C tests` (g++, con ASan/UBSan). Cubre reglas, invariantes de la serpiente en 400 partidas simuladas, CRC32 y recuperación de guardado.

## Estado real (v0.1)
Verificado en PC: núcleo (movimiento, colisiones, comida sin solapes, combos, comida dorada, modos Classic y Torus, determinismo por semilla), serialización del perfil con CRC32, doble ranura A/B y recuperación ante escritura interrumpida.
**No verificado:** `src/main.cpp`, `src/ui/render.cpp` y `src/platform/save_io.cpp` solo pasaron una comprobación de sintaxis contra cabeceras falsas. No hay fxSDK en mi entorno, así que NO se ha compilado contra gint ni probado en calculadora. Puntos de riesgo: nombres exactos de API (`rtc_ticks`, `GINT_CALL`, `gint_osmenu`), la ruta de ficheros `/OUROA.sav` en el sistema de ficheros del SO, y el rendimiento a 30 FPS.

## Qué funciona (en teoría, pendiente de hardware)
Título animado, menú (Classic / Torus / Stats), partidas con partículas, pausa, game over, récords, XP, nivel, monedas y estadísticas persistentes.

## Falta (siguiente etapas)
Tienda y skins, más modos (Dimension Shift, Roguelike, Boss Rush, Infection…), mejoras en partida, pseudo-3D/raycasting, audio (gint no ofrece audio nativo documentado: se investigará), secretos y logros. Los bits `unlocked`/`settings` del guardado están reservados pero aún sin uso.
