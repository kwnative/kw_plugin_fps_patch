#include "config.h"

void config_set_defaults(FpsConfig *config)
{
    config->enabled = TRUE;
    config->valid = TRUE;
    config->target_fps = 90;
    config->logging = FALSE;
    config->diagnostics = FALSE;
    config->stall_guard = FALSE;
}

BOOL config_load(FpsConfig *config, const wchar_t *path)
{
    DWORD attributes;
    int fps;
    if (config == NULL || path == NULL)
        return FALSE;
    attributes = GetFileAttributesW(path);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return FALSE;
    }

    /* A TRUE return means the INI existed, not that every setting was valid. */
    config->enabled = GetPrivateProfileIntW(L"fps_patch", L"enabled", config->enabled, path) != 0;
    fps = (int)GetPrivateProfileIntW(L"fps_patch", L"target_fps", config->target_fps, path);
    /* Ratios 3..6 are the complete-client schedules available above 30 FPS. */
    config->valid = fps >= 45 && fps <= 90 && fps % 15 == 0;
    config->target_fps = config->valid ? (u32)fps : 0u;
    config->logging = GetPrivateProfileIntW(L"fps_patch", L"logging", config->logging, path) != 0;
    config->diagnostics = GetPrivateProfileIntW(L"fps_patch", L"diagnostics", config->diagnostics, path) != 0;
    config->stall_guard = GetPrivateProfileIntW(L"fps_patch", L"stall_guard", config->stall_guard, path) != 0;
    return TRUE;
}
