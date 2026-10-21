#ifndef FPS_PATCH_CONFIG_H
#define FPS_PATCH_CONFIG_H

#include <kwengine/timing/common.h>

typedef struct FpsConfig {
    BOOL enabled;
    /* Parsing can succeed while a value is outside the supported schedules. */
    BOOL valid;
    u32 target_fps;
    BOOL logging;
    /* Opt-in low-volume timing/state observations for controlled tests. */
    BOOL diagnostics;
    /* Opt-in adaptive rate: stock30 floor, recovery up to target_fps. */
    BOOL stall_guard;
} FpsConfig;

void config_set_defaults(FpsConfig *config);
BOOL config_load(FpsConfig *config, const wchar_t *path);

#endif
