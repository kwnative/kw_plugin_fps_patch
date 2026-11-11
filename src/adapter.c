/* Plugin-owned FPS runtime, imported without the standalone DllMain or proxy.
 * The original runtime keeps ownership of stock 30-Hz initialization timing.
 * Only explicit prepare/activate callbacks are visible to the framework. */
#include <kwf/plugin.h>
#include <kwf/patch_query.h>
#if WITH_PACING_INTEGRATION
#include <kwpacing/integration/pacing_policy.h>
#endif
#include "runtime/config.h"
#include <kwengine/timing/game_layout.h>
#include <stdio.h>
#include <stddef.h>
static const struct kwf_context *fps_context;
static uint32_t startup_wait_owned_elsewhere;
static GamePacingWait startup_wait;
static FpsConfig startup_config;
static BOOL startup_ini_found;
static BOOL snapshot_load(FpsConfig *config, const wchar_t *path)
{
    (void)path;
    *config = startup_config;
    return startup_ini_found;
}
#define FPS_PATCH_INI_NAME L"kw_fps_patch.ini"
#define FPS_PATCH_INI_LABEL "kw_fps_patch.ini"
#define FPS_PATCH_LOG_NAME L"kw_fps_patch.log"
#define config_load snapshot_load
#include "runtime/runtime.c"
#undef config_load
#include "layout_reservations.h"
static uint32_t KWF_CALL reserve_fps_range(void *owner, uintptr_t address, uint32_t size)
{
    if (startup_wait_owned_elsewhere) {
        uintptr_t sleep_call = (uintptr_t)fps_runtime.game.module + startup_wait.instruction;
        if (address <= UINTPTR_MAX - size && address < sleep_call + 6u && sleep_call < address + size) {
            fps_context->host->log(owner, "FPS range overlaps separately owned native Sleep call");
            return KWF_CONFLICT;
        }
    }
    return fps_context->host->reserve(owner, address, size);
}
static uint32_t KWF_CALL fps_prepare(const struct kwf_context *context)
{
    wchar_t path[MAX_PATH];
    char diagnostic[192];
    uint32_t negotiated_pacing = 0;
    const struct kwf_patch_query *patches;
    if (context->role != KWF_ROLE_RENDERED)
        return KWF_UNSUPPORTED;
    fps_context = context;
    patches = context->host->service(context->owner, KWF_PATCH_QUERY_SERVICE, 1);
    if (!patches || patches->size < offsetof(struct kwf_patch_query, find) + sizeof(patches->find) || patches->version != 1 || !patches->find)
        return KWF_UNSUPPORTED;
    startup_wait_owned_elsewhere = 0;
#if WITH_PACING_INTEGRATION
    const struct kwpacing_policy *pacing = context->host->service(context->owner, KWPACING_POLICY_SERVICE, 1);
    if (pacing) {
        if (pacing->size < sizeof(*pacing) || pacing->mode > 2 || !pacing->engine_prepared || (negotiated_pacing && pacing->mode))
            return KWF_CONFLICT;
        if (pacing->mode)
            negotiated_pacing = pacing->mode;
    }
#endif
    config_set_defaults(&startup_config);
    if (!GetModuleFileNameW((HMODULE)context->plugin_module, path, MAX_PATH) || !path_replace_filename(path, MAX_PATH, L"kw_fps_patch.ini"))
        return KWF_ERROR;
    startup_ini_found = config_load(&startup_config, path);
    if (!startup_config.enabled || !startup_config.valid) {
        context->host->log(context->owner, "FPS INI disabled or invalid; no hooks installed");
        return KWF_UNSUPPORTED;
    }
    fps_runtime.self_module = (HMODULE)context->plugin_module;
    if (resolve_game_layout_ex(&fps_runtime.game, (u8 *)context->game_image, &fps_runtime.discovery) != GAME_RESOLVED) {
        snprintf(diagnostic, sizeof(diagnostic), "FPS discovery rejected at %s; no offset fallback",
                 game_resolve_stage_name(fps_runtime.discovery.stage));
        context->host->log(context->owner, diagnostic);
        return KWF_UNSUPPORTED;
    }
    context->host->log(context->owner, fps_runtime.game.keyboard.repeat_entry_kind == KEYBOARD_REPEAT_PROTECTED_ENTRY
                                           ? "keyboard resolved: validated protected native entry; original forwarding retained"
                                           : "keyboard resolved: validated native body");
    snprintf(diagnostic, sizeof(diagnostic), "resolved FPS bootstrap config RVA=%08lx session RVA=%08lx; waiting for stock runtime configuration",
             (unsigned long)fps_runtime.game.bootstrap.runtime_config_call.instruction,
             (unsigned long)fps_runtime.game.bootstrap.start_session_tail.instruction);
    context->host->log(context->owner, diagnostic);
    if (!resolve_game_pacing_wait(&fps_runtime.game, &startup_wait))
        return KWF_UNSUPPORTED;
    struct kwf_patch_info owner = {0};
    owner.size = sizeof(owner);
    uint32_t owned = patches->find(context->owner, (uintptr_t)fps_runtime.game.module + startup_wait.instruction, 6, &owner);
    if (owned != KWF_OK && owned != KWF_UNSUPPORTED)
        return KWF_CONFLICT;
    if (negotiated_pacing && owned != KWF_OK)
        return KWF_CONFLICT;
    startup_wait_owned_elsewhere = owned == KWF_OK;
    if (startup_wait_owned_elsewhere) {
        if (owner.address != (uintptr_t)fps_runtime.game.module + startup_wait.instruction || owner.length != 6 || !owner.provider_state)
            return KWF_CONFLICT;
        snprintf(diagnostic, sizeof(diagnostic), "native wait separately reserved by %s; FPS excludes those bytes", owner.provider_id);
        context->host->log(context->owner, diagnostic);
    }
    uint32_t result = kw_fps_reserve_layout(&fps_runtime.game, reserve_fps_range, context->owner);
    if (result != KWF_OK || !startup_config.stall_guard)
        return result;
    return kw_fps_reserve_stall_layout(&fps_runtime.game, reserve_fps_range, context->owner);
}
static uint32_t KWF_CALL fps_activate(void)
{
    /* No live FPS fields are written here. The original runtime-config wrapper
     * calls native initialization first, then changes the presentation rate. */
    if (!install_bootstrap_hooks(&fps_runtime))
        return KWF_ERROR;
    fps_runtime_log_activation();
    fps_context->host->log(fps_context->owner, "bootstrap armed; kw_fps_patch.log records actual runtime/session callback execution");
    return KWF_OK;
}
KWF_EXPORT const struct kwf_plugin *KWF_CALL KwPluginQuery(void)
{
    static const struct kwf_plugin plugin = {sizeof(plugin),  KWF_ABI_MAJOR, KWF_ABI_MINOR, KWF_ROLE_RENDERED,
                                             KWF_CAP_PATCHES, "fps_patch",   fps_prepare,   fps_activate};
    return &plugin;
}
/* Adapter-only access for the brokered implementation of upstream's writer. */
const struct kwf_context *kw_fps_context(void)
{
    return fps_context;
}
