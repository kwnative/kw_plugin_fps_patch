/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Game-Plugin
 */
#ifndef FPS_PATCH_RUNTIME_H
#define FPS_PATCH_RUNTIME_H

#include <kwengine/timing/common.h>

typedef enum BootstrapStatus {
    /* Stored for late logging because DllMain deliberately performs no file I/O. */
    BOOTSTRAP_NOT_ATTEMPTED = 0,
    BOOTSTRAP_INSTALLED = 1,
    BOOTSTRAP_BAD_PE = 2,
    BOOTSTRAP_BAD_PATCH_SITES = 3,
    BOOTSTRAP_WRITE_FAILED = 4
} BootstrapStatus;

BOOL runtime_attach(HMODULE self_module, u8 *game_module);
void fps_runtime_log_activation(void);
/* ABI-matched replacements for the two possible runtime-config callsites. */
int THISCALL runtime_config_thiscall_hook(void *original_this);
int runtime_config_noarg_hook(void);
int start_session_tail_hook(void);

#endif
