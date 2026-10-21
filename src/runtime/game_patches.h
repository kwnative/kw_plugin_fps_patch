#ifndef FPS_PATCH_GAME_PATCHES_H
#define FPS_PATCH_GAME_PATCHES_H

#include <kwengine/timing/common.h>
#include "config.h"
#include <kwengine/timing/game_layout.h>

BOOL game_patches_install(const GameLayout *game, const FpsConfig *config);
/* Reset only per-session fractional state; installed code remains process-wide. */
void game_patches_reset_state(void);

#endif
